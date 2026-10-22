// Proxy edge cases, the other proxy models and stacks of them: a sort over a filter over the model,
// QIdentityProxyModel, QConcatenateTablesProxyModel over two containers' table models (one of them reset, removed, grown by
// a batch) and QTransposeProxyModel over a table model. Each proxy is compared with its source cell by cell after every
// change, and QAbstractItemModelTester runs in fatal mode on the models and on every proxy.
// Catalogue: docs/qt-edge-cases/proxies-selection.md.
#include "proxy_support.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QConcatenateTablesProxyModel>
#include <QtCore/QTransposeProxyModel>
#include <QtCore/QStringListModel>

namespace {
using namespace edge_proxy;
using MapRig = Rig<QMap<int, QString>>;

QStringList shown(const QAbstractItemModel& m) { return column(m); }
int cols(const QAbstractItemModel& m) { return m.columnCount(); }

// a sort over a filter over the model: the stack shows the filtered rows sorted, maps down to the model through
// both proxies and back, and keeps doing so through batches, replaces, a reset and a clear.
void a_sort_over_a_filter_over_the_model() {
  MapRig rig;
  QSortFilterProxyModel filter;
  filter.setSourceModel(rig.model.get());
  filter.setFilterRegularExpression("^[a-m]");
  QAbstractItemModelTester filterTester(&filter, kFatal);
  QSortFilterProxyModel sort;
  sort.setSourceModel(&filter);
  sort.setSortRole(Qt::DisplayRole);
  sort.sort(0);
  QAbstractItemModelTester sortTester(&sort, kFatal);

  const auto check = [&] {
    CHECK(mapping_is_consistent(filter) && mapping_is_consistent(sort));
    QStringList expected;
    for (const QString& s : shown(*rig.model))
      if (!s.isEmpty() && s[0] >= 'a' && s[0] <= 'm') expected.append(s);
    expected.sort();
    CHECK(shown(sort) == expected);
    for (int row = 0; row < sort.rowCount(); ++row) {
      const QModelIndex down = filter.mapToSource(sort.mapToSource(sort.index(row, 0)));
      CHECK(down.isValid() && down.data() == sort.index(row, 0).data() && sort.mapFromSource(filter.mapFromSource(down)) == sort.index(row, 0));
    }
  };
  for (const auto& [key, text] : {std::pair{5, "pear"}, {2, "apple"}, {9, "kiwi"}, {1, "cherry"}, {7, "fig"}}) CHECK(rig.source.insert(key, text));
  check();
  CHECK(rig.source.replace(5, "banana"));  // pear is out, banana in
  check();
  CHECK(rig.source.replace(2, "zebra"));
  check();
  CHECK(rig.source.insertMany(QMap<int, QString>{{20, "lime"}, {21, "melon"}, {22, "nut"}, {0, "date"}}) == 4);
  check();
  CHECK(rig.source.removeMany({20, 9, 0, 77}) == 3);
  check();
  rig.source.resetWith(QMap<int, QString>{{1, "grape"}, {2, "orange"}, {3, "mango"}});
  check();
  CHECK(rig.source.clear());
  check();
  CHECK(sort.rowCount() == 0);
}

template <class RigT>
void an_identity_proxy_mirrors(RigT& rig, auto add) {
  QIdentityProxyModel identity;
  identity.setSourceModel(rig.model.get());
  QAbstractItemModelTester tester(&identity, kFatal);
  QPersistentModelIndex tracked;
  const auto check = [&] {
    CHECK(identity.rowCount() == rig.model->rowCount() && identity.columnCount() == cols(*rig.model));
    for (int row = 0; row < identity.rowCount(); ++row)
      for (int col = 0; col < identity.columnCount(); ++col) {
        const QModelIndex p = identity.index(row, col);
        CHECK(identity.mapToSource(p) == rig.model->index(row, col) && identity.mapFromSource(rig.model->index(row, col)) == p);
        CHECK(p.data() == rig.model->index(row, col).data() && p.data(kKeyRole) == rig.model->index(row, col).data(kKeyRole));
      }
  };
  add(rig.source);
  check();
  tracked = identity.index(1, 0);
  const QString text = tracked.data().toString();
  CHECK(rig.source.remove(rig.model->index(0, 0).data(kKeyRole).toInt()));
  check();
  CHECK(tracked.isValid() && tracked.row() == 0 && tracked.data().toString() == text);
  rig.source.resetWith({});
  check();
  CHECK(!tracked.isValid());
}

// an identity proxy is row for row the model it sits on, for a list model and for a table model, through
// inserts, a removal before its tracked index, a reset.
void an_identity_proxy_is_the_model() {
  MapRig map;
  an_identity_proxy_mirrors(map, [](auto& s) { s.insertMany(QMap<int, QString>{{1, "a"}, {2, "b"}, {3, "c"}}); });
  Rig<QHash<int, QString>> hash;
  an_identity_proxy_mirrors(hash, [](auto& s) { s.insertMany(QHash<int, QString>{{1, "a"}, {2, "b"}, {3, "c"}}); });
  Rig<QSet<int>> set;
  an_identity_proxy_mirrors(set, [](auto& s) { s.insertMany(QSet<int>{1, 2, 3}); });
  Rig<QMap<int, QString>, true> table;
  an_identity_proxy_mirrors(table, [](auto& s) { s.insertMany(QMap<int, QString>{{1, "a"}, {2, "b"}, {3, "c"}}); });
}

// an identity proxy forwards the roles of a change to its views, and a batch's runs one for one.
void an_identity_proxy_forwards_roles_and_runs() {
  MapRig rig;
  QIdentityProxyModel identity;
  identity.setSourceModel(rig.model.get());
  QList<int> roles;
  int inserts = 0;
  QObject::connect(&identity, &QAbstractItemModel::dataChanged, &identity, [&](const QModelIndex&, const QModelIndex&, const QList<int>& r) { roles = r; });
  QObject::connect(&identity, &QAbstractItemModel::rowsInserted, &identity, [&] { ++inserts; });
  CHECK(rig.source.insertMany(QMap<int, QString>{{10, "a"}, {30, "c"}, {50, "e"}}) == 3);
  CHECK(inserts == 1);
  CHECK(rig.source.insertMany(QMap<int, QString>{{0, "z"}, {20, "b"}, {40, "d"}, {60, "f"}}) == 4);
  CHECK(inserts == 1 + 4);  // four runs of one row
  CHECK(rig.source.editValue(30, [](QString& v) { v = "C"; }, {Qt::DisplayRole, Qt::UserRole + 4}));
  CHECK(roles == QList<int>({Qt::DisplayRole, Qt::UserRole + 4}));
  CHECK(rig.source.replace(30, "c"));
  CHECK(roles.isEmpty());
}

// QConcatenateTablesProxyModel over two containers' table models lists the rows of the first and then the rows of the
// second; a change in the first shifts the second's rows by the right amount, a batch in either is announced
// to the right rows, a reset of one source keeps the other's rows, and removing a source drops its rows.
void concatenating_two_containers() {
  Rig<QMap<int, QString>, true> first;
  Rig<QList<QString>, true> second;
  QConcatenateTablesProxyModel concat;
  concat.addSourceModel(first.model.get());
  concat.addSourceModel(second.model.get());
  QAbstractItemModelTester tester(&concat, kFatal);

  const auto expected = [&] { return shown(*first.model) + shown(*second.model); };
  const auto check = [&] {
    CHECK(shown(concat) == expected());
    for (int row = 0; row < concat.rowCount(); ++row) {
      const QModelIndex p = concat.index(row, 0);
      const QModelIndex s = concat.mapToSource(p);
      CHECK(s.isValid() && concat.mapFromSource(s) == p && s.data() == p.data());
      CHECK(s.model() == (row < first.model->rowCount() ? static_cast<QAbstractItemModel*>(first.model.get()) : second.model.get()));
    }
  };
  CHECK(concat.rowCount() == 0 && concat.columnCount() == 2);
  CHECK(second.source.insert(0, "s0") && second.source.insert(1, "s1"));
  check();
  QPersistentModelIndex inSecond = concat.index(0, 0);
  CHECK(first.source.insertMany(QMap<int, QString>{{3, "c"}, {1, "a"}, {2, "b"}}) == 3);
  check();
  CHECK(inSecond.row() == 3 && inSecond.data().toString() == "s0");  // pushed down by the batch in the first
  CHECK(first.source.removeMany({1, 3}) == 2);
  check();
  CHECK(inSecond.row() == 1 && inSecond.data().toString() == "s0");
  CHECK(first.source.replace(2, "B") && second.source.replace(1, "S1"));
  check();
  second.source.resetWith(QList<QString>{"r0", "r1", "r2"});
  check();
  CHECK(!inSecond.isValid() || inSecond.row() >= 0);
  first.source.resetWith({});
  check();
  CHECK(concat.rowCount() == 3);
  CHECK(first.source.insert(9, "n"));
  check();
  concat.removeSourceModel(first.model.get());
  CHECK(shown(concat) == shown(*second.model));
  CHECK(concat.mapToSource(concat.index(0, 0)).model() == second.model.get());
  CHECK(first.source.insert(10, "after removal") && shown(concat) == shown(*second.model));  // no longer followed
  CHECK(second.source.clear() && concat.rowCount() == 0);
}

// a concatenation reads the source models as they are when it is made: rows already in them are listed.
void concatenating_models_that_already_hold_rows() {
  Rig<QMap<int, QString>, true> first;
  Rig<QHash<int, QString>, true> second;
  CHECK(first.source.insert(1, "a") && second.source.insert(5, "x") && second.source.insert(6, "y"));
  QConcatenateTablesProxyModel concat;
  concat.addSourceModel(second.model.get());
  concat.addSourceModel(first.model.get());
  QAbstractItemModelTester tester(&concat, kFatal);
  CHECK(shown(concat) == QStringList({"x", "y", "a"}));
  CHECK(first.source.clear() && shown(concat) == QStringList({"x", "y"}));
}

// QTransposeProxyModel over a table model: rows are columns, and a change of rows in the source is a change of
// columns in the proxy; every cell is the source's transposed cell after every change; the headers swap sides.
void transposing_a_table_model() {
  Rig<QMap<int, QString>, true> rig;
  QTransposeProxyModel transpose;
  transpose.setSourceModel(rig.model.get());
  QAbstractItemModelTester tester(&transpose, kFatal);
  const auto check = [&] {
    CHECK(transpose.rowCount() == rig.model->columnCount() && transpose.columnCount() == rig.model->rowCount());
    for (int r = 0; r < transpose.rowCount(); ++r)
      for (int c = 0; c < transpose.columnCount(); ++c) {
        const QModelIndex t = transpose.index(r, c);
        CHECK(t.data() == rig.model->index(c, r).data() && transpose.mapToSource(t) == rig.model->index(c, r));
        CHECK(transpose.mapFromSource(rig.model->index(c, r)) == t);
        CHECK(t.data(kKeyRole) == rig.model->index(c, r).data(kKeyRole));
      }
  };
  check();
  CHECK(transpose.headerData(0, Qt::Vertical).toString() == "first" && transpose.headerData(1, Qt::Vertical).toString() == "second");
  int columnsInserted = 0;
  int columnsRemoved = 0;
  int resets = 0;
  QObject::connect(&transpose, &QAbstractItemModel::columnsInserted, &transpose, [&] { ++columnsInserted; });
  QObject::connect(&transpose, &QAbstractItemModel::columnsRemoved, &transpose, [&] { ++columnsRemoved; });
  QObject::connect(&transpose, &QAbstractItemModel::modelReset, &transpose, [&] { ++resets; });
  CHECK(rig.source.insertMany(QMap<int, QString>{{10, "a"}, {20, "b"}, {30, "c"}, {5, "z"}}) == 4);
  check();
  CHECK(columnsInserted == 1 && transpose.columnCount() == 4);  // all new, one run
  CHECK(rig.source.insertMany(QMap<int, QString>{{1, "p"}, {15, "q"}, {40, "r"}}) == 3);
  check();
  CHECK(columnsInserted == 1 + 3);  // before 5, between 10 and 20, after 30: three runs
  int changed = 0;
  QObject::connect(&transpose, &QAbstractItemModel::dataChanged, &transpose, [&] { ++changed; });
  CHECK(rig.source.replace(15, "Q"));
  check();
  CHECK(changed == 1);
  CHECK(rig.source.removeMany({1, 15, 40, 99}) == 3);
  check();
  CHECK(columnsRemoved == 3 && transpose.columnCount() == 4);
  rig.source.resetWith(QMap<int, QString>{{7, "u"}});
  check();
  CHECK(resets == 1 && transpose.columnCount() == 1);
  CHECK(rig.source.remove(7));
  check();
  CHECK(transpose.columnCount() == 0 && transpose.rowCount() == 2);  // the columns of the table are still its rows
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  a_sort_over_a_filter_over_the_model();
  an_identity_proxy_is_the_model();
  an_identity_proxy_forwards_roles_and_runs();
  concatenating_two_containers();
  concatenating_models_that_already_hold_rows();
  transposing_a_table_model();
  return rqt_test::finish("capability_edge_proxy_other");
}
