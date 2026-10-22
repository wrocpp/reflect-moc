// Random changes through a QSortFilterProxyModel, with more seeds than the stress test and with every sort role:
// the display role, the key role, a filter on the display role, and dynamicSortFilter on and off. The container's
// whole interface is used (insert, remove, replace, editValue, move, ranges, batches, reset, clear), and a selection
// model sits on the proxy. After every step the proxy must be a consistent view of the model, show the rows that pass
// the filter in the order of the sort role (when it re-sorts on its own), and a view built from its signals alone
// (ModelWatcher) must agree with it. QAbstractItemModelTester runs in fatal mode on the model and the proxy.
// Catalogue: docs/qt-edge-cases/proxies-selection.md.
#include "observable_models_support.hpp"
#include "proxy_support.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QConcatenateTablesProxyModel>
#include <QtCore/QTransposeProxyModel>
#include <QtCore/QRegularExpression>

#include <algorithm>
#include <cstdio>
#include <random>

namespace {
using namespace edge_proxy;

constexpr int kSteps = 1200;
constexpr unsigned kSeeds[] = {101, 102, 103, 104, 105, 106, 107, 108, 109, 110};
constexpr int kKeyDomain = 24;
constexpr int kLongestBatch = 6;
constexpr int kKinds = 100;
constexpr int kWeightInsert = 22;
constexpr int kWeightRemove = 14;
constexpr int kWeightReplace = 10;
constexpr int kWeightEdit = 8;
constexpr int kWeightMove = 8;
constexpr int kWeightInsertMany = 12;
constexpr int kWeightRemoveMany = 10;
constexpr int kWeightSelect = 8;
constexpr int kWeightReset = 3;  // the rest is clear
constexpr int kInvalidateEvery = 10;  // a static proxy is brought up to date this often

struct Config {
  int sortRole = Qt::DisplayRole;
  bool dynamic = true;
  bool filter = false;
  int column = 0;  // the column that is sorted and filtered, for a table
};

bool passes(const Config& c, const QString& text) { return !c.filter || text.contains(QRegularExpression("[02468]$")); }

template <class C, bool Table>
int run(unsigned seed, const Config& config) {
  using S = Setup<C>;
  using Key = typename S::Key;
  using Traits = typename S::Traits;
  Rig<C, Table> rig;
  QSortFilterProxyModel proxy;
  proxy.setSourceModel(rig.model.get());
  proxy.setSortRole(config.sortRole);
  proxy.setDynamicSortFilter(config.dynamic);
  proxy.setFilterKeyColumn(config.column);
  if (config.filter) proxy.setFilterRegularExpression("[02468]$");
  proxy.sort(config.column);
  QAbstractItemModelTester tester(&proxy, kFatal);
  observable_test::ModelWatcher watcher(&proxy, !S::isList);
  QItemSelectionModel selection(&proxy);
  std::mt19937 rng(seed);
  const auto draw = [&](int low, int high) { return std::uniform_int_distribution<int>(low, high - 1)(rng); };
  int nextValue = 1;
  int problems = 0;
  const auto expect = [&](bool ok, const char* what, int step) {
    if (ok) return;
    ++problems;
    if (problems <= 3) std::fprintf(stderr, "seed %u step %d: %s\n", seed, step, what);
  };

  const auto random_key = [&]() -> Key {
    if constexpr (S::isList)
      return static_cast<Key>(draw(-1, static_cast<int>(rig.source.size()) + 2));
    else
      return static_cast<Key>(draw(0, kKeyDomain));
  };
  const auto fresh = [&] { return QString::number(nextValue++); };
  const auto content = [&](int count) {
    C out;
    for (int i = 0; i < count; ++i) {
      if constexpr (S::isList) {
        Traits::insert(out, Traits::size(out), S::value(nextValue++, 0));
      } else {
        const Key k = static_cast<Key>(draw(0, kKeyDomain));
        if (Traits::can_insert(out, k)) Traits::insert(out, k, S::value(nextValue++, k));
      }
    }
    return out;
  };

  for (int step = 0; step < kSteps; ++step) {
    const int kind = draw(0, kKinds);
    const Key key = random_key();
    int edge = 0;
    const auto at = [&](int weight) { return kind < (edge += weight); };
    if (at(kWeightInsert)) {
      if constexpr (S::isList)
        rig.source.insert(key, fresh());
      else
        rig.source.insert(key, S::value(nextValue++, key));
    } else if (at(kWeightRemove)) {
      rig.source.remove(key);
    } else if (at(kWeightReplace)) {
      rig.source.replace(key, S::value(nextValue++, key));
    } else if (at(kWeightEdit)) {
      QList<int> roles;
      for (int i = draw(0, 3); i > 0; --i) roles.append(Qt::UserRole + draw(0, 3));
      rig.source.editValue(key, [&](typename S::Value& v) { v = S::value(nextValue++, key); }, roles);
    } else if (at(kWeightMove)) {
      rig.source.move(key, random_key());
    } else if (at(kWeightInsertMany)) {
      const C entries = content(draw(1, kLongestBatch + 1));
      if constexpr (S::isList)
        rig.source.insertRange(static_cast<Key>(draw(0, static_cast<int>(rig.source.size()) + 1)), QList<typename S::Value>(entries.begin(), entries.end()));
      else
        rig.source.insertMany(entries);
    } else if (at(kWeightRemoveMany)) {
      if constexpr (S::isList) {
        rig.source.removeRange(key, draw(1, kLongestBatch + 1));
      } else {
        QList<Key> keys;
        for (int i = draw(1, kLongestBatch + 1); i > 0; --i) keys.append(random_key());
        rig.source.removeMany(keys);
      }
    } else if (at(kWeightSelect)) {
      if (proxy.rowCount() > 0)
        selection.select(proxy.index(draw(0, proxy.rowCount()), 0), QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
    } else if (at(kWeightReset)) {
      rig.source.resetWith(content(draw(0, kLongestBatch + 1)));
    } else {
      rig.source.clear();
    }

    watcher.check();
    expect(watcher.clean(), "the proxy's signals and its rows disagree", step);
    expect(mapping_is_consistent(proxy), "the proxy's mapping is not consistent", step);
    // Selected rows are rows of the proxy, and map to rows of the model.
    for (const QModelIndex& selected : selection.selectedIndexes())
      expect(selected.isValid() && selected.row() < proxy.rowCount() && proxy.mapToSource(selected).isValid(), "a stale selected index", step);

    const bool strict = config.dynamic || step % kInvalidateEvery == 0;
    if (!config.dynamic && step % kInvalidateEvery == 0) proxy.invalidate();
    if (strict) {
      QStringList expected;  // the sort-role values of the rows that pass, as a multiset
      for (int row = 0; row < rig.model->rowCount(); ++row)
        if (passes(config, rig.model->index(row, 0).data().toString())) expected.append(rig.model->index(row, 0).data(config.sortRole).toString());
      QStringList actual;
      for (int row = 0; row < proxy.rowCount(); ++row) actual.append(proxy.index(row, config.column).data(config.sortRole).toString());
      QStringList sortedActual = actual;
      expected.sort();
      sortedActual.sort();
      expect(expected == sortedActual, "the proxy does not list exactly the rows that pass", step);
      if (config.sortRole == Qt::DisplayRole || !S::isList) {
        for (int row = 1; row < proxy.rowCount(); ++row) {
          const QVariant a = proxy.index(row - 1, config.column).data(config.sortRole);
          const QVariant b = proxy.index(row, config.column).data(config.sortRole);
          const bool inOrder = config.sortRole == kKeyRole ? a.toInt() <= b.toInt() : a.toString() <= b.toString();
          expect(inOrder, "the proxy is out of order", step);
        }
      }
    }
  }
  watcher.check();
  return problems;
}

template <class C, bool Table>
bool every_config(const char* name) {
  int problems = 0;
  for (const unsigned seed : kSeeds)
    for (const bool dynamic : {true, false})
      for (const bool filter : {false, true}) {
        problems += run<C, Table>(seed, {Qt::DisplayRole, dynamic, filter, 0});
        problems += run<C, Table>(seed, {kKeyRole, dynamic, filter});
        if constexpr (Table) problems += run<C, Table>(seed, {Qt::DisplayRole, dynamic, filter, 1});
      }
  std::printf("%-10s %-5s %d problems\n", name, Table ? "table" : "list", problems);
  return problems == 0;
}

// the other proxies through random changes, a list table and a map table side by side: an identity proxy and a
// transposed proxy over the list (which has moves, runs, and resets), a concatenation of the two, and a sort over a
// filter over the map. Each is compared with its sources cell by cell after every change.
using ListTable = Rig<QList<QString>, true>;
using MapTable = Rig<QMap<int, QString>, true>;

std::vector<QString> cells(const QAbstractItemModel& m) {
  std::vector<QString> out;
  for (int r = 0; r < m.rowCount(); ++r)
    for (int c = 0; c < m.columnCount(); ++c) out.push_back(m.index(r, c).data().toString());
  return out;
}

int other_proxies(unsigned seed) {
  ListTable list;
  MapTable map;
  QIdentityProxyModel identity;
  identity.setSourceModel(list.model.get());
  QTransposeProxyModel transpose;
  transpose.setSourceModel(list.model.get());
  QConcatenateTablesProxyModel concat;
  concat.addSourceModel(list.model.get());
  concat.addSourceModel(map.model.get());
  QSortFilterProxyModel filter;
  filter.setSourceModel(map.model.get());
  filter.setFilterRegularExpression("[02468]$");
  QSortFilterProxyModel sorted;
  sorted.setSourceModel(&filter);
  sorted.sort(0);
  QAbstractItemModelTester t1(&identity, kFatal), t2(&transpose, kFatal), t3(&concat, kFatal), t4(&filter, kFatal), t5(&sorted, kFatal);
  QItemSelectionModel selection(&concat);

  std::mt19937 rng(seed);
  const auto draw = [&](int low, int high) { return std::uniform_int_distribution<int>(low, high - 1)(rng); };
  int next = 1;
  int problems = 0;
  const auto fresh = [&] { return QString::number(next++); };
  for (int step = 0; step < kSteps; ++step) {
    const int kind = draw(0, 100);
    const int size = static_cast<int>(list.source.size());
    const int key = draw(0, kKeyDomain);
    if (kind < 12) list.source.insert(draw(-1, size + 2), fresh());
    else if (kind < 20) list.source.remove(draw(-1, size + 2));
    else if (kind < 26) list.source.replace(draw(-1, size + 2), fresh());
    else if (kind < 30) list.source.editValue(draw(0, size + 1), [&](QString& v) { v = fresh(); }, {Qt::DisplayRole});
    else if (kind < 38) list.source.move(draw(-1, size + 2), draw(-1, size + 2));
    else if (kind < 44) list.source.insertRange(draw(0, size + 1), QList<QString>{fresh(), fresh(), fresh()});
    else if (kind < 49) list.source.removeRange(draw(0, size + 1), draw(1, 4));
    else if (kind < 51) list.source.resetWith(QList<QString>{fresh(), fresh()});
    else if (kind < 52) list.source.clear();
    else if (kind < 60) map.source.insert(key, fresh());
    else if (kind < 66) map.source.remove(key);
    else if (kind < 72) map.source.replace(key, fresh());
    else if (kind < 80) {
      QMap<int, QString> batch;
      for (int i = draw(1, kLongestBatch + 1); i > 0; --i) batch.insert(draw(0, kKeyDomain), fresh());
      map.source.insertMany(batch);
    } else if (kind < 87) {
      QList<int> keys;
      for (int i = draw(1, kLongestBatch + 1); i > 0; --i) keys.append(draw(0, kKeyDomain));
      map.source.removeMany(keys);
    } else if (kind < 89) {
      QMap<int, QString> content;
      for (int i = draw(0, kLongestBatch + 1); i > 0; --i) content.insert(draw(0, kKeyDomain), fresh());
      map.source.resetWith(content);
    } else if (kind < 90) map.source.clear();
    else if (concat.rowCount() > 0) selection.select(concat.index(draw(0, concat.rowCount()), 0), QItemSelectionModel::Toggle | QItemSelectionModel::Rows);

    const auto expect = [&](bool ok, const char* what) {
      if (ok) return;
      ++problems;
      if (problems <= 3) std::fprintf(stderr, "other proxies, seed %u step %d: %s\n", seed, step, what);
    };
    expect(cells(identity) == cells(*list.model) && mapping_is_consistent(identity), "identity");
    bool transposed = transpose.rowCount() == 2 && transpose.columnCount() == list.model->rowCount();
    for (int r = 0; transposed && r < transpose.rowCount(); ++r)
      for (int c = 0; c < transpose.columnCount(); ++c)
        transposed = transposed && transpose.index(r, c).data() == list.model->index(c, r).data() && transpose.mapToSource(transpose.index(r, c)) == list.model->index(c, r);
    expect(transposed, "transpose");
    std::vector<QString> both = cells(*list.model);
    const std::vector<QString> second = cells(*map.model);
    both.insert(both.end(), second.begin(), second.end());
    bool concatMapped = true;
    for (int r = 0; r < concat.rowCount(); ++r) {
      const QModelIndex p = concat.index(r, 0);
      const QModelIndex q = concat.mapToSource(p);
      concatMapped = concatMapped && q.isValid() && concat.mapFromSource(q) == p;
    }
    expect(cells(concat) == both && concatMapped, "concatenation");
    for (const QModelIndex& i : selection.selectedIndexes()) expect(i.isValid() && i.row() < concat.rowCount(), "stale selection in the concatenation");
    QStringList expected;
    for (int row = 0; row < map.model->rowCount(); ++row)
      if (passes({Qt::DisplayRole, true, true}, map.model->index(row, 0).data().toString())) expected.append(map.model->index(row, 0).data().toString());
    expected.sort();
    expect(column(sorted) == expected && mapping_is_consistent(sorted), "sort over filter");
  }
  return problems;
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  CHECK((every_config<QList<QString>, false>("QList")));
  CHECK((every_config<QList<QString>, true>("QList")));
  CHECK((every_config<QMap<int, QString>, false>("QMap")));
  CHECK((every_config<QMap<int, QString>, true>("QMap")));
  CHECK((every_config<QHash<int, QString>, false>("QHash")));
  CHECK((every_config<QHash<int, QString>, true>("QHash")));
  CHECK((every_config<QSet<int>, false>("QSet")));
  CHECK((every_config<QSet<int>, true>("QSet")));
  int other = 0;
  for (const unsigned seed : kSeeds) other += other_proxies(seed);
  std::printf("other proxies %d problems\n", other);
  CHECK(other == 0);
  return rqt_test::finish("capability_edge_proxy_random");
}
