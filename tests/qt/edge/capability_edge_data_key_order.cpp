// Data edge cases, a key type whose QMap order is not its operator<. QMap sorts with std::less<Key>, which a program
// may specialise for its own type; the model's sorted index searches with std::less<Key> too, so its rows follow the
// container for a prefilled map, after every insert and removal, in a batch and through a reset, and for a multi map.
// Catalogue: docs/qt-edge-cases/data-types.md (D-14).
#include "data_support.hpp"

#include <functional>
#include <random>

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

void rows_follow_the_order_of_the_map() {
  Map prefilled;
  for (int n : {1, 2, 3}) prefilled.insert(Rank{n}, n);
  CHECK(ns(prefilled.keys()) == QList<int>({3, 2, 1}));  // the container orders by std::less
  Rig<Map> rig(prefilled);
  CHECK(ns(rig.keys()) == QList<int>({3, 2, 1}));
  CHECK(rig.source.insert(Rank{4}, 4));  // belongs first in the map's order
  CHECK(ns(rig.keys()) == QList<int>({4, 3, 2, 1}) && ns(rig.keys()) == ns(rig.source.items().keys()));
  CHECK(rig.source.insert(Rank{0}, 0) && rig.source.insert(Rank{2 * 1}, 9) == false);
  CHECK(ns(rig.keys()) == QList<int>({4, 3, 2, 1, 0}));

  Map five;
  for (int n : {1, 2, 3, 4, 5}) five.insert(Rank{n}, n);
  Rig<Map> removal(five);
  CHECK(removal.source.remove(Rank{5}));
  CHECK(ns(removal.keys()) == QList<int>({4, 3, 2, 1}));  // the row of key 5 went, not another
  CHECK(removal.source.remove(Rank{2}));
  CHECK(ns(removal.keys()) == QList<int>({4, 3, 1}));

  Rig<Map> started_empty;
  for (int n : {1, 3, 2}) CHECK(started_empty.source.insert(Rank{n}, n));
  CHECK(ns(started_empty.keys()) == QList<int>({3, 2, 1}));
}

// The same through batches, a reset and a random run: the model lists what QMap::keys() lists, after every step.
void random_changes_keep_the_order() {
  Rig<Map> rig;
  std::mt19937 rng(21);
  const auto draw = [&](int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); };
  for (int step = 0; step < 600; ++step) {
    switch (draw(6)) {
      case 0: rig.source.insert(Rank{draw(20)}, step); break;
      case 1: rig.source.remove(Rank{draw(20)}); break;
      case 2: {
        Map batch;
        for (int i = draw(5) + 1; i > 0; --i) batch.insert(Rank{draw(20)}, step);
        rig.source.insertMany(batch);
        break;
      }
      case 3: {
        QList<Rank> keys;
        for (int i = draw(5) + 1; i > 0; --i) keys.append(Rank{draw(20)});
        rig.source.removeMany(keys);
        break;
      }
      case 4: {
        Map content;
        for (int i = draw(6); i > 0; --i) content.insert(Rank{draw(20)}, step);
        rig.source.resetWith(content);
        break;
      }
      default: rig.source.replace(Rank{draw(20)}, step);
    }
    CHECK(ns(rig.keys()) == ns(rig.source.items().keys()));
    CHECK(rig.model.rowCount() == rig.source.size());
  }
}

// A QMultiMap orders the same way, and its model's index too.
void a_multi_map_follows_std_less() {
  using Multi = QMultiMap<Rank, int>;
  observable::ObservableContainer<Multi> source;
  observable::ContainerModel<Multi> model(&source, [](const observable::MultiKey<Rank>& key, const int&) { return QVariant(key.key.n); });
  QAbstractItemModelTester tester(&model, edge_data::kFatal);
  for (int n : {1, 3, 2, 3}) CHECK(source.insert({Rank{n}, 0}, n));
  QList<int> shown;
  for (int row = 0; row < model.rowCount(); ++row) shown.append(model.index(row, 0).data().toInt());
  CHECK(shown == ns(source.items().keys()) && shown == QList<int>({3, 3, 2, 1}));
  CHECK(source.remove({Rank{3}, 1}) && source.insert({Rank{5}, 0}, 5));
  shown.clear();
  for (int row = 0; row < model.rowCount(); ++row) shown.append(model.index(row, 0).data().toInt());
  CHECK(shown == ns(source.items().keys()));
}
}  // namespace

int main() {
  rows_follow_the_order_of_the_map();
  random_changes_keep_the_order();
  a_multi_map_follows_std_less();
  return rqt_test::finish("capability_edge_data_key_order");
}
