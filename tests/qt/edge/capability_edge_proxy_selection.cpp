// Selection edge cases: QItemSelectionModel over the container models and through proxies.
// The current index and the selection of a list model are compared, step by step and signal by signal, with
// the same selection over QStringListModel (Qt's own list model) under the same change; where Qt is the reference the
// container model has to behave as it does. For a map the selection is followed by key, through batches that run
// inside it, through a sorted proxy, and through a reset.
// Catalogue: docs/qt-edge-cases/proxies-selection.md.
#include "proxy_support.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QStringListModel>
#include <QtGui/QStandardItemModel>

#include <algorithm>
#include <functional>

namespace {
using namespace edge_proxy;
using ListRig = Rig<QList<QString>>;
using MapRig = Rig<QMap<int, QString>>;
using Flags = QItemSelectionModel::SelectionFlags;

constexpr int kRows = 8;

QStringList texts(int n) {
  QStringList out;
  for (int i = 0; i < n; ++i) out.append(QString("r%1").arg(i));
  return out;
}

// What a selection model says, to compare two of them.
struct Snapshot {
  bool currentValid = false;
  int currentRow = -1;
  QList<int> rows;                         // selectedRows, ascending
  QList<std::pair<int, int>> ranges;       // the ranges the selection holds
  bool hasSelection = false;
  int currentChanged = 0, currentRowChanged = 0, selectionChanged = 0;
  bool operator==(const Snapshot&) const = default;
};

struct Watch {
  explicit Watch(QItemSelectionModel& s) : selection(s) {
    QObject::connect(&s, &QItemSelectionModel::currentChanged, &s, [this] { ++snapshot.currentChanged; });
    QObject::connect(&s, &QItemSelectionModel::currentRowChanged, &s, [this] { ++snapshot.currentRowChanged; });
    QObject::connect(&s, &QItemSelectionModel::selectionChanged, &s, [this] { ++snapshot.selectionChanged; });
  }
  Snapshot take() {
    Snapshot s = snapshot;
    s.currentValid = selection.currentIndex().isValid();
    s.currentRow = selection.currentIndex().row();
    for (const QModelIndex& i : selection.selectedRows()) s.rows.append(i.row());
    std::sort(s.rows.begin(), s.rows.end());
    for (const QItemSelectionRange& r : selection.selection()) s.ranges.append({r.top(), r.bottom()});
    std::sort(s.ranges.begin(), s.ranges.end());  // the order Qt keeps its ranges in is not part of the contract
    s.hasSelection = selection.hasSelection();
    return s;
  }
  void reset_counts() { snapshot = {}; }
  QItemSelectionModel& selection;
  Snapshot snapshot;
};

// A change, applied to the list model under test and to QStringListModel.
struct Change {
  const char* name;
  std::function<void(ListRig&)> ours;
  std::function<void(QStringListModel&)> reference;
};

void insert_row_at(QStringListModel& m, int row, const QString& text) {
  m.insertRows(row, 1);
  m.setData(m.index(row), text);
}

std::vector<Change> changes() {
  return {
      {"insert before", [](ListRig& r) { r.source.insert(0, "new"); }, [](QStringListModel& m) { insert_row_at(m, 0, "new"); }},
      {"insert in the middle", [](ListRig& r) { r.source.insert(4, "new"); }, [](QStringListModel& m) { insert_row_at(m, 4, "new"); }},
      {"insert at the end", [](ListRig& r) { r.source.insert(kRows, "new"); }, [](QStringListModel& m) { insert_row_at(m, kRows, "new"); }},
      {"remove the first", [](ListRig& r) { r.source.remove(0); }, [](QStringListModel& m) { m.removeRows(0, 1); }},
      {"remove in the middle", [](ListRig& r) { r.source.remove(3); }, [](QStringListModel& m) { m.removeRows(3, 1); }},
      {"remove the last", [](ListRig& r) { r.source.remove(kRows - 1); }, [](QStringListModel& m) { m.removeRows(kRows - 1, 1); }},
      {"replace", [](ListRig& r) { r.source.replace(3, "other"); }, [](QStringListModel& m) { m.setData(m.index(3), "other"); }},
      {"move down", [](ListRig& r) { r.source.move(1, 5); }, [](QStringListModel& m) { m.moveRows({}, 1, 1, {}, 6); }},
      {"move up", [](ListRig& r) { r.source.move(6, 2); }, [](QStringListModel& m) { m.moveRows({}, 6, 1, {}, 2); }},
      {"insert a run", [](ListRig& r) { r.source.insertRange(3, QList<QString>{"a", "b", "c"}); },
       [](QStringListModel& m) {
         m.insertRows(3, 3);
         m.setData(m.index(3), "a");
         m.setData(m.index(4), "b");
         m.setData(m.index(5), "c");
       }},
      {"remove a run", [](ListRig& r) { r.source.removeRange(2, 4); }, [](QStringListModel& m) { m.removeRows(2, 4); }},
      {"reset", [](ListRig& r) { r.source.resetWith(QList<QString>{"x", "y", "z"}); }, [](QStringListModel& m) { m.setStringList({"x", "y", "z"}); }},
      {"reset to the same", [](ListRig& r) { r.source.resetWith(texts(kRows)); }, [](QStringListModel& m) { m.setStringList(texts(kRows)); }},
      {"clear", [](ListRig& r) { r.source.clear(); }, [](QStringListModel& m) { m.setStringList({}); }},  // clear is a reset of the model, not row removals
  };
}

// the same current index and selection, and the same signals, as QStringListModel, for every change,
// every current row and every selected range.
void a_list_selection_behaves_like_qts() {
  const int currents[] = {0, 3, 7};
  const std::pair<int, int> ranges[] = {{1, 4}, {5, 7}, {0, 7}, {3, 3}};
  for (const Change& change : changes()) {
    for (const int current : currents) {
      for (const auto& [top, bottom] : ranges) {
        ListRig ours;
        for (const QString& t : texts(kRows)) CHECK(ours.source.insert(ours.source.size(), t));
        QStringListModel reference(texts(kRows));
        QItemSelectionModel oursSelection(ours.model.get());
        QItemSelectionModel referenceSelection(&reference);
        Watch oursWatch(oursSelection);
        Watch referenceWatch(referenceSelection);
        for (auto [selection, model] : {std::pair<QItemSelectionModel*, QAbstractItemModel*>{&oursSelection, ours.model.get()},
                                        {&referenceSelection, &reference}}) {
          selection->setCurrentIndex(model->index(current, 0), QItemSelectionModel::NoUpdate);
          selection->select(QItemSelection(model->index(top, 0), model->index(bottom, 0)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        oursWatch.reset_counts();
        referenceWatch.reset_counts();
        change.ours(ours);
        change.reference(reference);
        CHECK(column(*ours.model) == column(reference));
        const Snapshot a = oursWatch.take();
        const Snapshot b = referenceWatch.take();
        if (!(a == b)) {
          std::fprintf(stderr, "%s, current %d, selected %d..%d: current %d/%d vs %d/%d, rows %lld vs %lld, signals %d,%d,%d vs %d,%d,%d\n", change.name,
                       current, top, bottom, a.currentValid, a.currentRow, b.currentValid, b.currentRow, static_cast<long long>(a.rows.size()),
                       static_cast<long long>(b.rows.size()), a.currentChanged, a.currentRowChanged, a.selectionChanged, b.currentChanged,
                       b.currentRowChanged, b.selectionChanged);
        }
        CHECK(a == b);
      }
    }
  }
}

// through a sorted proxy the same changes give the same selection as over QStringListModel: the selection is
// made in the proxy's rows, and compared in the proxy's rows.
void a_selection_through_a_sorted_proxy_behaves_like_qts() {
  const int currents[] = {0, 3, 7};
  const std::pair<int, int> ranges[] = {{1, 4}, {5, 7}, {0, 7}};
  for (const Change& change : changes()) {
    for (const int current : currents) {
      for (const auto& [top, bottom] : ranges) {
        ListRig ours;
        QStringList scrambled{"r5", "r1", "r7", "r0", "r3", "r6", "r2", "r4"};
        for (const QString& t : scrambled) CHECK(ours.source.insert(ours.source.size(), t));
        QStringListModel reference(scrambled);
        QSortFilterProxyModel oursProxy;
        QSortFilterProxyModel referenceProxy;
        oursProxy.setSourceModel(ours.model.get());
        referenceProxy.setSourceModel(&reference);
        for (QSortFilterProxyModel* p : {&oursProxy, &referenceProxy}) p->sort(0);
        QAbstractItemModelTester oursTester(&oursProxy, kFatal);
        QItemSelectionModel oursSelection(&oursProxy);
        QItemSelectionModel referenceSelection(&referenceProxy);
        Watch oursWatch(oursSelection);
        Watch referenceWatch(referenceSelection);
        for (auto [selection, model] : {std::pair<QItemSelectionModel*, QAbstractItemModel*>{&oursSelection, &oursProxy},
                                        {&referenceSelection, &referenceProxy}}) {
          selection->setCurrentIndex(model->index(current, 0), QItemSelectionModel::NoUpdate);
          selection->select(QItemSelection(model->index(top, 0), model->index(bottom, 0)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        oursWatch.reset_counts();
        referenceWatch.reset_counts();
        change.ours(ours);
        change.reference(reference);
        CHECK(column(oursProxy) == column(referenceProxy));
        CHECK(mapping_is_consistent(oursProxy));
        const Snapshot a = oursWatch.take();
        const Snapshot b = referenceWatch.take();
        if (!(a == b)) std::fprintf(stderr, "proxy: %s, current %d, selected %d..%d differs\n", change.name, current, top, bottom);
        CHECK(a == b);
      }
    }
  }
}

// a table model's selection (Rows, Columns, Toggle, Deselect, ClearAndSelect) over a range, under inserts, removals
// and a reset, is that of QStandardItemModel with the same two columns.
struct TableChange {
  const char* name;
  std::function<void(Rig<QList<QString>, true>&)> ours;
  std::function<void(QStandardItemModel&)> reference;
};

void put_row(QStandardItemModel& m, int row, const QString& text) {
  m.insertRow(row, QList<QStandardItem*>{new QStandardItem(text), new QStandardItem(text)});
}

void fill(QStandardItemModel& m, const QStringList& rows) {
  m.clear();
  m.setColumnCount(2);
  for (int i = 0; i < rows.size(); ++i) put_row(m, i, rows[i]);
}

void a_table_selection_behaves_like_qts() {
  const std::vector<TableChange> tableChanges{
      {"insert in the middle", [](auto& r) { r.source.insert(3, "n"); }, [](auto& m) { put_row(m, 3, "n"); }},
      {"insert at the top", [](auto& r) { r.source.insert(0, "n"); }, [](auto& m) { put_row(m, 0, "n"); }},
      {"remove in the middle", [](auto& r) { r.source.remove(3); }, [](auto& m) { m.removeRow(3); }},
      {"remove the first", [](auto& r) { r.source.remove(0); }, [](auto& m) { m.removeRow(0); }},
      {"insert a run", [](auto& r) { r.source.insertRange(2, QList<QString>{"a", "b"}); }, [](auto& m) {
         m.insertRows(2, 2);  // one announcement for the run, as the container's insertRange gives
         for (int col = 0; col < 2; ++col) {
           m.setItem(2, col, new QStandardItem("a"));
           m.setItem(3, col, new QStandardItem("b"));
         }
       }},
      {"remove a run", [](auto& r) { r.source.removeRange(1, 4); }, [](auto& m) { m.removeRows(1, 4); }},
      {"reset", [](auto& r) { r.source.resetWith(QList<QString>{"x", "y"}); }, [](auto& m) { fill(m, {"x", "y"}); }},
  };
  using F = QItemSelectionModel;
  struct Gesture {
    const char* name;
    std::function<void(F&, QAbstractItemModel&)> apply;
  };
  const std::vector<Gesture> gestures{
      {"rows of a cell range", [](F& s, QAbstractItemModel& m) { s.select(QItemSelection(m.index(1, 1), m.index(4, 1)), F::Select | F::Rows); }},
      {"a column", [](F& s, QAbstractItemModel& m) { s.select(m.index(2, 1), F::Select | F::Columns); }},
      {"toggle twice", [](F& s, QAbstractItemModel& m) {
         s.select(m.index(2, 0), F::Toggle | F::Rows);
         s.select(QItemSelection(m.index(2, 0), m.index(5, 0)), F::Toggle | F::Rows);
       }},
      {"select then deselect a row", [](F& s, QAbstractItemModel& m) {
         s.select(QItemSelection(m.index(0, 0), m.index(6, 1)), F::Select);
         s.select(m.index(3, 0), F::Deselect | F::Rows);
       }},
      {"clear and select", [](F& s, QAbstractItemModel& m) {
         s.select(m.index(1, 0), F::Select | F::Rows);
         s.select(m.index(5, 1), F::ClearAndSelect | F::Rows);
       }},
  };
  for (const TableChange& change : tableChanges) {
    for (const Gesture& gesture : gestures) {
      Rig<QList<QString>, true> ours;
      for (const QString& t : texts(kRows)) CHECK(ours.source.insert(ours.source.size(), t));
      QStandardItemModel reference;
      fill(reference, texts(kRows));
      QItemSelectionModel oursSelection(ours.model.get());
      QItemSelectionModel referenceSelection(&reference);
      Watch oursWatch(oursSelection);
      Watch referenceWatch(referenceSelection);
      for (auto [selection, model] : {std::pair<QItemSelectionModel*, QAbstractItemModel*>{&oursSelection, ours.model.get()},
                                      {&referenceSelection, &reference}}) {
        selection->setCurrentIndex(model->index(2, 1), QItemSelectionModel::NoUpdate);
        gesture.apply(*selection, *model);
      }
      oursWatch.reset_counts();
      referenceWatch.reset_counts();
      change.ours(ours);
      change.reference(reference);
      CHECK(column(*ours.model) == column(reference));
      const Snapshot a = oursWatch.take();
      const Snapshot b = referenceWatch.take();
      // The selected columns and cells, besides the rows.
      CHECK(oursSelection.selectedColumns(0).size() == referenceSelection.selectedColumns(0).size());
      CHECK(oursSelection.selectedIndexes().size() == referenceSelection.selectedIndexes().size());
      if (!(a == b)) {
        std::fprintf(stderr, "table: %s after %s differs\n", gesture.name, change.name);
        for (const Snapshot* sn : {&a, &b}) {
          std::fprintf(stderr, "  current %d/%d signals %d,%d,%d ranges:", sn->currentValid, sn->currentRow, sn->currentChanged, sn->currentRowChanged, sn->selectionChanged);
          for (const auto& [t, bt] : sn->ranges) std::fprintf(stderr, " %d-%d", t, bt);
          std::fprintf(stderr, "\n");
        }
      }
      CHECK(a == b);
    }
  }
}

QList<int> keys_of(QItemSelectionModel& selection, const QAbstractItemModel& model) {
  QList<int> out;
  for (const QModelIndex& i : selection.selectedRows()) out.append(model.data(i, kKeyRole).toInt());
  std::sort(out.begin(), out.end());
  return out;
}

MapRig& fill_map(MapRig& rig, std::initializer_list<int> keys) {
  for (const int k : keys) CHECK(rig.source.insert(k, QString::number(k)));
  return rig;
}

// a range selected in a map keeps its entries through a batch that runs inside it: the new entries are not
// selected, the old ones stay, and the removed ones leave the selection.
void a_selection_spanning_a_batch() {
  MapRig rig;
  fill_map(rig, {10, 20, 30, 40, 50, 60, 70, 80, 90});
  QItemSelectionModel selection(rig.model.get());
  selection.select(QItemSelection(rig.model->index(2, 0), rig.model->index(6, 0)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  selection.setCurrentIndex(rig.model->index(4, 0), QItemSelectionModel::NoUpdate);  // key 50
  CHECK(keys_of(selection, *rig.model) == QList<int>({30, 40, 50, 60, 70}));

  CHECK(rig.source.insertMany(QMap<int, QString>{{35, "35"}, {45, "45"}, {55, "55"}, {5, "5"}, {95, "95"}}) == 5);
  CHECK(keys_of(selection, *rig.model) == QList<int>({30, 40, 50, 60, 70}));
  CHECK(rig.model->data(selection.currentIndex(), kKeyRole).toInt() == 50);
  CHECK(selection.hasSelection());

  CHECK(rig.source.removeMany({40, 60, 10, 95}) == 4);
  CHECK(keys_of(selection, *rig.model) == QList<int>({30, 50, 70}));
  CHECK(rig.model->data(selection.currentIndex(), kKeyRole).toInt() == 50);

  // The current entry removed with others: the current index is invalid or a row that is there, never a stale row.
  CHECK(rig.source.removeMany({50, 20, 80}) == 3);
  CHECK(keys_of(selection, *rig.model) == QList<int>({30, 70}));
  const QModelIndex current = selection.currentIndex();
  CHECK(!current.isValid() || (current.row() >= 0 && current.row() < rig.model->rowCount()));

  CHECK(rig.source.replace(30, "changed") && keys_of(selection, *rig.model) == QList<int>({30, 70}));
  selection.clearSelection();
  CHECK(!selection.hasSelection() && selection.selectedRows().isEmpty());
  rig.source.resetWith(QMap<int, QString>{{1, "1"}});
  CHECK(!selection.currentIndex().isValid() && !selection.hasSelection());
}

// a hash and a set keep a selection by entry too: a row selected stays the same entry while others come and go.
template <class RigT, class Entries>
void a_selection_in_an_unordered_container(RigT& rig, Entries first, Entries more) {
  CHECK(rig.source.insertMany(first) == 5);
  QItemSelectionModel selection(rig.model.get());
  selection.select(QItemSelection(rig.model->index(1, 0), rig.model->index(3, 0)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  const QList<int> before = keys_of(selection, *rig.model);
  CHECK(before.size() == 3);
  CHECK(rig.source.insertMany(more) == 2);  // after the old rows: the selected rows keep their places
  CHECK(keys_of(selection, *rig.model) == before);
  CHECK(rig.source.remove(rig.model->index(0, 0).data(kKeyRole).toInt()));
  CHECK(keys_of(selection, *rig.model) == before);
  CHECK(rig.source.remove(before.first()));
  CHECK(keys_of(selection, *rig.model).size() == 2);
}

void selections_in_a_hash_and_a_set() {
  Rig<QHash<int, QString>> hash;
  a_selection_in_an_unordered_container(hash, QHash<int, QString>{{1, "1"}, {2, "2"}, {3, "3"}, {4, "4"}, {5, "5"}},
                                        QHash<int, QString>{{6, "6"}, {7, "7"}});
  Rig<QSet<int>> set;
  a_selection_in_an_unordered_container(set, QSet<int>{1, 2, 3, 4, 5}, QSet<int>{6, 7});
}

// select through a sorted proxy: the selection made in the proxy's rows maps to the right entries in the model
// (mapSelectionToSource) and back (mapSelectionFromSource), and it follows its entries when a replace re-sorts them.
void selecting_through_a_proxy() {
  MapRig rig;
  fill_map(rig, {1, 2, 3, 4, 5, 6});
  const QStringList texts{"d", "f", "b", "a", "e", "c"};
  for (int i = 0; i < 6; ++i) CHECK(rig.source.replace(i + 1, texts[i]));
  QSortFilterProxyModel proxy;
  proxy.setSourceModel(rig.model.get());
  proxy.setSortRole(Qt::DisplayRole);
  proxy.sort(0);
  QAbstractItemModelTester tester(&proxy, kFatal);
  QItemSelectionModel selection(&proxy);
  selection.select(QItemSelection(proxy.index(1, 0), proxy.index(3, 0)), QItemSelectionModel::Select | QItemSelectionModel::Rows);  // b c d
  const QItemSelection inSource = proxy.mapSelectionToSource(selection.selection());
  QSet<int> keys;
  for (const QModelIndex& i : inSource.indexes()) keys.insert(rig.model->data(i, kKeyRole).toInt());
  CHECK(keys == QSet<int>({1, 3, 6}));  // d, b, c
  QItemSelectionModel direct(rig.model.get());
  direct.select(inSource, QItemSelectionModel::Select);
  CHECK(proxy.mapSelectionFromSource(direct.selection()).indexes().size() == 3);
  CHECK(proxy.mapSelectionFromSource(inSource).indexes().size() == selection.selection().indexes().size());
  for (const QModelIndex& i : proxy.mapSelectionFromSource(inSource).indexes()) CHECK(selection.isSelected(i));

  CHECK(rig.source.replace(3, "z"));  // b -> z: moves from row 1 to the end
  CHECK(keys_of(selection, proxy) == QList<int>({1, 3, 6}));
  CHECK(column(proxy) == QStringList({"a", "c", "d", "e", "f", "z"}));
  CHECK(rig.source.removeMany({1, 2}) == 2);
  CHECK(keys_of(selection, proxy) == QList<int>({3, 6}));
  CHECK(mapping_is_consistent(proxy));
}

// the source reset under a proxy with a selection and a current index: the proxy resets, the selection and the
// current index are gone, and the new rows are not selected.
void a_reset_under_a_proxy_selection() {
  MapRig rig;
  fill_map(rig, {1, 2, 3, 4});
  QSortFilterProxyModel proxy;
  proxy.setSourceModel(rig.model.get());
  proxy.sort(0, Qt::DescendingOrder);
  QAbstractItemModelTester tester(&proxy, kFatal);
  QItemSelectionModel selection(&proxy);
  Watch watch(selection);
  selection.select(QItemSelection(proxy.index(0, 0), proxy.index(2, 0)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  selection.setCurrentIndex(proxy.index(1, 0), QItemSelectionModel::NoUpdate);
  CHECK(selection.hasSelection() && selection.currentIndex().isValid());
  rig.source.resetWith(QMap<int, QString>{{7, "7"}, {8, "8"}, {9, "9"}});
  CHECK(proxy.rowCount() == 3);
  CHECK(!selection.hasSelection() && !selection.currentIndex().isValid() && selection.selectedRows().isEmpty());
  // The selection model works again on the new rows.
  selection.select(proxy.index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  CHECK(keys_of(selection, proxy) == QList<int>({9}));
  CHECK(rig.source.clear());
  CHECK(!selection.hasSelection() && proxy.rowCount() == 0);
}

// a selection through a filter keeps what still passes: an edit that makes a selected row fail drops it from the
// selection, an edit that makes a row pass does not select it.
void a_selection_through_a_filter() {
  MapRig rig;
  fill_map(rig, {1, 2, 3, 4});
  for (int k = 1; k <= 4; ++k) CHECK(rig.source.replace(k, QString("a%1").arg(k)));
  QSortFilterProxyModel proxy;
  proxy.setSourceModel(rig.model.get());
  proxy.setFilterFixedString("a");
  QAbstractItemModelTester tester(&proxy, kFatal);
  QItemSelectionModel selection(&proxy);
  selection.select(QItemSelection(proxy.index(0, 0), proxy.index(2, 0)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  CHECK(keys_of(selection, proxy) == QList<int>({1, 2, 3}));
  CHECK(rig.source.replace(2, "b2"));  // selected, now fails
  CHECK(keys_of(selection, proxy) == QList<int>({1, 3}));
  CHECK(rig.source.replace(2, "a2"));  // passes again: not selected
  CHECK(keys_of(selection, proxy) == QList<int>({1, 3}));
  proxy.setFilterFixedString("zzz");  // nothing passes: the proxy resets its rows
  CHECK(proxy.rowCount() == 0 && !selection.hasSelection());
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  a_list_selection_behaves_like_qts();
  a_selection_through_a_sorted_proxy_behaves_like_qts();
  a_table_selection_behaves_like_qts();
  a_selection_spanning_a_batch();
  selections_in_a_hash_and_a_set();
  selecting_through_a_proxy();
  a_reset_under_a_proxy_selection();
  a_selection_through_a_filter();
  return rqt_test::finish("capability_edge_proxy_selection");
}
