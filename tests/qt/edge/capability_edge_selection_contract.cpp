// Selection edge cases: what setData of the selected role accepts, a selection through a sort and a filter proxy, a Multi
// selection through inserts, removals and resets of rows, the selection next to a QItemSelectionModel, a receiver that changes the
// selection while a notification is going out, the order of destruction, and the role names. QAbstractItemModelTester runs in
// fatal mode on every host and proxy.
// Catalogue: docs/qt-edge-cases/matrix-selection.md (S-01 to S-07).
#include "check.hpp"
#include "data_support.hpp"
#include "selection_hosts.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QSortFilterProxyModel>
#include <QtTest/QAbstractItemModelTester>

namespace {
using namespace selection_test;

constexpr auto kFatal = QAbstractItemModelTester::FailureReportingMode::Fatal;

ColorList colors(int n) {
  ColorList items;
  for (int i = 0; i < n; ++i) items.append(ColorItem{.name = QStringLiteral("color%1").arg(i), .hex = QStringLiteral("#%1").arg(i)});
  return items;
}

// The row of a proxy that shows the entry with this name.
int proxy_row_of(const QAbstractItemModel& proxy, const QString& name) {
  for (int row = 0; row < proxy.rowCount(); ++row)
    if (proxy.index(row, 0).data(NameRole).toString() == name) return row;
  return -1;
}

template <class Model>
QStringList selected_names(const Model& m) {
  QStringList out;
  for (int row = 0; row < m.rowCount(); ++row)
    if (m.data(m.index(row), Model::SelectedRole).toBool()) out << m.data(m.index(row), NameRole).toString();
  return out;
}

// S-01: setData of the selected role: a request that changes nothing says nothing and is still true; any QVariant is a bool
// (an invalid one is false, a non-empty string other than "false" and "0" is true); an invalid or out of range index and
// another role are refused.
void setData_of_the_selected_role() {
  ColorSingleSelectionModel single;
  QAbstractItemModelTester tester(&single, kFatal);
  single.resetWith(colors(4));
  int announced = 0;
  QObject::connect(&single, &QAbstractItemModel::dataChanged, &single, [&] { ++announced; });
  CHECK(single.setData(single.index(1), true, ColorSingleSelectionModel::SelectedRole) && announced == 1);
  CHECK(single.setData(single.index(1), true, ColorSingleSelectionModel::SelectedRole) && announced == 1);  // already so
  CHECK(single.setData(single.index(3), false, ColorSingleSelectionModel::SelectedRole) && announced == 1);  // another row, already not
  CHECK(single.setData(single.index(1), QVariant(), ColorSingleSelectionModel::SelectedRole) && !single.hasSelection());  // an invalid variant is false
  CHECK(single.setData(single.index(2), QStringLiteral("yes"), ColorSingleSelectionModel::SelectedRole) && single.hasSelection());
  CHECK(single.setData(single.index(2), QStringLiteral("false"), ColorSingleSelectionModel::SelectedRole) && !single.hasSelection());
  CHECK(single.setData(single.index(0), 7, ColorSingleSelectionModel::SelectedRole) && single.hasSelection());
  CHECK(single.setData(single.index(0), 0, ColorSingleSelectionModel::SelectedRole) && !single.hasSelection());
  const int before = announced;
  CHECK(!single.setData(QModelIndex(), true, ColorSingleSelectionModel::SelectedRole));
  CHECK(!single.setData(single.index(9), true, ColorSingleSelectionModel::SelectedRole));
  CHECK(!single.setData(single.index(0), true, Qt::EditRole) && !single.setData(single.index(0), true, NameRole));
  CHECK(announced == before && !single.hasSelection());
  CHECK(!single.data(QModelIndex(), ColorSingleSelectionModel::SelectedRole).toBool() && !single.data(single.index(9), ColorSingleSelectionModel::SelectedRole).toBool());

  ColorMultiSelectionModel multi;
  QAbstractItemModelTester multiTester(&multi, kFatal);
  multi.resetWith(colors(6));
  for (const int row : {1, 2, 3, 5}) CHECK(multi.setData(multi.index(row), true, ColorMultiSelectionModel::SelectedRole));
  int runs = 0;
  QObject::connect(&multi, &QAbstractItemModel::dataChanged, &multi, [&](const QModelIndex& a, const QModelIndex& b) { runs += (b.row() - a.row() + 1) * 10 + 1; });
  multi.clearSelection();  // runs [1,3] and [5,5]
  CHECK(runs == 31 + 11 && multi.selectedCount() == 0 && !multi.hasSelection());
  multi.clearSelection();
  CHECK(runs == 42);  // nothing selected: nothing announced
}

// S-02: a selection through a sort and a filter proxy: setData through the proxy reaches the right source row, a sort on the
// selected role moves selected rows (the proxy sees dataChanged for that role only), a filter on it shows the selected rows, and
// a persistent proxy index follows its entry.
void a_selection_through_proxies() {
  ColorMultiSelectionModel host;
  host.resetWith(colors(6));
  QSortFilterProxyModel sorted;
  sorted.setSourceModel(&host);
  sorted.setSortRole(ColorMultiSelectionModel::SelectedRole);
  sorted.sort(0, Qt::DescendingOrder);
  QAbstractItemModelTester sortedTester(&sorted, kFatal);
  QSortFilterProxyModel filtered;
  filtered.setSourceModel(&host);
  filtered.setFilterRole(ColorMultiSelectionModel::SelectedRole);
  filtered.setFilterFixedString("true");
  QAbstractItemModelTester filteredTester(&filtered, kFatal);

  CHECK(filtered.rowCount() == 0);
  // Rows that compare equal come in no promised order, so the entry is looked up by name.
  const int atFirst = proxy_row_of(sorted, "color5");
  QPersistentModelIndex last = sorted.index(atFirst, 0);
  CHECK(last.data(NameRole).toString() == "color5");
  CHECK(sorted.setData(sorted.index(atFirst, 0), true, ColorMultiSelectionModel::SelectedRole));  // select color5 through the sorted proxy
  CHECK(selected_names(host) == QStringList({"color5"}));
  CHECK(sorted.index(0, 0).data(NameRole).toString() == "color5");  // it sorted to the top
  CHECK(last.isValid() && last.row() == 0 && last.data(NameRole).toString() == "color5");
  CHECK(filtered.rowCount() == 1 && filtered.index(0, 0).data(NameRole).toString() == "color5");
  CHECK(filtered.setData(filtered.index(0, 0), false, ColorMultiSelectionModel::SelectedRole) && filtered.rowCount() == 0 && !host.hasSelection());
  host.setData(host.index(1), true, ColorMultiSelectionModel::SelectedRole);
  host.setData(host.index(4), true, ColorMultiSelectionModel::SelectedRole);
  CHECK(filtered.rowCount() == 2);
  host.removeItem(1);  // a selected row goes: the filtered proxy loses it
  CHECK(filtered.rowCount() == 1 && host.selectedCount() == 1);
  host.resetWith(colors(3));
  CHECK(filtered.rowCount() == 0 && sorted.rowCount() == 3 && !host.hasSelection());
}

// S-03: a Multi selection through runs: rows inserted inside the selected block are not selected, a run removed across the block's
// edge takes its selected rows, a reset and a clear drop everything.
void a_multi_selection_through_runs() {
  ColorMultiSelectionModel host;
  QAbstractItemModelTester tester(&host, kFatal);
  host.resetWith(colors(8));
  for (const int row : {2, 3, 4, 5}) host.setData(host.index(row), true, ColorMultiSelectionModel::SelectedRole);
  CHECK(selected_names(host) == QStringList({"color2", "color3", "color4", "color5"}));
  CHECK(host.container().insertRange(4, ColorList{{"new1", "#n1"}, {"new2", "#n2"}}));
  CHECK(selected_names(host) == QStringList({"color2", "color3", "color4", "color5"}) && host.selectedCount() == 4);  // the two new rows are not
  CHECK(!host.data(host.index(4), ColorMultiSelectionModel::SelectedRole).toBool() && !host.data(host.index(5), ColorMultiSelectionModel::SelectedRole).toBool());
  CHECK(host.container().removeRange(3, 4));  // color3, color4, new1, new2: two of them selected
  CHECK(selected_names(host) == QStringList({"color2", "color5"}) && host.selectedCount() == 2);
  CHECK(host.container().removeRange(0, 2));  // color0, color1
  CHECK(selected_names(host) == QStringList({"color2", "color5"}));
  host.resetWith(colors(2));
  CHECK(!host.hasSelection() && host.selectedCount() == 0);
  host.setData(host.index(1), true, ColorMultiSelectionModel::SelectedRole);
  CHECK(host.container().clear() && !host.hasSelection() && host.rowCount() == 0);
  CHECK(host.container().insert(0, ColorItem{"again", "#a"}) && !host.data(host.index(0), ColorMultiSelectionModel::SelectedRole).toBool());
}

// S-04: the selection of the host and a QItemSelectionModel on it are two things: neither changes the other; both follow the rows.
void next_to_a_QItemSelectionModel() {
  ColorMultiSelectionModel host;
  QAbstractItemModelTester tester(&host, kFatal);
  host.resetWith(colors(6));
  QItemSelectionModel view(&host);
  view.setCurrentIndex(host.index(3), QItemSelectionModel::NoUpdate);
  view.select(host.index(2), QItemSelectionModel::Select);
  host.setData(host.index(4), true, ColorMultiSelectionModel::SelectedRole);
  CHECK(view.isSelected(host.index(2)) && !view.isSelected(host.index(4)) && host.selectedCount() == 1);
  view.clearSelection();
  CHECK(host.selectedCount() == 1 && selected_names(host) == QStringList({"color4"}));
  host.clearSelection();
  CHECK(view.currentIndex().row() == 3);
  // Rows come and go: both follow their entries.
  view.select(host.index(4), QItemSelectionModel::Select);
  host.setData(host.index(4), true, ColorMultiSelectionModel::SelectedRole);
  host.insertItem(0, ColorItem{"first", "#f"});
  CHECK(view.currentIndex().row() == 4 && view.isSelected(host.index(5)) && selected_names(host) == QStringList({"color4"}) && host.data(host.index(5), NameRole).toString() == "color4");
  host.removeItem(5);  // the selected entry goes: both lose it
  CHECK(!view.isSelected(host.index(5)) && !host.hasSelection() && !view.hasSelection());
  host.resetWith(colors(2));
  CHECK(!view.currentIndex().isValid() && !host.hasSelection());
}

// S-05: a receiver of a notification changes the selection again: the nested call completes its own notifications inside the
// outer one, and the state after is that of the last request.
void a_receiver_changes_the_selection_again() {
  ColorSingleSelectionModel host;
  QAbstractItemModelTester tester(&host, kFatal);
  host.resetWith(colors(5));
  QStringList log;
  QObject::connect(&host, &ColorSingleSelectionModel::selectedRowChanged, &host, [&] {
    log << "row";
    if (host.hasSelection() && host.selectedName() == "color1") host.setData(host.index(3), true, ColorSingleSelectionModel::SelectedRole);
  });
  QObject::connect(&host, &ColorSingleSelectionModel::hasSelectionChanged, &host, [&] { log << (host.hasSelection() ? "has" : "none"); });
  host.setData(host.index(1), true, ColorSingleSelectionModel::SelectedRole);
  CHECK(host.selectedName() == "color3" && host.hasSelection());
  CHECK(selected_names(host) == QStringList({"color3"}));
  // The outer request announced the row first, and the receiver's request ran inside that and announced its own row change; the
  // presence flip of the outer one came last.
  CHECK(log == (QStringList{"row", "row", "has"}));
  // A receiver of the rows' own signals changes the selection of a row that is there.
  ColorMultiSelectionModel multi;
  QAbstractItemModelTester multiTester(&multi, kFatal);
  multi.resetWith(colors(3));
  QObject::connect(&multi, &QAbstractItemModel::rowsInserted, &multi, [&](const QModelIndex&, int first, int) { multi.setData(multi.index(first), true, ColorMultiSelectionModel::SelectedRole); });
  multi.insertItem(1, ColorItem{"fresh", "#f"});
  CHECK(selected_names(multi) == QStringList({"fresh"}) && multi.selectedCount() == 1);
}

// S-06: destruction: a container deleted under a host resets it, the selection goes, and the notifications follow; a host deleted
// first leaves the container alone.
void destruction_order() {
  {
    auto* host = new ColorSingleSelectionModel;
    host->resetWith(colors(3));
    host->setData(host->index(1), true, ColorSingleSelectionModel::SelectedRole);
    int has = 0, row = 0, resets = 0;
    QObject::connect(host, &ColorSingleSelectionModel::hasSelectionChanged, host, [&] { ++has; });
    QObject::connect(host, &ColorSingleSelectionModel::selectedRowChanged, host, [&] { ++row; });
    QObject::connect(host, &QAbstractItemModel::modelReset, host, [&] { ++resets; });
    delete &host->container();  // the container goes, the host stays
    CHECK(resets == 1 && has == 1 && row == 1);
    CHECK(host->rowCount() == 0 && !host->hasSelection() && host->selectedName().isEmpty());
    delete host;
  }
  {
    auto* host = new ColorMultiSelectionModel;
    host->resetWith(colors(3));
    host->setData(host->index(1), true, ColorMultiSelectionModel::SelectedRole);
    int has = 0, count = 0;
    QObject::connect(host, &ColorMultiSelectionModel::hasSelectionChanged, host, [&] { ++has; });
    QObject::connect(host, &ColorMultiSelectionModel::selectedCountChanged, host, [&] { ++count; });
    delete &host->container();
    CHECK(has == 1 && count == 1 && host->selectedCount() == 0 && host->rowCount() == 0);
    delete host;
  }
  // A host deleted first: its container is a child and goes with it; nothing is announced to anyone else.
  QObject outsider;
  auto* host = new ColorMultiSelectionModel;
  host->resetWith(colors(2));
  QPointer<QObject> container(&host->container());
  delete host;
  CHECK(!container);
}

// S-07: the selected role is the largest base role plus one (named "isSelected"), is a role of the model and of no base role, and a
// role or a name that is taken is refused when the names are appended (a debug build asserts).
void role_names() {
  static_assert(ColorSingleSelectionModel::SelectedRole == HexRole + 1 && ColorMultiSelectionModel::SelectedRole == HexRole + 1);
  ColorMultiSelectionModel multi;
  const QHash<int, QByteArray> names = multi.roleNames();
  CHECK(names.value(ColorMultiSelectionModel::SelectedRole) == "isSelected" && names.value(NameRole) == "name" && names.value(HexRole) == "hex");
  CHECK(names.value(Qt::DisplayRole) == "display" && !names.contains(Qt::UserRole));  // the container's own roles stay (a list has no key role)
  CHECK(names.size() == multi.ColorListModel::roleNames().size() + 1);
#ifndef QT_NO_DEBUG
  using edge_data::aborts;
  CHECK(aborts([] {
    QHash<int, QByteArray> taken{{ColorMultiSelectionModel::SelectedRole, "other"}};
    ColorMultiSelectionModel::Tracker::appendRoleNames(taken);
  }));
  CHECK(aborts([] {
    QHash<int, QByteArray> taken{{NameRole, "isSelected"}};
    ColorMultiSelectionModel::Tracker::appendRoleNames(taken);
  }));
  CHECK(!aborts([] {
    QHash<int, QByteArray> free{{NameRole, "name"}};
    ColorMultiSelectionModel::Tracker::appendRoleNames(free);
  }));
#endif
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
#ifndef QT_NO_DEBUG
  role_names();  // forks first
#endif
  setData_of_the_selected_role();
  a_selection_through_proxies();
  a_multi_selection_through_runs();
  next_to_a_QItemSelectionModel();
  a_receiver_changes_the_selection_again();
  destruction_order();
#ifdef QT_NO_DEBUG
  role_names();
#endif
  return rqt_test::finish("capability_edge_selection_contract");
}
