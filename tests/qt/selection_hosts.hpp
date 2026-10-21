// Two models that host a selection tracker, over an observable container of colours: a Single one (at most one row is
// selected) and a Multi one (any rows). They are the example of the contract: the tracker is a member, the notifications are
// rqt::signal members, the selected role is a role of the model, and the tracker is kept in step with the rows from the
// model's own signals, before a view hears of the change, while the notifications are announced after it.
#pragma once

#include <container_model.hpp>
#include <selection_tracker.hpp>

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QString>

#include <array>
#include <optional>

namespace selection_test {

struct ColorItem {
  QString name;
  QString hex;
  bool operator==(const ColorItem&) const = default;
};

using ColorList = QList<ColorItem>;
using ColorSource = observable::ObservableContainer<ColorList>;

enum ColorRole { NameRole = Qt::UserRole + 1, HexRole };
inline constexpr std::array<int, 2> kColorRoles{NameRole, HexRole};

// A list model of colours with a name and a hex role, over its own container (a child of the model).
class ColorListModel : public observable::ContainerModel<ColorList> {
  RQT_OBJECT

 public:
  using Base = observable::ContainerModel<ColorList>;
  explicit ColorListModel(QObject* parent = nullptr)
      : Base(new ColorSource, [](const qsizetype&, const ColorItem& c) { return QVariant(c.name); }, parent) {
    source()->setParent(this);
  }

  void resetWith(ColorList items) { container().resetWith(std::move(items)); }
  bool insertItem(int row, ColorItem item) { return container().insert(row, std::move(item)); }
  bool removeItem(int row) { return container().remove(row); }
  const ColorList& items() const { return source()->items(); }
  ColorSource& container() { return *source(); }

  QHash<int, QByteArray> roleNames() const override {
    QHash<int, QByteArray> names = Base::roleNames();
    names.insert(NameRole, "name");
    names.insert(HexRole, "hex");
    return names;
  }

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (role != NameRole && role != HexRole) return Base::data(index, role);
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const ColorItem& item = items().at(index.row());
    return role == NameRole ? item.name : item.hex;
  }
};

// What a host has to announce after the rows have changed, collected from the model's signals.
struct Pending {
  bool open = false;
  bool had = false;
  bool shifted = false;
  observable::SelectionDelta delta;
};

class ColorSingleSelectionModel : public ColorListModel {
  RQT_OBJECT
  RQT_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
  RQT_PROPERTY(QString selectedName READ selectedName NOTIFY selectedRowChanged)

 public:
  using Tracker = observable::SelectionTracker<observable::SelectionMode::Single, kColorRoles>;
  static constexpr int SelectedRole = Tracker::selectedRole();

  explicit ColorSingleSelectionModel(QObject* parent = nullptr) : ColorListModel(parent) {
    using M = QAbstractItemModel;
    // The tracker follows the rows before a view hears of them.
    connect(this, &M::rowsInserted, this, [this](const QModelIndex&, int first, int last) { m_pending.shifted |= m_tracker.rowsInserted(first, last - first + 1); });
    connect(this, &M::rowsAboutToBeRemoved, this, [this] {
      if (!m_pending.open) m_pending = {.open = true, .had = m_tracker.hasSelection(), .delta = {}};
    });
    connect(this, &M::rowsRemoved, this, [this](const QModelIndex&, int first, int last) {
      const observable::SelectionDelta d = m_tracker.rowsRemoved(first, last);
      m_pending.delta.selectionChanged |= d.selectionChanged;
      m_pending.delta.countChanged |= d.countChanged;
    });
    connect(this, &M::modelAboutToBeReset, this, [this] { m_pending = {.open = true, .had = m_tracker.hasSelection(), .delta = {}}; });
    connect(this, &M::modelReset, this, [this] { m_tracker.reset(rowCount()); });
    // And the notifications go out after the container's own announcement has reached the views.
    connect(&container(), &ColorSource::entryAdded, this, [this](const qsizetype&) { announce(false); });
    connect(&container(), &ColorSource::rangeAdded, this, [this](const qsizetype&, const qsizetype&) { announce(false); });
    connect(&container(), &ColorSource::entriesAdded, this, [this](const QList<qsizetype>&) { announce(false); });
    connect(&container(), &ColorSource::entryRemoved, this, [this](const qsizetype&) { announce(true); });
    connect(&container(), &ColorSource::rangeRemoved, this, [this](const qsizetype&, const qsizetype&) { announce(true); });
    connect(&container(), &ColorSource::entriesRemoved, this, [this](const QList<qsizetype>&) { announce(true); });
    connect(&container(), &ColorSource::resetCompleted, this, [this] { announce(true); });
    connect(&container(), &ColorSource::cleared, this, [this] { announce(true); });
    // A container that goes resets the model, and with it the selection: the notifications follow that reset too.
    connect(&container(), &QObject::destroyed, this, [this] { announce(true); });
  }

