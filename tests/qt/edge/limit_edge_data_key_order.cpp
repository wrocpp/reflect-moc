// Pinned: a key type whose QMap order is not its operator<. QMap sorts with std::less<Key>, which a program may
// specialise for its own type; the model's sorted index of the keys searches with std::lower_bound, which uses
// operator<. When the two differ, the index is searched in an order it does not have: a key lands at the wrong
// row, and a removal announces and drops a row that is not the key's.
// A fix gives the index the order of std::less<Key> (or refuses a type where the two differ); this file then
// becomes capability_edge_data_key_order.
// Catalogue: docs/qt-edge-cases/data-types.md (D-14).
#include "data_support.hpp"

#include <functional>

namespace {
using edge_data::Rig;

struct Rank {
  int n = 0;
  friend bool operator<(const Rank& a, const Rank& b) { return a.n < b.n; }  // ascending
  friend bool operator==(const Rank&, const Rank&) = default;
};
}  // namespace

// QMap<Rank, V> lists the largest first.
template <>
struct std::less<Rank> {
  bool operator()(const Rank& a, const Rank& b) const { return a.n > b.n; }
};

namespace {
using Map = QMap<Rank, int>;

QList<int> ns(const QList<Rank>& keys) {
  QList<int> out;
  for (const Rank& r : keys) out.append(r.n);
  return out;
}

void the_model_does_not_follow_the_order_of_the_map() {
  Map prefilled;
  for (int n : {1, 2, 3}) prefilled.insert(Rank{n}, n);
  // The container really orders by std::less.
  CHECK(ns(prefilled.keys()) == QList<int>({3, 2, 1}));

  // Built from that content, the model's sorted copy is given keys in descending order: its "ascending" invariant
  // does not hold from the start.
  Rig<Map> rig(prefilled);
  CHECK(ns(rig.keys()) == QList<int>({3, 2, 1}));  // the rows follow the map for now

  // A new key is searched in that copy with operator<: 2 lands in the wrong place.
  // Rank 4 belongs before 3 in the map's order: its row should be 0.
  CHECK(rig.source.insert(Rank{4}, 4));
  CHECK(ns(rig.source.items().keys()) == QList<int>({4, 3, 2, 1}));
  CHECK(ns(rig.keys()) != QList<int>({4, 3, 2, 1}));
  CHECK(ns(rig.keys()) == QList<int>({3, 2, 1, 4}));  // lower_bound under operator< puts it at the end
}

// Started empty, the copy is ascending (operator<) while the map is descending: the rows disagree from the first
// two inserts on.
void an_empty_start_gives_the_opposite_order() {
  Rig<Map> rig;
  for (int n : {1, 3, 2}) CHECK(rig.source.insert(Rank{n}, n));
  CHECK(ns(rig.source.items().keys()) == QList<int>({3, 2, 1}));
  CHECK(ns(rig.keys()) == QList<int>({1, 2, 3}));
  CHECK(ns(rig.keys()) != ns(rig.source.items().keys()));
}

// A removal in the prefilled case drops the wrong key from the model's copy.
void a_removal_drops_the_wrong_row() {
  Map prefilled;
  for (int n : {1, 2, 3, 4, 5}) prefilled.insert(Rank{n}, n);
  Rig<Map> rig(prefilled);
  CHECK(rig.source.remove(Rank{5}));
  CHECK(ns(rig.source.items().keys()) == QList<int>({4, 3, 2, 1}));
  // Rank 5 was at row 0; rowOf found its row by operator< in a descending vector and erased another row.
  CHECK(ns(rig.keys()) != QList<int>({4, 3, 2, 1}));
  // A ghost row: the model still lists the removed key 5, and has dropped the row of key 1.
  CHECK(ns(rig.keys()) == QList<int>({5, 4, 3, 2}));
}
}  // namespace

int main() {
  the_model_does_not_follow_the_order_of_the_map();
  an_empty_start_gives_the_opposite_order();
  a_removal_drops_the_wrong_row();
  return rqt_test::finish("limit_edge_data_key_order");
}
