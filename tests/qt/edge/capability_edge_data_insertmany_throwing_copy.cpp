// A batch is staged, then announced. A value whose copy throws (an allocation that fails inside a QString copy is the
// real case) is copied while the batch is staged, before "entriesAboutToBeAdded", so a throw at the first, a middle or
// the last entry leaves the container, the signals and the model exactly as they were. The same holds for takeMany,
// and resetWith, which takes its content by value, makes no copy of its own. The count of copies a batch makes is
// pinned: one per entry for insertMany and takeMany. See also capability_edge_data_values (editValue, snapshots).
// Catalogue: docs/qt-edge-cases/data-types.md (D-22, D-24, D-25).
#include "data_support.hpp"

#include <QtCore/QHash>
#include <QtCore/QMap>

#include <stdexcept>

namespace {
using edge_data::Rig;

constexpr int kBatch = 5;
constexpr int kPreset = 100;  // the key that is in the container before the batch
constexpr int kLastCopy = kBatch - 1;

struct Fragile {
  static inline int copiesLeft = -1;  // negative: copies are fine; 0: the next copy throws
  static inline int copies = 0;
  int v = 0;
  Fragile() = default;
  explicit Fragile(int v) : v(v) {}
  Fragile(const Fragile& other) : v(other.v) { check(); }
  Fragile(Fragile&& other) noexcept : v(other.v) {}
  Fragile& operator=(const Fragile& other) {
    v = other.v;
    check();
    return *this;
  }
  Fragile& operator=(Fragile&& other) noexcept {
    v = other.v;
    return *this;
  }
  static void check() {
    ++copies;
    if (copiesLeft == 0) throw std::runtime_error("copy");
    if (copiesLeft > 0) --copiesLeft;
  }
  static void disarm() { copiesLeft = -1; }
};

template <class C>
struct Probe {
  Rig<C> rig;
  int about = 0, done = 0;
  using Source = observable::ObservableContainer<C>;
  explicit Probe() {
    QObject::connect(&rig.source, &Source::entriesAboutToBeAdded, &rig.source, [this](const QList<int>&) { ++about; });
    QObject::connect(&rig.source, &Source::entriesAdded, &rig.source, [this](const QList<int>&) { ++done; });
    QObject::connect(&rig.source, &Source::entriesAboutToBeRemoved, &rig.source, [this](const QList<int>&) { ++about; });
    QObject::connect(&rig.source, &Source::entriesRemoved, &rig.source, [this](const QList<int>&) { ++done; });
    CHECK(rig.source.insert(kPreset, Fragile{kPreset}));
  }
  bool unchanged() const {
    return about == 0 && done == 0 && rig.source.size() == 1 && rig.source.contains(kPreset) &&
           rig.model.rowCount() == 1 && rig.keys() == QList<int>{kPreset};
  }
};

template <class C>
C batchOf() {
  C batch;
  for (int key = 1; key <= kBatch; ++key) batch.insert(key, Fragile{key});
  return batch;
}

template <class C>
void a_throwing_copy_in_insertMany_changes_nothing() {
  for (const int failAt : {0, kLastCopy / 2, kLastCopy}) {  // the first, a middle and the last copy
    Probe<C> p;
    const C batch = batchOf<C>();
    Fragile::copiesLeft = failAt;
    bool threw = false;
    try {
      p.rig.source.insertMany(batch);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    Fragile::disarm();
    CHECK(threw);
    CHECK(p.unchanged());
    // the container is intact: the same batch goes in when the copies work
    CHECK(p.rig.source.insertMany(batch) == kBatch);
    CHECK(p.about == 1 && p.done == 1 && p.rig.source.size() == kBatch + 1 && p.rig.model.rowCount() == kBatch + 1);
  }
}

template <class C>
void a_throwing_copy_in_takeMany_changes_nothing() {
  for (const int failAt : {0, kLastCopy / 2, kLastCopy}) {
    Probe<C> p;
    CHECK(p.rig.source.insertMany(batchOf<C>()) == kBatch);
    p.about = p.done = 0;
    Fragile::copiesLeft = failAt;
    bool threw = false;
    try {
      (void)p.rig.source.takeMany({1, 2, 3, 4, 5});
    } catch (const std::runtime_error&) {
      threw = true;
    }
    Fragile::disarm();
    CHECK(threw);
    CHECK(p.about == 0 && p.done == 0 && p.rig.source.size() == kBatch + 1 && p.rig.model.rowCount() == kBatch + 1);
    CHECK(p.rig.source.takeMany({1, 2, 3, 4, 5}).size() == kBatch);
    CHECK(p.about == 1 && p.done == 1 && p.rig.model.rowCount() == 1);
  }
}

// resetWith takes its content by value, so a copy that throws is the caller's, before the call; the call makes none.
template <class C>
void resetWith_makes_no_copy_and_a_caller_copy_that_throws_changes_nothing() {
  Probe<C> p;
  C content = batchOf<C>();
  Fragile::copiesLeft = 0;
  bool threw = false;
  try {
    C argument = content;  // the caller's copy of the argument: a Qt container shares until it is written to
    argument.detach();     // so the values are copied here
    p.rig.source.resetWith(std::move(argument));
  } catch (const std::runtime_error&) {
    threw = true;
  }
  Fragile::disarm();
  CHECK(threw);
  CHECK(p.unchanged());
  Fragile::copies = 0;
  p.rig.source.resetWith(std::move(content));
  CHECK(Fragile::copies == 0);
  CHECK(p.rig.source.size() == kBatch && p.rig.model.rowCount() == kBatch);
}

// Copies per batch: one per entry, made while staging.
template <class C>
void a_batch_copies_each_value_once() {
  Probe<C> p;
  const C batch = batchOf<C>();
  Fragile::copies = 0;
  CHECK(p.rig.source.insertMany(batch) == kBatch);
  CHECK(Fragile::copies == kBatch);
  Fragile::copies = 0;
  CHECK(p.rig.source.takeMany({1, 2, 3, 4, 5}).size() == kBatch);
  CHECK(Fragile::copies == kBatch);
}

template <class C>
void all() {
  a_throwing_copy_in_insertMany_changes_nothing<C>();
  a_throwing_copy_in_takeMany_changes_nothing<C>();
  resetWith_makes_no_copy_and_a_caller_copy_that_throws_changes_nothing<C>();
  a_batch_copies_each_value_once<C>();
}
}  // namespace

int main() {
  all<QMap<int, Fragile>>();
  all<QHash<int, Fragile>>();
  return rqt_test::finish("capability_edge_data_insertmany_throwing_copy");
}
