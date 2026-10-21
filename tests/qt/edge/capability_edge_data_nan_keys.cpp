// Data edge cases, NaN keys. A floating point key that is not equal to itself cannot be found again: a QHash or
// QSet never finds it (it equals nothing), a QMap finds it for every key (it is equivalent to all). The container
// therefore refuses it: insert returns false, insertMany skips it like a key that is already there, contains,
// remove and replace never match it, and a content handed over whole (the constructor, resetWith) loses the entries
// that have one. Infinity is a usable key; -0.0 and 0.0 are still one key.
// Catalogue: docs/qt-edge-cases/data-types.md (D-03, D-04).
#include "data_support.hpp"

#include <cmath>
#include <limits>

namespace {
using edge_data::Rig;
using observable::ObservableContainer;

const double kNaN = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

// D-03: a QMap<double>: the NaN key is refused, in an empty map and in one with entries, and no key matches it.
void a_map_refuses_a_nan_key() {
  Rig<QMap<double, int>> empty;
  CHECK(!empty.source.insert(kNaN, 1));
  CHECK(empty.source.size() == 0 && empty.model.rowCount() == 0);
  CHECK(empty.source.insert(1.0, 1) && empty.source.insert(2.0, 2));
  CHECK(!empty.source.insert(kNaN, 3));
  CHECK(empty.source.size() == 2 && empty.model.rowCount() == 2);
  // Nothing matches a NaN, and nothing else is disturbed.
  CHECK(!empty.source.contains(kNaN) && !empty.source.remove(kNaN) && !empty.source.replace(kNaN, 9));
  CHECK(!empty.source.contains(5.0) && !empty.source.remove(5.0));
  CHECK(empty.keys() == QList<double>({1.0, 2.0}));
  // A float key is the same.
  Rig<QMap<float, int>> floats;
  CHECK(!floats.source.insert(std::numeric_limits<float>::quiet_NaN(), 1) && floats.source.insert(0.5f, 1));
  // Infinity and -0.0 are keys.
  CHECK(empty.source.insert(kInf, 1) && empty.source.insert(-kInf, 1));
  CHECK(empty.source.insert(-0.0, 1) && !empty.source.insert(0.0, 1));
  CHECK(empty.keys() == QList<double>({-kInf, -0.0, 1.0, 2.0, kInf}));
}

// D-04: a QHash<double> and a QSet<double>: the same refusal, where each insert used to add an entry that could never
// be found again.
void a_hash_and_a_set_refuse_a_nan_key() {
  Rig<QHash<double, int>> hash;
  CHECK(!hash.source.insert(kNaN, 1) && !hash.source.insert(kNaN, 2));
  CHECK(hash.source.insert(1.0, 3));
  CHECK(!hash.source.contains(kNaN) && !hash.source.remove(kNaN) && !hash.source.replace(kNaN, 5));
  CHECK(hash.source.size() == 1 && hash.model.rowCount() == 1);
  Rig<QSet<double>> set;
  CHECK(!set.source.insert(kNaN) && set.source.insert(1.0) && !set.source.insert(kNaN));
  CHECK(set.source.size() == 1 && set.model.rowCount() == 1 && !set.source.remove(kNaN));
  Rig<QHash<float, int>> floats;
  CHECK(!floats.source.insert(std::numeric_limits<float>::quiet_NaN(), 1) && floats.source.size() == 0);
}

// insertMany skips a NaN key like a key that is there, and announces the keys that are left (or nothing).
void a_batch_skips_nan_keys() {
  Rig<QHash<double, int>> hash;
  int announced = 0;
  QObject::connect(&hash.source, &decltype(hash.source)::entriesAdded, &hash.source, [&](const QList<double>&) { ++announced; });
  QHash<double, int> entries;
  entries.insert(kNaN, 1);
  entries.insert(kNaN, 2);
  entries.insert(7.0, 3);
  CHECK(entries.size() == 3);  // a hash holds both NaN entries
  CHECK(hash.source.insertMany(entries) == 1 && announced == 1);
  CHECK(hash.keys() == QList<double>({7.0}));
  QHash<double, int> onlyNaN;
  onlyNaN.insert(kNaN, 1);
  CHECK(hash.source.insertMany(onlyNaN) == 0 && announced == 1);  // nothing left: nothing announced

  Rig<QMap<double, int>> map;
  QMap<double, int> mapEntries;
  mapEntries.insert(kNaN, 1);  // a map with a NaN key holds that entry alone
  CHECK(map.source.insertMany(mapEntries) == 0 && map.model.rowCount() == 0);
  mapEntries.clear();
  mapEntries.insert(1.0, 1);
  mapEntries.insert(2.0, 2);
  CHECK(map.source.insertMany(mapEntries) == 2);

  Rig<QSet<double>> set;
  QSet<double> setEntries{kNaN, kNaN, 3.0};
  CHECK(set.source.insertMany(setEntries) == 1 && set.keys() == QList<double>({3.0}));
  // removeMany and takeMany ignore a NaN as they ignore an absent key.
  CHECK(set.source.removeMany({kNaN, 3.0}) == 1 && set.source.size() == 0);
  CHECK(hash.source.takeMany({kNaN, 7.0}).size() == 1);
}

// A content given whole loses its NaN entries: the constructor and resetWith (which still announces, once).
void a_whole_content_loses_its_nan_entries() {
  QHash<double, int> content;
  content.insert(kNaN, 1);
  content.insert(kNaN, 2);
  content.insert(1.0, 3);
  Rig<QHash<double, int>> hash(content);
  CHECK(hash.source.size() == 1 && hash.model.rowCount() == 1 && hash.keys() == QList<double>({1.0}));
  int resets = 0;
  QObject::connect(&hash.source, &decltype(hash.source)::resetCompleted, &hash.source, [&] { ++resets; });
  CHECK(hash.source.resetWith(content));
  CHECK(resets == 1 && hash.source.size() == 1 && hash.model.rowCount() == 1);
  QHash<double, int> onlyNaN;
  onlyNaN.insert(kNaN, 1);
  CHECK(hash.source.resetWith(onlyNaN));
  CHECK(resets == 2 && hash.source.size() == 0 && hash.model.rowCount() == 0);

  QMap<double, int> mapContent;
  mapContent.insert(kNaN, 1);
  Rig<QMap<double, int>> map(mapContent);
  CHECK(map.source.size() == 0 && map.model.rowCount() == 0);
  QMap<double, int> fine;
  fine.insert(2.0, 2);
  fine.insert(1.0, 1);
  CHECK(map.source.resetWith(fine) && map.keys() == QList<double>({1.0, 2.0}));

  Rig<QSet<double>> set(QSet<double>{kNaN, kNaN, 5.0, 6.0});
  CHECK(set.source.size() == 2 && set.model.rowCount() == 2);
  CHECK(set.source.resetWith(QSet<double>{kNaN}) && set.source.size() == 0);
}

// Keys that are not floating point are not looked at.
void other_keys_are_untouched() {
  static_assert(observable::usable_key(1) && observable::usable_key(0.0) && observable::usable_key(-0.0));
  CHECK(observable::usable_key(QString()));
  CHECK(!observable::usable_key(kNaN) && observable::usable_key(kInf));
  Rig<QMap<int, int>> ints;
  CHECK(ints.source.insert(1, 1) && ints.source.resetWith(QMap<int, int>{{2, 2}}));
}
}  // namespace

int main() {
  a_map_refuses_a_nan_key();
  a_hash_and_a_set_refuse_a_nan_key();
  a_batch_skips_nan_keys();
  a_whole_content_loses_its_nan_entries();
  other_keys_are_untouched();
  return rqt_test::finish("capability_edge_data_nan_keys");
}
