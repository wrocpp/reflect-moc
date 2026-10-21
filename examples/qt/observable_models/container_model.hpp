// The models of an observable container, written once. The begin and end calls of QAbstractItemModel are paired
// here, driven by the container's typed signals, so a new data type needs formatters and nothing else.
//
//   ContainerModel<C>       a QAbstractListModel: one column
//   ContainerTableModel<C>  a QAbstractTableModel: one column per Column given
//
// Where the row comes from is the row index's business (row_index.hpp): the key for a list, a sorted copy of the
// keys for a map, insertion slots for a hash or set. Editing goes through the container: setData builds the new
// value with the column's editor and calls replace, so the change is announced like any other and a refused
// edit changes nothing.
//
// A change to many entries reaches a view as the fewest begin and end pairs: a batch is split into runs of
// consecutive rows, and each run is one pair, so a view keeps its selection and scroll position where a reset
// would drop them. A reset of the container is a reset of the model, and nothing else.
//
// The model counts its own rows, because while a batch is being announced the container already holds the final
// content and the two counts differ.
//
// The model holds its container by QPointer. When the container is destroyed first the model resets to empty from the
// container's destroyed signal, so every call (rowCount, data, index, setData, flags, headerData) stays safe, and a
// model that is destroyed first needs nothing. A model made while the container is announcing a change, between the
// "about" and the "done" signal, did not hear the "about" one: at the "done" signal it resets to the container's
// content instead of ending a change it never began.
//
// A model and its container live in one thread, so the connections are direct; a model made for a container of
// another thread asserts in a debug build.
#pragma once

