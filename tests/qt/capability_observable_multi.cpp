// QMultiMap and QMultiHash behind the observable container and its model: an entry is a MultiKey, its key and
// its occurrence counted from the most recently inserted. Random inserts, removals and replacements of entries
// that share keys are checked against a plain list of rows, with a view mirror built from signals alone,
// QAbstractItemModelTester in fatal mode, a selection and a persistent index that must follow their entries, and
// a sorting proxy on top.
#include "observable_models_support.hpp"

#include <container_model.hpp>

#include <QtCore/QCoreApplication>
#include <QtTest/QAbstractItemModelTester>

#include <algorithm>
#include <cstdio>
#include <random>

namespace {
using observable_test::Sample;
constexpr int kKeys = 6;       // few keys, so most entries share one
constexpr int kSteps = 4000;
constexpr unsigned kSeeds[] = {1, 2, 3, 4, 5, 6};

template <class C>
struct Plain {
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  static constexpr bool sorted = Traits::order == observable::Order::sorted_multi;

  struct Row {
    int key;
    int value;
    int id;
  };
  std::vector<Row> rows;
  int nextId = 0;

  std::vector<std::size_t> rows_of(int key) const {  // ascending row
    std::vector<std::size_t> out;
    for (std::size_t r = 0; r < rows.size(); ++r)
      if (rows[r].key == key) out.push_back(r);
    return out;
  }
  // The row of entry (key, n): the n-th from the newest. A sorted container keeps the newest first, an unordered one last.
  std::optional<std::size_t> find(int key, qsizetype n) const {
    const auto of = rows_of(key);
    if (n < 0 || n >= static_cast<qsizetype>(of.size())) return std::nullopt;
    return sorted ? of[static_cast<std::size_t>(n)] : of[of.size() - 1 - static_cast<std::size_t>(n)];
  }
  qsizetype occurrence_of(std::size_t row) const {
    const auto of = rows_of(rows[row].key);
    const auto at = static_cast<qsizetype>(std::find(of.begin(), of.end(), row) - of.begin());
    return sorted ? at : static_cast<qsizetype>(of.size()) - 1 - at;
  }
  void insert(int key, int value) {
    auto at = rows.end();
    if (sorted) at = std::find_if(rows.begin(), rows.end(), [&](const Row& r) { return r.key >= key; });
    rows.insert(at, Row{key, value, nextId++});
  }
};

template <class C>
bool run(const char* name, unsigned seed, int& applied) {
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Source = observable::ObservableContainer<C>;
  using Model = observable::ContainerModel<C>;
  Source source;
  Model model(&source, [](const Key&, const Sample& s) { return QVariant(s.value); });
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  observable_test::ModelWatcher watcher(&model, false);
  QItemSelectionModel selection(&model);
  QSortFilterProxyModel proxy;
  proxy.setSortRole(Qt::DisplayRole);
  proxy.setSourceModel(&model);
  proxy.sort(0);
  observable_test::ModelWatcher proxyWatcher(&proxy, false);
  QPersistentModelIndex tracked;
  std::optional<int> trackedId;
  QSet<int> selectedIds;
  Plain<C> plain;
  std::mt19937 rng(seed);
  int mismatches = 0;
  int value = 1;
  const auto draw = [&](int low, int high) { return std::uniform_int_distribution<int>(low, high - 1)(rng); };
  const auto expect = [&](bool ok) { mismatches += !ok; };

  for (int step = 0; step < kSteps; ++step) {
    const int kind = draw(0, 100);
    const int key = draw(0, kKeys);
    const auto count = static_cast<qsizetype>(plain.rows_of(key).size());
    const qsizetype n = draw(-1, static_cast<int>(count) + 2);
    bool accepted = false;
    if (kind < 35) {
      const qsizetype occurrence = draw(0, 10) == 0 ? 1 : 0;  // a new entry is the newest: anything else is refused
      accepted = source.insert(Key{key, occurrence}, Sample{value});
      expect(accepted == (occurrence == 0));
      if (accepted) plain.insert(key, value++);
    } else if (kind < 60) {
      const auto row = plain.find(key, n);
      const std::optional<int> id = row ? std::optional<int>(plain.rows[*row].id) : std::nullopt;
      accepted = source.remove(Key{key, n});
      expect(accepted == row.has_value());
      if (accepted) {
        plain.rows.erase(plain.rows.begin() + static_cast<std::ptrdiff_t>(*row));
        selectedIds.remove(*id);
        if (trackedId == id) trackedId.reset();
      }
    } else if (kind < 80) {
      const auto row = plain.find(key, n);
      accepted = source.replace(Key{key, n}, Sample{value});
      expect(accepted == row.has_value());
      if (accepted) plain.rows[*row].value = value++;
    } else if (kind < 98) {
      if (!plain.rows.empty()) {
        const int row = draw(0, static_cast<int>(plain.rows.size()));
        const int id = plain.rows[static_cast<std::size_t>(row)].id;
        selection.select(model.index(row, 0), QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
        if (selectedIds.contains(id)) selectedIds.remove(id); else selectedIds.insert(id);
        tracked = model.index(row, 0);
        trackedId = id;
      }
    } else {
      accepted = source.clear();
      expect(accepted == !plain.rows.empty());
      if (accepted) {
        plain.rows.clear();
        selectedIds.clear();
        trackedId.reset();
      }
    }
    applied += accepted;

    watcher.check();
    proxyWatcher.check();
    expect(source.size() == static_cast<qsizetype>(plain.rows.size()));
    expect(model.rowCount() == static_cast<int>(plain.rows.size()));
    for (std::size_t r = 0; r < plain.rows.size(); ++r) {
      const QModelIndex index = model.index(static_cast<int>(r), 0);
      expect(index.data().toInt() == plain.rows[r].value);
      expect(!model.data(index, observable_test::kKeyRole).isValid());  // no stable key to offer
      // The entry (key, n) of this row is the one the container holds with that value.
      expect(source.value(Key{plain.rows[r].key, plain.occurrence_of(r)}).value == plain.rows[r].value);
    }
    QSet<int> expectedRows;
    for (std::size_t r = 0; r < plain.rows.size(); ++r)
      if (selectedIds.contains(plain.rows[r].id)) expectedRows.insert(static_cast<int>(r));
    QSet<int> selectedRows;
    for (const QModelIndex& index : selection.selectedRows()) selectedRows.insert(index.row());
    expect(selectedRows == expectedRows);
    if (trackedId) {
      const auto row = std::find_if(plain.rows.begin(), plain.rows.end(), [&](const auto& r) { return r.id == *trackedId; });
      expect(row != plain.rows.end() && tracked.isValid() && tracked.row() == row - plain.rows.begin());
    } else {
      expect(!tracked.isValid());
    }
    expect(proxy.rowCount() == model.rowCount());
  }
  if (!watcher.clean() || !proxyWatcher.clean() || mismatches) {
    std::fprintf(stderr, "%s seed %u: %d mismatches\n", name, seed, mismatches);
    for (const QString& v : watcher.violations().mid(0, 3)) std::fprintf(stderr, "  %s\n", qPrintable(v));
    for (const QString& v : proxyWatcher.violations().mid(0, 3)) std::fprintf(stderr, "  proxy: %s\n", qPrintable(v));
    return false;
  }
  return true;
}

template <class C>
bool stress(const char* name) {
  bool ok = true;
  int applied = 0;
  for (const unsigned seed : kSeeds) ok = run<C>(name, seed, applied) && ok;
  std::printf("%-12s %d changes applied\n", name, applied);
  return ok;
}

// The occurrence numbering the traits promise, against Qt's own values(key) order.
void occurrence_follows_values() {
  QMultiMap<int, int> map;
  QMultiHash<int, int> hash;
  for (int v = 1; v <= 4; ++v) {
    map.insert(7, v);
    hash.insert(7, v);
  }
  using MapTraits = observable::ContainerTraits<QMultiMap<int, int>>;
  using HashTraits = observable::ContainerTraits<QMultiHash<int, int>>;
  const QList<int> expected = map.values(7);  // most recently inserted first
  for (qsizetype n = 0; n < expected.size(); ++n) {
    CHECK(MapTraits::value(map, {7, n}) == expected[n]);
    CHECK(HashTraits::value(hash, {7, n}) == hash.values(7)[n]);
  }
  CHECK(MapTraits::value(map, {7, 0}) == 4 && HashTraits::value(hash, {7, 0}) == 4);  // the newest first
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  occurrence_follows_values();
  CHECK((stress<QMultiMap<int, Sample>>("QMultiMap")));
  CHECK((stress<QMultiHash<int, Sample>>("QMultiHash")));
  return rqt_test::finish("capability_observable_multi");
}
