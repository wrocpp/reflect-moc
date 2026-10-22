// Pinned: a NaN key. QMap<double> orders by std::less, under which NaN is equivalent to every key, and QHash<double>
// and QSet<double> compare with ==, under which NaN equals nothing, itself included. The library does not refuse
// the key, so the container's answers (contains, remove, replace) stop matching the keys.
//   map:  a NaN entry is found by every key and removed by every key; once it is in, nothing else gets in
//   hash: every insert(NaN) adds another entry, and none of them can be found, replaced or removed again
// Both stay consistent as far as the model goes: the rows follow the container's size and a removal leaves no
// ghost row. A fix refuses a NaN key in insert; this file then becomes capability_edge_data_nan_keys (refused).
// Catalogue: docs/qt-edge-cases/data-types.md (D-03, D-04).
#include "data_support.hpp"

#include <cmath>
#include <limits>

namespace {
using edge_data::Rig;
const double kNaN = std::numeric_limits<double>::quiet_NaN();

// D-03: NaN in a QMap<double>.
void a_nan_key_in_a_map_matches_every_key() {
  Rig<QMap<double, int>> map;
  CHECK(map.source.insert(kNaN, 1));
  CHECK(map.model.rowCount() == 1);
  // The entry answers to any key, so no other key can be inserted.
  CHECK(map.source.contains(5.0) && map.source.contains(-1e300));
  CHECK(!map.source.insert(1.0, 2));
  CHECK(map.source.size() == 1);
  CHECK(map.source.value(7.0) == 1);
  // replace(5.0) changes the NaN entry, and announces the key 5.0 that the model has no row for by name.
  CHECK(map.source.replace(5.0, 9));
  CHECK(map.source.value(kNaN) == 9);
  CHECK(std::isnan(map.keys().first()));
  // and remove(7.0) removes it.
  CHECK(map.source.remove(7.0));
  CHECK(map.source.size() == 0 && map.model.rowCount() == 0);

  // With entries in, a NaN key is refused: contains(NaN) is true for any non-empty map.
  Rig<QMap<double, int>> filled;
  CHECK(filled.source.insert(1.0, 1) && filled.source.insert(2.0, 2));
  CHECK(!filled.source.insert(kNaN, 3));
  CHECK(filled.source.size() == 2 && filled.model.rowCount() == 2);
}

// D-04: NaN in a QHash<double> and a QSet<double>.
void a_nan_key_in_a_hash_cannot_be_found_again() {
  Rig<QHash<double, int>> hash;
  CHECK(hash.source.insert(kNaN, 1));
  CHECK(hash.source.insert(kNaN, 2));  // not a duplicate: NaN != NaN
  CHECK(hash.source.insert(1.0, 3));
  CHECK(hash.source.size() == 3 && hash.model.rowCount() == 3);
  CHECK(!hash.source.contains(kNaN));
  CHECK(!hash.source.remove(kNaN));
  CHECK(!hash.source.replace(kNaN, 5));
  CHECK(hash.source.size() == 3);
  // Only a clear or a reset gets rid of them.
  CHECK(hash.source.clear() && hash.model.rowCount() == 0);

  Rig<QSet<double>> set;
  CHECK(set.source.insert(kNaN) && set.source.insert(kNaN));
  CHECK(set.source.size() == 2 && set.model.rowCount() == 2);
  CHECK(!set.source.remove(kNaN));
  CHECK(set.source.size() == 2);
}
}  // namespace

int main() {
  a_nan_key_in_a_map_matches_every_key();
  a_nan_key_in_a_hash_cannot_be_found_again();
  return rqt_test::finish("limit_edge_data_nan_keys");
}
