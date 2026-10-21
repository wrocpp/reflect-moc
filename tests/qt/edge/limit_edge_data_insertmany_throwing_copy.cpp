// Pinned: insertMany copies every value after it has announced the batch. A value whose copy throws (an allocation
// that fails inside a QString copy is the real case, and a QList<Big> value copies) leaves the container with
// the entries that were already inserted, "entriesAboutToBeAdded" emitted and "entriesAdded" never: the model,
// which acts at the "done" signal, still lists the old rows. editValue and takeMany copy before they announce and
// are safe (capability_edge_data_values).
// A fix copies the values into a staging container before announcing; this file then becomes
// capability_edge_data_insertmany_throwing_copy.
// Catalogue: docs/qt-edge-cases/data-types.md (D-24).
#include "data_support.hpp"

#include <stdexcept>

namespace {
using edge_data::Rig;

struct Fragile {
  static inline int copiesLeft = -1;  // negative: copies are fine; 0: the next copy throws
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
    if (copiesLeft == 0) throw std::runtime_error("copy");
    if (copiesLeft > 0) --copiesLeft;
  }
};

void a_throwing_copy_leaves_the_model_behind_the_container() {
  using Map = QMap<int, Fragile>;
  Rig<Map> rig;
  CHECK(rig.source.insert(10, Fragile{10}));
  QMap<int, Fragile> batch;
  for (int key = 1; key <= 3; ++key) batch.insert(key, Fragile{key});

  int about = 0;
  int done = 0;
  QObject::connect(&rig.source, &decltype(rig.source)::entriesAboutToBeAdded, &rig.source, [&](const QList<int>&) { ++about; });
  QObject::connect(&rig.source, &decltype(rig.source)::entriesAdded, &rig.source, [&](const QList<int>&) { ++done; });

  // Two copies per entry today; let the first entry through and fail on the second.
  Fragile::copiesLeft = 2;
  bool threw = false;
  try {
    rig.source.insertMany(batch);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  Fragile::copiesLeft = -1;
  CHECK(threw);
  CHECK(about == 1 && done == 0);                   // announced, never confirmed
  CHECK(rig.source.size() == 2);                    // key 1 went in before the throw
  CHECK(rig.source.contains(1) && !rig.source.contains(2));
  CHECK(rig.model.rowCount() == 1);                 // the model was never told
  CHECK(rig.model.rowCount() != rig.source.size()); // a ghost: the model lacks a row the container has
}
}  // namespace

int main() {
  a_throwing_copy_leaves_the_model_behind_the_container();
  return rqt_test::finish("limit_edge_data_insertmany_throwing_copy");
}
