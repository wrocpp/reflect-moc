// Data edge cases, sizes. A Qt container counts in qsizetype, a model in int, so the container refuses a change that
// would give it more than observable::kMaxRows (INT_MAX) entries instead of letting the model's row count wrap:
// insert, insertRange and insertMany return false or 0 and announce nothing, resetWith returns false and keeps the
// content it has. Nothing here holds 2^31 entries: stand-in types with a traits specialisation report the size.
// Catalogue: docs/qt-edge-cases/data-types.md (D-30).
#include "data_support.hpp"

#include <climits>

namespace {
// A list that holds nothing and says how many entries it has.
struct BigList {
  qsizetype entries = 0;
  void clear() { entries = 0; }
};
// A set-like stand-in for the batches.
struct BigSet {
  qsizetype entries = 0;
  QList<int> keys;
  void clear() { entries = 0; }
  void insert(int key) { keys.append(key); }  // what a batch's staging and commit call on a set
  void detach() {}
};
}  // namespace

namespace observable {
template <>
struct ContainerTraits<BigList> {
  using Key = qsizetype;
  using Value = int;
  static constexpr Order order = Order::indexed;
  static constexpr bool movable = false;
  static constexpr bool assignable = true;
  static qsizetype size(const BigList& c) { return c.entries; }
  static bool contains(const BigList& c, Key k) { return k >= 0 && k < c.entries; }
  static bool can_insert(const BigList& c, Key k) { return k >= 0 && k <= c.entries; }
  static bool can_move(const BigList&, Key, Key) { return false; }
  static void insert(BigList& c, Key, int) { ++c.entries; }
  static void erase(BigList& c, Key) { --c.entries; }
  static void assign(BigList&, Key, int) {}
  static void move(BigList&, Key, Key) {}
  static const int& value(const BigList&, Key) {
    static const int zero = 0;
    return zero;
  }
  static QList<Key> keys(const BigList&) { return {}; }
};

template <>
struct ContainerTraits<BigSet> {
  using Key = int;
  using Value = int;
  static constexpr Order order = Order::unordered;
  static constexpr bool movable = false;
  static constexpr bool assignable = false;
  static qsizetype size(const BigSet& c) { return c.entries + c.keys.size(); }
  static bool contains(const BigSet& c, Key k) { return c.keys.contains(k); }
  static bool can_insert(const BigSet& c, Key k) { return !c.keys.contains(k); }
  static bool can_move(const BigSet&, Key, Key) { return false; }
  static void insert(BigSet& c, Key k, int) { c.keys.append(k); }
  static void erase(BigSet& c, Key k) { c.keys.removeOne(k); }
  static void assign(BigSet&, Key, int) {}
  static void move(BigSet&, Key, Key) {}
  static const int& value(const BigSet&, Key k) {
    static int last = 0;
    return last = k;
  }
  static QList<Key> keys(const BigSet& c) { return c.keys; }
};
}  // namespace observable

namespace {
using observable::kMaxRows;
using observable::ObservableContainer;

constexpr qsizetype kPastIntMax = qsizetype{INT_MAX} + 1;

void the_limit_and_its_helpers() {
  static_assert(kMaxRows == INT_MAX);
  static_assert(observable::fits_rows(0, kMaxRows) && !observable::fits_rows(1, kMaxRows));
  static_assert(observable::fits_rows(kMaxRows - 1, 1) && !observable::fits_rows(kMaxRows, 1));
  static_assert(!observable::fits_rows(0, -1) && observable::fits_rows(kMaxRows, 0));
  CHECK(observable::to_row(0) == 0 && observable::to_row(kMaxRows) == INT_MAX && observable::to_row(12345) == 12345);
}

void a_list_stops_at_the_limit() {
  ObservableContainer<BigList> source(BigList{INT_MAX - 1});
  observable::ContainerModel<BigList> model(&source, [](const qsizetype&, const int&) { return QVariant(); });
  CHECK(model.rowCount() == INT_MAX - 1);
  int announced = 0;
  QObject::connect(&source, &decltype(source)::entryAdded, &source, [&](const qsizetype&) { ++announced; });
  QObject::connect(&source, &decltype(source)::rangeAdded, &source, [&](const qsizetype&, const qsizetype&) { ++announced; });
  // A run that does not fit is refused whole; one that does is taken.
  CHECK(!source.insertRange(0, QList<int>{1, 2}));
  CHECK(source.size() == INT_MAX - 1 && model.rowCount() == INT_MAX - 1 && announced == 0);
  CHECK(source.insertRange(0, QList<int>{1}));
  CHECK(source.size() == INT_MAX && model.rowCount() == INT_MAX && announced == 1);
  // Full: every way in is refused, the model keeps a count that is right.
  CHECK(!source.insert(0, 1) && !source.insert(INT_MAX, 1) && !source.insertRange(5, QList<int>{1}));
  CHECK(source.size() == INT_MAX && model.rowCount() == INT_MAX && announced == 1);
  CHECK(source.remove(3) && model.rowCount() == INT_MAX - 1);  // taking one out makes room for one
  CHECK(source.insert(0, 1) && model.rowCount() == INT_MAX);
}

void a_reset_past_the_limit_is_refused() {
  ObservableContainer<BigList> source(BigList{7});
  observable::ContainerModel<BigList> model(&source, [](const qsizetype&, const int&) { return QVariant(); });
  int resets = 0;
  QObject::connect(&source, &decltype(source)::aboutToBeReset, &source, [&] { ++resets; });
  CHECK(!source.resetWith(BigList{kPastIntMax}));
  CHECK(!source.resetWith(BigList{(qsizetype{1} << 32) + 3}));
  CHECK(resets == 0 && source.size() == 7 && model.rowCount() == 7);  // the content and the model are as they were
  CHECK(source.resetWith(BigList{INT_MAX}));                          // the largest that fits is taken
  CHECK(resets == 1 && model.rowCount() == INT_MAX);
  CHECK(!source.resetWith(BigList{kPastIntMax}) && model.rowCount() == INT_MAX);
}

void a_batch_past_the_limit_is_refused() {
  ObservableContainer<BigSet> source(BigSet{INT_MAX - 2, {}});
  int announced = 0;
  QObject::connect(&source, &decltype(source)::entriesAboutToBeAdded, &source, [&](const QList<int>&) { ++announced; });
  CHECK(source.insertMany(BigSet{0, {1, 2, 3}}) == 0);  // three do not fit under INT_MAX - 2 + 3
  CHECK(announced == 0 && source.size() == INT_MAX - 2);
  CHECK(source.insertMany(BigSet{0, {1, 2}}) == 2 && announced == 1 && source.size() == INT_MAX);
  CHECK(source.insertMany(BigSet{0, {7}}) == 0 && !source.insert(9, 9) && source.size() == INT_MAX);
  CHECK(source.remove(1) && source.insert(9, 9));
}

#ifndef QT_NO_DEBUG
// A constructor cannot refuse: a content past the limit asserts in a debug build (the child dies of SIGABRT).
void an_oversized_constructor_asserts() {
  CHECK(edge_data::aborts([] { ObservableContainer<BigList> source(BigList{kPastIntMax}); }));
  CHECK(!edge_data::aborts([] { ObservableContainer<BigList> source(BigList{INT_MAX}); }));
}
#endif
}  // namespace

int main() {
  the_limit_and_its_helpers();
  a_list_stops_at_the_limit();
  a_reset_past_the_limit_is_refused();
  a_batch_past_the_limit_is_refused();
#ifndef QT_NO_DEBUG
  an_oversized_constructor_asserts();
#endif
  return rqt_test::finish("capability_edge_data_size_limit");
}
