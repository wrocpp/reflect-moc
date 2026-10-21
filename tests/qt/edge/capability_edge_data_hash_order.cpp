// Data edge cases, hashes, sets and multi hashes: the model lists a QHash or QSet in the order entries were added,
// whatever the hash does with its buckets, its seed or its capacity.
// Catalogue: docs/qt-edge-cases/data-types.md (D-31 to D-36).
#include "data_support.hpp"
#include "observable_models_support.hpp"

#include <QtCore/QHashFunctions>
#include <QtCore/QString>

#include <random>
#include <vector>

namespace {
using edge_data::Rig;
using observable::MultiKey;
using observable::ObservableContainer;

constexpr int kEntries = 3000;      // enough to rehash a QHash many times (it doubles its buckets)
constexpr int kSeedChanges = 5;     // the first run uses the deterministic seed, the others a new random one
constexpr int kSteps = 1500;
constexpr unsigned kDriverSeed = 11;

// D-31: through growth, rehash and removals, the rows are the entries in the order they were added.
template <class C, class K>
void order_survives_rehash(auto make_key) {
  Rig<C> rig;
  std::vector<K> expected;
  std::mt19937 rng(3);
  for (int i = 0; i < kEntries; ++i) {
    const K key = make_key(static_cast<int>(rng() % 100000));
    if (rig.source.insert(key, {})) expected.push_back(key);
    if (i % 4 == 3 && !expected.empty()) {  // remove one now and then: dead slots, then a compaction
      const std::size_t at = rng() % expected.size();
      CHECK(rig.source.remove(expected[at]));
      expected.erase(expected.begin() + static_cast<std::ptrdiff_t>(at));
    }
  }
  CHECK(rig.model.rowCount() == static_cast<int>(expected.size()));
  CHECK(rig.keys() == QList<K>(expected.begin(), expected.end()));
  // The container's own order is not the model's: it is the hash's.
  CHECK(observable::ContainerTraits<C>::keys(rig.source.items()) != rig.keys());
}

void rows_follow_insertion_through_rehash() {
  order_survives_rehash<QHash<int, int>, int>([](int n) { return n; });
  order_survives_rehash<QHash<QString, int>, QString>([](int n) { return QString::number(n); });
  order_survives_rehash<QSet<int>, int>([](int n) { return n; });
}

// D-32: the global seed of QHash (deterministic, then new random ones) changes the iteration order of every hash made afterwards. The random driver, which
// compares the model with a reference after each of thousands of changes, gives the same clean result under every seed.
template <class C>
void driver_is_clean_under_every_seed() {
  using Source = ObservableContainer<C>;
  for (int change = 0; change < kSeedChanges; ++change) {
    if (change == 0)
      QHashSeed::setDeterministicGlobalSeed();
    else
      QHashSeed::resetRandomGlobalSeed();
    const auto report = observable_test::run_random<C, Source>(kDriverSeed, kSteps, [](Source* source) {
      const auto shown = [](const auto& key, const auto& value) { return QVariant(observable_test::Make<C>::shown(key, value)); };
      return std::make_unique<observable::ContainerModel<C>>(source, shown);
    });
    CHECK(report.clean());
    CHECK(report.applied > 0);
  }
  QHashSeed::resetRandomGlobalSeed();
}

void results_do_not_depend_on_the_hash_seed() {
  driver_is_clean_under_every_seed<QHash<unsigned, observable_test::Sample>>();
  driver_is_clean_under_every_seed<QSet<unsigned>>();
}

// D-34: a hash that was given room first behaves the same: reserve changes buckets, not rows.
void a_reserved_hash_keeps_insertion_order() {
  QHash<int, int> room;
  room.reserve(100000);
  Rig<QHash<int, int>> rig(std::move(room));
  std::vector<int> expected;
  for (int i = 0; i < 500; ++i) {
    const int key = (i * 7919) % 1009;
    if (rig.source.insert(key, i)) expected.push_back(key);
  }
  CHECK(rig.keys() == QList<int>(expected.begin(), expected.end()));
  CHECK(rig.source.items().capacity() >= 100000);
}

// D-35: a set holds an element once: a repeat is refused with nothing announced, in a single insert and in a batch.
void a_set_refuses_a_repeat() {
  Rig<QSet<QString>> rig;
  int announced = 0;
  QObject::connect(&rig.source, &decltype(rig.source)::entryAdded, &rig.source, [&](const QString&) { ++announced; });
  CHECK(rig.source.insert("a") && !rig.source.insert("a") && announced == 1);
  QSet<QString> more{"a", "b", "c"};
  CHECK(rig.source.insertMany(more) == 2);
  CHECK(rig.model.rowCount() == 3);
  CHECK(rig.source.insertMany(more) == 0);
  CHECK(!rig.source.replace("a", "z"));  // a set has no value to replace
  CHECK(!rig.source.editValue("a", [](QString&) {}));
  CHECK(rig.source.removeMany({"a", "a", "q"}) == 1 && rig.model.rowCount() == 2);
}

// D-36: a QMultiHash keeps duplicates: the same key again, even with the same value, is a new row; the entries of
// a key are numbered from the newest, the rows are in the order added, and an occurrence that is not there is refused.
void a_multi_hash_keeps_duplicates() {
  using C = QMultiHash<int, int>;
  using Source = ObservableContainer<C>;
  Source source;
  observable::ContainerModel<C> model(&source, [](const MultiKey<int>&, const int& value) { return QVariant(value); });
  QAbstractItemModelTester tester(&model, edge_data::kFatal);
  const auto shown = [&] {
    QList<int> out;
    for (int row = 0; row < model.rowCount(); ++row) out.append(model.index(row, 0).data().toInt());
    return out;
  };
  CHECK(source.insert({1, 0}, 10) && source.insert({1, 0}, 10) && source.insert({1, 0}, 30) && source.insert({2, 0}, 40));
  CHECK(source.size() == 4 && model.rowCount() == 4);
  CHECK(shown() == QList<int>({10, 10, 30, 40}));
  CHECK(!source.insert({1, 2}, 99));          // a new entry is the newest: only occurrence 0
  CHECK(!source.contains({1, 3}) && !source.remove({1, 3}) && !source.replace({1, 3}, 5));
  CHECK(source.value({1, 0}) == 30 && source.value({1, 2}) == 10);  // occurrence 0 is the newest
  CHECK(source.remove({1, 1}));               // the middle one: a duplicate value 10 goes
  CHECK(shown() == QList<int>({10, 30, 40}));
  CHECK(source.replace({1, 1}, 11) && shown() == QList<int>({11, 30, 40}));
  CHECK(source.insertMany(C{}) == 0 && source.removeMany({}) == 0);  // batches refuse a multi container
}
}  // namespace

int main() {
  rows_follow_insertion_through_rehash();
  results_do_not_depend_on_the_hash_seed();
  a_reserved_hash_keeps_insertion_order();
  a_set_refuses_a_repeat();
  a_multi_hash_keeps_duplicates();
  return rqt_test::finish("capability_edge_data_hash_order");
}