#include "observable_container.hpp"
#include "row_index.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QAbstractListModel>
#include <QtCore/QAbstractTableModel>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QThread>

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace observable {

// What one column shows, and how an edit of it becomes a new value (none: the column is read only).
template <class C>
struct Column {
  using Traits = ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  using Display = std::function<QVariant(const Key&, const Value&)>;
  using Editor = std::function<std::optional<Value>(const Value& current, const QVariant& edited)>;

  QString header;
  Display display;
  Editor edit{};
};

template <class C, class Model>
class ContainerModelBase : public Model {
 public:
  using Source = ObservableContainer<C>;
  using Traits = ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;

  static constexpr int KeyRole = Qt::UserRole;
  // A list entry has no identity but its row, and an entry of a multi container only its key and occurrence, both
  // of which shift under an insert or remove without a signal, so a role that answered with them would go stale
  // in a sorting proxy. Lists and multi containers do not offer the role.
  static constexpr bool hasKeyRole = Traits::order == Order::sorted || Traits::order == Order::unordered;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : m_rowCount;
  }

  QHash<int, QByteArray> roleNames() const override {
    auto names = Model::roleNames();
    if (hasKeyRole) names.insert(KeyRole, "key");
    return names;
  }

  Qt::ItemFlags flags(const QModelIndex& index) const override {
    auto flags = Model::flags(index);
    if (index.isValid() && editable(index.column())) flags |= Qt::ItemIsEditable;
    return flags;
  }

  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override {
    if (!m_source || !index.isValid() || index.row() >= rowCount() || role != Qt::EditRole || !editable(index.column())) return false;
    const Key key = keyAt(index.row());
    const std::optional<Value> edited = editorOf(index.column())(m_source->value(key), value);
    return edited && m_source->replace(key, *edited);
  }

 protected:
  ContainerModelBase(Source* source, QObject* parent) : Model(parent), m_source(source) {
    Q_ASSERT_X(source->thread() == this->thread(), "observable::ContainerModelBase",
               "a model must live in the thread of its container (move data between threads, not the model)");
    m_index.assign(Traits::keys(source->items()));
    m_rowCount = static_cast<int>(source->size());
    connectSource();
  }

  using Editor = typename Column<C>::Editor;
  virtual bool editable(int column) const = 0;
  virtual const Editor& editorOf(int column) const = 0;

  Key keyAt(qsizetype row) const { return m_index.keyAt(row); }
  Source* source() const { return m_source.data(); }

  // The value of the entry in a row, or nullptr for a row whose entry the container has already dropped, which a
  // row inside a batch removal still is while its begin and end are being announced.
  const Value* valueAt(const Key& key) const { return m_source && m_source->contains(key) ? &m_source->value(key) : nullptr; }

 private:
  qsizetype rowOf(const Key& key) const { return m_index.rowOf(key); }
  qsizetype rowToInsert(const Key& key) const { return m_index.rowToInsert(key); }

  // Takes the container's content in one reset: for a container that is gone, the empty content.
  void resync() {
    this->beginResetModel();
    takeContent();
    this->endResetModel();
  }

  void takeContent() {
    m_open = false;
    if (m_source) {
      m_index.assign(Traits::keys(m_source->items()));
      m_rowCount = static_cast<int>(m_source->size());
    } else {
      m_index.clear();
      m_rowCount = 0;
    }
  }

  // The end of a change whose beginning this model heard. A model made between the two signals did not: it follows
  // the container with a reset, and the caller has nothing left to do.
  bool heardTheBeginning() {
    if (m_open) {
      m_open = false;
      return true;
    }
    resync();
    return false;
  }

  void connectSource() {
    QObject::connect(m_source, &QObject::destroyed, this, [this] { resync(); });

    QObject::connect(m_source, &Source::entryAboutToBeAdded, this, [this](const Key& key) {
      m_pendingRow = static_cast<int>(rowToInsert(key));
      m_open = true;
      this->beginInsertRows(QModelIndex(), m_pendingRow, m_pendingRow);
    });
    QObject::connect(m_source, &Source::entryAdded, this, [this](const Key& key) {
      if (!heardTheBeginning()) return;
      m_index.insert(key, m_pendingRow);
      ++m_rowCount;
      this->endInsertRows();
    });

    QObject::connect(m_source, &Source::entryAboutToBeRemoved, this, [this](const Key& key) {
      m_pendingRow = static_cast<int>(rowOf(key));
      m_open = true;
      this->beginRemoveRows(QModelIndex(), m_pendingRow, m_pendingRow);
    });
    QObject::connect(m_source, &Source::entryRemoved, this, [this](const Key& key) {
      if (!heardTheBeginning()) return;
      m_index.erase(key, m_pendingRow);
      --m_rowCount;
      this->endRemoveRows();
    });

    // The roles go through as they came, and an empty list means every role, as it does for dataChanged.
    QObject::connect(m_source, &Source::entryChanged, this, [this](const Key& key, const QList<int>& roles) {
      const int row = static_cast<int>(rowOf(key));
      const int lastColumn = static_cast<const QAbstractItemModel*>(this)->columnCount() - 1;
      emit this->dataChanged(this->index(row, 0), this->index(row, lastColumn), roles);
    });

    if constexpr (Traits::movable) {
      QObject::connect(m_source, &Source::entryAboutToBeMoved, this, [this](const Key& from, const Key& to) {
        const int source = static_cast<int>(from);
        const int target = static_cast<int>(to);
        m_open = true;
        // beginMoveRows takes the row the block is put before, so a move down counts one further.
        this->beginMoveRows(QModelIndex(), source, source, QModelIndex(), target > source ? target + 1 : target);
      });
      QObject::connect(m_source, &Source::entryMoved, this, [this](const Key&, const Key&) {
        if (heardTheBeginning()) this->endMoveRows();
      });
    }

    // A run of a list is contiguous, so one begin and one end serve it however long it is.
    if constexpr (Traits::order == Order::indexed) {
      QObject::connect(m_source, &Source::rangeAboutToBeAdded, this, [this](const Key& first, const Key& last) {
        m_open = true;
        this->beginInsertRows(QModelIndex(), static_cast<int>(first), static_cast<int>(last));
      });
      QObject::connect(m_source, &Source::rangeAdded, this, [this](const Key& first, const Key& last) {
        if (!heardTheBeginning()) return;
        m_rowCount += static_cast<int>(last - first + 1);
        this->endInsertRows();
      });
      QObject::connect(m_source, &Source::rangeAboutToBeRemoved, this, [this](const Key& first, const Key& last) {
        m_open = true;
        this->beginRemoveRows(QModelIndex(), static_cast<int>(first), static_cast<int>(last));
      });
      QObject::connect(m_source, &Source::rangeRemoved, this, [this](const Key& first, const Key& last) {
        if (!heardTheBeginning()) return;
        m_rowCount -= static_cast<int>(last - first + 1);
        this->endRemoveRows();
      });
    }

    // A batch is announced once for the whole list of keys, and the model turns it into one begin and end pair per
    // run of consecutive rows. Both directions are done at the "done" signal, when the container holds the final
    // content and the model's own list of rows is still the old one.
    if constexpr (Traits::order == Order::sorted || Traits::order == Order::unordered) {
      QObject::connect(m_source, &Source::entriesAdded, this, [this](const QList<Key>& keys) { insertRuns(keys); });
      QObject::connect(m_source, &Source::entriesRemoved, this, [this](const QList<Key>& keys) { removeRuns(keys); });
    }

    QObject::connect(m_source, &Source::aboutToBeCleared, this, [this] {
      m_open = true;
      this->beginResetModel();
    });
    QObject::connect(m_source, &Source::cleared, this, [this] {
      if (!heardTheBeginning()) return;
      m_index.clear();
      m_rowCount = 0;
      this->endResetModel();
    });

    // A reset of the container replaces everything between one pair of signals, and the model follows with one reset.
    QObject::connect(m_source, &Source::aboutToBeReset, this, [this] {
      m_open = true;
      this->beginResetModel();
    });
    QObject::connect(m_source, &Source::resetCompleted, this, [this] {
      if (!heardTheBeginning()) return;
      takeContent();
      this->endResetModel();
    });
  }

  // The new keys come in the order their rows will have: ascending for a map, in the order they were added for a
  // hash or set. A new key's final row is where it lands among the old rows plus the new keys before it, and a run
  // of consecutive final rows is inserted with one begin and one end, the runs from the top so that each begin
  // names a row that is already right.
  void insertRuns(const QList<Key>& keys) {
    std::vector<qsizetype> rows;
    rows.reserve(static_cast<std::size_t>(keys.size()));
    for (qsizetype i = 0; i < keys.size(); ++i) rows.push_back(rowToInsert(keys[i]) + i);
    for (std::size_t from = 0; from < rows.size();) {
      std::size_t to = from;
      while (to + 1 < rows.size() && rows[to + 1] == rows[to] + 1) ++to;
      this->beginInsertRows(QModelIndex(), static_cast<int>(rows[from]), static_cast<int>(rows[to]));
      for (std::size_t k = from; k <= to; ++k) m_index.insert(keys[static_cast<qsizetype>(k)], rows[k]);
      m_rowCount += static_cast<int>(to - from + 1);
      this->endInsertRows();
      from = to + 1;
    }
  }

  // The rows of the removed keys, ascending and split into runs. The runs go from the bottom up, so the rows of
  // the runs still to do do not move.
  void removeRuns(const QList<Key>& keys) {
    std::vector<std::pair<qsizetype, Key>> doomed;
    doomed.reserve(static_cast<std::size_t>(keys.size()));
    for (const Key& key : keys) doomed.emplace_back(rowOf(key), key);
    std::sort(doomed.begin(), doomed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (std::size_t to = doomed.size(); to > 0;) {
      std::size_t from = to - 1;
      while (from > 0 && doomed[from - 1].first + 1 == doomed[from].first) --from;
      this->beginRemoveRows(QModelIndex(), static_cast<int>(doomed[from].first), static_cast<int>(doomed[to - 1].first));
      for (std::size_t k = to; k-- > from;) m_index.erase(doomed[k].second, doomed[k].first);
      m_rowCount -= static_cast<int>(to - from);
      this->endRemoveRows();
      to = from;
    }
  }

  QPointer<Source> m_source;
  typename RowIndexFor<Traits::order, Key>::type m_index;
  int m_pendingRow = -1;
  int m_rowCount = 0;
  bool m_open = false;  // this model heard the "about" signal of a change and has not seen its "done" signal
};

template <class C>
class ContainerModel : public ContainerModelBase<C, QAbstractListModel> {
  RQT_OBJECT

  using Base = ContainerModelBase<C, QAbstractListModel>;

 public:
  using Source = typename Base::Source;
  using Key = typename Base::Key;
  using Value = typename Base::Value;
  using Formatter = typename Column<C>::Display;
  using Editor = typename Column<C>::Editor;

  ContainerModel(Source* source, Formatter formatter, QObject* parent = nullptr)
      : Base(source, parent), m_formatter(std::move(formatter)) {}

  // With an editor the model accepts setData for the display role's edit.
  ContainerModel(Source* source, Formatter formatter, Editor editor, QObject* parent = nullptr)
      : Base(source, parent), m_formatter(std::move(formatter)), m_editor(std::move(editor)) {}

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= this->rowCount()) return {};
    const Key key = this->keyAt(index.row());
    if (role == Base::KeyRole && Base::hasKeyRole) return QVariant::fromValue(key);
    const Value* value = this->valueAt(key);
    if (role == Qt::DisplayRole && value) return m_formatter(key, *value);
    return {};
  }

 protected:
  bool editable(int) const override { return static_cast<bool>(m_editor); }
  const Editor& editorOf(int) const override { return m_editor; }

 private:
  Formatter m_formatter;
  Editor m_editor;
};

