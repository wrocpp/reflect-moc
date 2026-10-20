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
// A model and its container live in one thread, so the connections are direct; a model made for a container of
// another thread asserts in a debug build.
#pragma once

#include "observable_container.hpp"
#include "row_index.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QAbstractListModel>
#include <QtCore/QAbstractTableModel>
#include <QtCore/QString>
#include <QtCore/QThread>

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
  // A list entry has no identity but its row, which shifts under every insert and remove without a signal,
  // so a role that answered with it would go stale in a sorting proxy. Lists do not offer the role.
  static constexpr bool hasKeyRole = Traits::order != Order::indexed;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_source->size());
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
    if (!index.isValid() || index.row() >= rowCount() || role != Qt::EditRole || !editable(index.column())) return false;
    const Key key = keyAt(index.row());
    const std::optional<Value> edited = editorOf(index.column())(m_source->value(key), value);
    return edited && m_source->replace(key, *edited);
  }

 protected:
  ContainerModelBase(Source* source, QObject* parent) : Model(parent), m_source(source) {
    Q_ASSERT_X(source->thread() == this->thread(), "observable::ContainerModelBase",
               "a model must live in the thread of its container (move data between threads, not the model)");
    m_index.assign(Traits::keys(source->items()));
    connectSource();
  }

  using Editor = typename Column<C>::Editor;
  virtual bool editable(int column) const = 0;
  virtual const Editor& editorOf(int column) const = 0;

  Key keyAt(qsizetype row) const { return m_index.keyAt(row); }
  Source* source() const { return m_source; }

 private:
  qsizetype rowOf(const Key& key) const { return m_index.rowOf(key); }
  qsizetype rowToInsert(const Key& key) const { return m_index.rowToInsert(key); }

  void connectSource() {
    QObject::connect(m_source, &Source::entryAboutToBeAdded, this, [this](const Key& key) {
      m_pendingRow = static_cast<int>(rowToInsert(key));
      this->beginInsertRows(QModelIndex(), m_pendingRow, m_pendingRow);
    });
    QObject::connect(m_source, &Source::entryAdded, this, [this](const Key& key) {
      m_index.insert(key, m_pendingRow);
      this->endInsertRows();
    });

    QObject::connect(m_source, &Source::entryAboutToBeRemoved, this, [this](const Key& key) {
      m_pendingRow = static_cast<int>(rowOf(key));
      this->beginRemoveRows(QModelIndex(), m_pendingRow, m_pendingRow);
    });
    QObject::connect(m_source, &Source::entryRemoved, this, [this](const Key& key) {
      m_index.erase(key, m_pendingRow);
      this->endRemoveRows();
    });

    QObject::connect(m_source, &Source::entryChanged, this, [this](const Key& key) {
      const int row = static_cast<int>(rowOf(key));
      const int lastColumn = static_cast<const QAbstractItemModel*>(this)->columnCount() - 1;
      emit this->dataChanged(this->index(row, 0), this->index(row, lastColumn));
    });

    if constexpr (Traits::movable) {
      QObject::connect(m_source, &Source::entryAboutToBeMoved, this, [this](const Key& from, const Key& to) {
        const int source = static_cast<int>(from);
        const int target = static_cast<int>(to);
        // beginMoveRows takes the row the block is put before, so a move down counts one further.
        this->beginMoveRows(QModelIndex(), source, source, QModelIndex(), target > source ? target + 1 : target);
      });
      QObject::connect(m_source, &Source::entryMoved, this, [this](const Key&, const Key&) { this->endMoveRows(); });
    }

    // A run of a list is contiguous, so one begin and one end serve it however long it is.
    if constexpr (Traits::order == Order::indexed) {
      QObject::connect(m_source, &Source::rangeAboutToBeAdded, this, [this](const Key& first, const Key& last) {
        this->beginInsertRows(QModelIndex(), static_cast<int>(first), static_cast<int>(last));
      });
      QObject::connect(m_source, &Source::rangeAdded, this, [this](const Key&, const Key&) { this->endInsertRows(); });
      QObject::connect(m_source, &Source::rangeAboutToBeRemoved, this, [this](const Key& first, const Key& last) {
        this->beginRemoveRows(QModelIndex(), static_cast<int>(first), static_cast<int>(last));
      });
      QObject::connect(m_source, &Source::rangeRemoved, this, [this](const Key&, const Key&) { this->endRemoveRows(); });
    }

    QObject::connect(m_source, &Source::aboutToBeCleared, this, [this] { this->beginResetModel(); });
    QObject::connect(m_source, &Source::cleared, this, [this] {
      m_index.clear();
      this->endResetModel();
    });
  }

  Source* m_source;
  typename RowIndexFor<Traits::order, Key>::type m_index;
  int m_pendingRow = -1;
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
    if (role == Qt::DisplayRole) return m_formatter(key, this->source()->value(key));
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
    if (role == Qt::DisplayRole) return m_columns[static_cast<std::size_t>(index.column())].display(key, this->source()->value(key));
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
