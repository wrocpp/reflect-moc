// Pinned: the rows of a hash that arrives whole depend on the hash's seed. A QHash or QSet has no insertion order of
// its own, so when content comes in at once (the container's constructor, resetWith, and the argument of
// insertMany) the model takes the keys in the order the hash iterates them, and that order changes with the
// process's random QHash seed. The same content shows its rows in a different order in two runs of a program.
// After that first fill the model follows insertion order, independent of the seed (capability_edge_data_hash_order).
// A fix orders a batch by something the caller controls (a sorted list of keys, or an ordered argument type); this
// file then becomes capability_edge_data_hash_seed_order.
// Catalogue: docs/qt-edge-cases/data-types.md (D-33).
#include "data_support.hpp"

#include <QtCore/QHashFunctions>

namespace {
using edge_data::Rig;
using observable::ObservableContainer;

constexpr int kKeys = 40;  // enough keys that two seeds iterate them differently

QHash<int, int> content() {
  QHash<int, int> out;
  for (int key = 0; key < kKeys; ++key) out.insert(key, key);
  return out;
}

// The rows of a model built from the same content under the deterministic seed and under a random one.
void the_first_fill_follows_the_seed() {
  QHashSeed::setDeterministicGlobalSeed();
  const QHash<int, int> a = content();
  QHash<int, int> b;
  for (int attempt = 0; attempt < 8 && (b.isEmpty() || b.keys() == a.keys()); ++attempt) {
    QHashSeed::resetRandomGlobalSeed();
    b = content();
  }
  QHashSeed::resetRandomGlobalSeed();
  CHECK(!b.isEmpty() && a.keys() != b.keys());  // the seed changed the hash's own order

  // The constructor takes the content as it iterates.
  Rig<QHash<int, int>> first(a);
  Rig<QHash<int, int>> second(b);
  CHECK(first.keys() == a.keys() && second.keys() == b.keys());
  CHECK(first.keys() != second.keys());  // same entries, different rows

  // resetWith and insertMany are the same.
  ObservableContainer<QHash<int, int>> source;
  observable::ContainerModel<QHash<int, int>> model(&source, [](const int&, const int&) { return QVariant(); });
  source.resetWith(b);
  QList<int> afterReset;
  for (int row = 0; row < model.rowCount(); ++row) afterReset.append(model.index(row, 0).data(Qt::UserRole).toInt());
  CHECK(afterReset == b.keys());
  source.clear();
  CHECK(source.insertMany(a) == kKeys);
  QList<int> afterBatch;
  for (int row = 0; row < model.rowCount(); ++row) afterBatch.append(model.index(row, 0).data(Qt::UserRole).toInt());
  CHECK(afterBatch == a.keys() && afterBatch != afterReset);
}
}  // namespace

int main() {
  the_first_fill_follows_the_seed();
  return rqt_test::finish("limit_edge_data_hash_seed_order");
}