template <class C>
class ContainerTableModel : public ContainerModelBase<C, QAbstractTableModel> {
  RQT_OBJECT

  using Base = ContainerModelBase<C, QAbstractTableModel>;

 public:
  using Source = typename Base::Source;
  using Key = typename Base::Key;
  using Value = typename Base::Value;
  using Editor = typename Column<C>::Editor;

  ContainerTableModel(Source* source, std::vector<Column<C>> columns, QObject* parent = nullptr)
      : Base(source, parent), m_columns(std::move(columns)) {}

  int columnCount(const QModelIndex& parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_columns.size());
  }

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= this->rowCount() || index.column() >= columnCount()) return {};
    const Key key = this->keyAt(index.row());
    if (role == Base::KeyRole && Base::hasKeyRole) return QVariant::fromValue(key);
    const Value* value = this->valueAt(key);
    if (role == Qt::DisplayRole && value) return m_columns[static_cast<std::size_t>(index.column())].display(key, *value);
    return {};
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= columnCount()) return {};
    return m_columns[static_cast<std::size_t>(section)].header;
  }

 protected:
  bool editable(int column) const override {
    return column >= 0 && column < columnCount() && static_cast<bool>(m_columns[static_cast<std::size_t>(column)].edit);
  }
  const Editor& editorOf(int column) const override { return m_columns[static_cast<std::size_t>(column)].edit; }

 private:
  std::vector<Column<C>> m_columns;
};

}  // namespace observable