  signals:
  rqt::signal<void()> hasSelectionChanged;
  rqt::signal<void()> selectedRowChanged;

 public:
  bool hasSelection() const { return m_tracker.hasSelection(); }
  QString selectedName() const { return observable::selectedValue<&ColorItem::name>(m_tracker, items()); }
  [[= rqt::invokable]] void clearSelection() { observable::clearSelectionAndNotify(this, m_tracker); }

  QHash<int, QByteArray> roleNames() const override {
    QHash<int, QByteArray> names = ColorListModel::roleNames();
    return Tracker::appendRoleNames(names);
  }
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (role != SelectedRole) return ColorListModel::data(index, role);
    return index.isValid() && index.row() < rowCount() && m_tracker.isSelected(index.row());
  }
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override {
    if (role != SelectedRole) return ColorListModel::setData(index, value, role);
    if (!index.isValid() || index.row() >= rowCount()) return false;
    return observable::applySelectionValue(this, m_tracker, index, value);
  }

 private:
  // The notifications of the rows that came or went, in the contract's order: the selected row, then whether there is a selection.
  void announce(bool removal) {
    const Pending pending = std::exchange(m_pending, {});
    if (!removal) {
      if (pending.shifted) selectedRowChanged();
      return;
    }
    if (!pending.open) return;
    const bool flipped = pending.had != m_tracker.hasSelection();
    if (pending.delta.selectionChanged || flipped) selectedRowChanged();
    if (flipped) hasSelectionChanged();
  }

  Tracker m_tracker;
  Pending m_pending;
};

class ColorMultiSelectionModel : public ColorListModel {
  RQT_OBJECT
  RQT_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
  RQT_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)

 public:
  using Tracker = observable::SelectionTracker<observable::SelectionMode::Multi, kColorRoles>;
  static constexpr int SelectedRole = Tracker::selectedRole();

  explicit ColorMultiSelectionModel(QObject* parent = nullptr) : ColorListModel(parent) {
    using M = QAbstractItemModel;
    m_tracker.reset(rowCount());
    connect(this, &M::rowsInserted, this, [this](const QModelIndex&, int first, int last) { m_tracker.rowsInserted(first, last - first + 1); });
    connect(this, &M::rowsAboutToBeRemoved, this, [this] {
      if (!m_pending.open) m_pending = {.open = true, .had = m_tracker.hasSelection(), .delta = {}};
    });
    connect(this, &M::rowsRemoved, this, [this](const QModelIndex&, int first, int last) {
      const observable::SelectionDelta d = m_tracker.rowsRemoved(first, last);
      m_pending.delta.selectionChanged |= d.selectionChanged;
      m_pending.delta.countChanged |= d.countChanged;
    });
    connect(this, &M::modelAboutToBeReset, this, [this] { m_pending = {.open = true, .had = m_tracker.hasSelection(), .delta = {}}; });
    connect(this, &M::modelReset, this, [this] { m_tracker.reset(rowCount()); });
    connect(&container(), &ColorSource::entryRemoved, this, [this](const qsizetype&) { announce(); });
    connect(&container(), &ColorSource::rangeRemoved, this, [this](const qsizetype&, const qsizetype&) { announce(); });
    connect(&container(), &ColorSource::entriesRemoved, this, [this](const QList<qsizetype>&) { announce(); });
    connect(&container(), &ColorSource::resetCompleted, this, [this] { announce(); });
    connect(&container(), &ColorSource::cleared, this, [this] { announce(); });
    connect(&container(), &QObject::destroyed, this, [this] { announce(); });
  }

  signals:
  rqt::signal<void()> hasSelectionChanged;
  rqt::signal<void()> selectedCountChanged;

 public:
  bool hasSelection() const { return m_tracker.hasSelection(); }
  int selectedCount() const { return m_tracker.count(); }
  [[= rqt::invokable]] void clearSelection() { observable::clearSelectionAndNotify(this, m_tracker); }

  QHash<int, QByteArray> roleNames() const override {
    QHash<int, QByteArray> names = ColorListModel::roleNames();
    return Tracker::appendRoleNames(names);
  }
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (role != SelectedRole) return ColorListModel::data(index, role);
    return index.isValid() && index.row() < m_tracker.rowCount() && m_tracker.isSelected(index.row());
  }
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override {
    if (role != SelectedRole) return ColorListModel::setData(index, value, role);
    if (!index.isValid() || index.row() >= rowCount()) return false;
    return observable::applySelectionValue(this, m_tracker, index, value);
  }

 private:
  void announce() {
    const Pending pending = std::exchange(m_pending, {});
    if (!pending.open) return;
    const bool flipped = pending.had != m_tracker.hasSelection();
    if (pending.delta.countChanged || flipped) selectedCountChanged();
    if (flipped) hasSelectionChanged();
  }

  Tracker m_tracker;
  Pending m_pending;
};

}  // namespace selection_test
