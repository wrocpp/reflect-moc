// Data edge cases, values: what a value type must be, what the container copies, and what a copy that throws or an
// implicitly shared value does. The container needs a nothrow move (a static_assert) and nothing else is checked, so
// the cases below look at what the rest of the interface asks of the type.
// A compile-time refusal cannot be a running test: those rows are covered by the type traits that decide them, and
// the part of the interface that does compile is run.
// Catalogue: docs/qt-edge-cases/data-types.md (D-20 to D-26).
#include "data_support.hpp"

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <memory>
#include <stdexcept>

namespace {
using edge_data::Rig;
using observable::ObservableContainer;

// Counts its copies and moves; a copy can be made to throw after a number of copies.
struct Counted {
  static inline int copies = 0;
  static inline int moves = 0;
  static inline int throwAtCopy = 0;  // the n-th copy from now on throws; 0 is never
  int v = 0;
  Counted() = default;
  explicit Counted(int v) : v(v) {}
  Counted(const Counted& other) : v(other.v) { copied(); }
  Counted(Counted&& other) noexcept : v(other.v) { ++moves; }
  Counted& operator=(const Counted& other) {
    v = other.v;
    copied();
    return *this;
  }
  Counted& operator=(Counted&& other) noexcept {
    v = other.v;
    ++moves;
    return *this;
  }
  static void reset() { copies = moves = throwAtCopy = 0; }
  static void copied() {
    if (throwAtCopy != 0 && ++copies >= throwAtCopy) throw std::runtime_error("copy");
    if (throwAtCopy == 0) ++copies;
  }
};

struct NoDefault {
  explicit NoDefault(int v) : v(v) {}
  int v;
};

// D-22: copies per call, measured on Qt 6.10 (QMap::insert takes the value by const reference, so an insert copies
// once even from a temporary). The bounds are what the code does today; a rise is a regression.
template <class C, class K>
void copy_budget(K a, K b, K c, int insertCopies, int removeCopies) {
  ObservableContainer<C> source;
  Counted::reset();
  CHECK(source.insert(a, Counted{1}));
  CHECK(Counted::copies <= insertCopies);
  Counted::reset();
  CHECK(source.replace(a, Counted{2}));
  CHECK(Counted::copies == 0);  // a replace moves
  Counted::reset();
  CHECK(source.editValue(a, [](Counted& value) { value.v = 3; }));
  CHECK(Counted::copies == 1);  // the edit runs on one copy, moved back
  C entries;
  observable::ContainerTraits<C>::insert(entries, b, Counted{4});
  Counted::reset();
  CHECK(source.insertMany(entries) == 1);
  CHECK(Counted::copies <= 2);  // one to take the value out of the argument, one into the container
  Counted::reset();
  const C taken = source.takeMany({b});
  CHECK(taken.size() == 1 && Counted::copies <= 2);
  C content;
  observable::ContainerTraits<C>::insert(content, c, Counted{5});
  Counted::reset();
  source.resetWith(std::move(content));
  CHECK(Counted::copies == 0);
  Counted::reset();
  CHECK(source.remove(c));
  CHECK(Counted::copies <= removeCopies);
}

void copies_per_operation() {
  copy_budget<QMap<int, Counted>>(1, 2, 3, 1, 0);
  copy_budget<QHash<int, Counted>>(1, 2, 3, 1, 1);  // QHash may move an entry to close the gap, and copies it
  ObservableContainer<QList<Counted>> list;
  Counted::reset();
  CHECK(list.insert(0, Counted{1}));
  CHECK(Counted::copies == 0);
  QList<Counted> run;
  run.append(Counted{2});
  run.append(Counted{3});
  Counted::reset();
  CHECK(list.insertRange(1, std::move(run)));
  CHECK(Counted::copies == 0);
  CHECK(list.move(0, 2));
  CHECK(list.remove(0));
  CHECK(Counted::copies == 0);
}

// D-23: an edit runs on a copy before anything is announced, so a copy that throws, or an edit that throws, leaves the
// container and every model as they were and emits nothing.
void a_throwing_copy_in_editValue_changes_nothing() {
  Rig<QMap<int, Counted>> rig;
  CHECK(rig.source.insert(1, Counted{7}));
  int announced = 0;
  QObject::connect(&rig.source, &decltype(rig.source)::entryAboutToBeChanged, &rig.source, [&](const int&, const QList<int>&) { ++announced; });
  QObject::connect(&rig.source, &decltype(rig.source)::entryChanged, &rig.source, [&](const int&, const QList<int>&) { ++announced; });
  Counted::reset();
  Counted::throwAtCopy = 1;
  bool threw = false;
  try {
    rig.source.editValue(1, [](Counted& value) { value.v = 99; });
  } catch (const std::runtime_error&) {
    threw = true;
  }
  Counted::reset();
  CHECK(threw);
  CHECK(rig.source.value(1).v == 7 && announced == 0 && rig.model.rowCount() == 1);
  // The edit itself throwing is the same.
  threw = false;
  try {
    rig.source.editValue(1, [](Counted& value) {
      value.v = 99;
      throw std::runtime_error("edit");
    });
  } catch (const std::runtime_error&) {
    threw = true;
  }
  CHECK(threw && rig.source.value(1).v == 7 && announced == 0);
  CHECK(rig.source.editValue(1, [](Counted& value) { value.v = 8; }) && rig.source.value(1).v == 8 && announced == 2);
}

// D-25: takeMany copies the entries before it announces the removal, so a throwing copy changes nothing.
void a_throwing_copy_in_takeMany_changes_nothing() {
  Rig<QMap<int, Counted>> rig;
  for (int key = 1; key <= 3; ++key) CHECK(rig.source.insert(key, Counted{key}));
  int announced = 0;
  QObject::connect(&rig.source, &decltype(rig.source)::entriesAboutToBeRemoved, &rig.source, [&](const QList<int>&) { ++announced; });
  Counted::reset();
  Counted::throwAtCopy = 3;  // the second entry
  bool threw = false;
  try {
    rig.source.takeMany({1, 2, 3});
  } catch (const std::runtime_error&) {
    threw = true;
  }
  Counted::reset();
  CHECK(threw && announced == 0);
  CHECK(rig.source.size() == 3 && rig.model.rowCount() == 3);
  CHECK(rig.source.takeMany({1, 2, 3}).size() == 3 && announced == 1 && rig.model.rowCount() == 0);
}

// D-26: the container holds a value type that is implicitly shared (QString, QByteArray, a QList) by copy on write.
// A copy of items() taken before a change keeps the content it had, through every kind of change.
template <class C, class K, class V>
void a_snapshot_is_not_changed(K a, K b, V first, V second) {
  Rig<C> rig;
  CHECK(rig.source.insert(a, first));
  const C before = rig.source.items();
  CHECK(rig.source.insert(b, second));
  CHECK(before.size() == 1 && rig.source.size() == 2);
  const C afterInsert = rig.source.items();
  CHECK(rig.source.replace(a, second));
  CHECK(afterInsert.size() == 2 && observable::ContainerTraits<C>::value(afterInsert, a) == first);
  const C afterReplace = rig.source.items();
  CHECK(rig.source.editValue(a, [&](V& value) { value = first; }));
  CHECK(observable::ContainerTraits<C>::value(afterReplace, a) == second);
  const C afterEdit = rig.source.items();
  CHECK(rig.source.remove(b));
  CHECK(afterEdit.size() == 2);
  rig.source.resetWith(C{});
  CHECK(afterEdit.size() == 2 && observable::ContainerTraits<C>::value(afterEdit, a) == first);
  CHECK(before.size() == 1 && observable::ContainerTraits<C>::value(before, a) == first);
  CHECK(rig.model.rowCount() == 0);
}

void a_snapshot_survives_every_change() {
  a_snapshot_is_not_changed<QMap<int, QString>>(1, 2, QString("one"), QString("two"));
  a_snapshot_is_not_changed<QHash<QString, QByteArray>>(QString("a"), QString("b"), QByteArray("x"), QByteArray("y"));
  a_snapshot_is_not_changed<QMap<int, QStringList>>(1, 2, QStringList{"p", "q"}, QStringList{"r"});
  // A list is changed by index.
  Rig<QList<QString>> list;
  CHECK(list.source.insert(0, "a") && list.source.insert(1, "b"));
  const QList<QString> before = list.source.items();
  CHECK(list.source.replace(0, "z") && list.source.move(0, 1) && list.source.remove(0));
  CHECK(before == QList<QString>({"a", "b"}));
  CHECK(list.source.items() == QList<QString>({"z"}));
}

// D-20: a move-only value passes the nothrow-move gate (its move is noexcept), yet a Qt container copies its data
// when it grows or detaches: QList<std::unique_ptr<int>>::append already fails to compile in qarraydataops.h, and
// so does every mutator of the container. ContainerModel fails too: its setData is virtual, so it is always
// instantiated, and it copies the value into replace. What compiles is the container's own declaration, size,
// contains, resetWith and clear (shown below with an empty list), which never copy an element.
// Verified with g++ 16.2 and Qt 6.10 while writing this row; the traits are what decide it.
void a_move_only_value_gets_through_the_gate_and_not_far_beyond() {
  using Move = std::unique_ptr<int>;
  static_assert(std::is_nothrow_move_constructible_v<Move> && std::is_nothrow_move_assignable_v<Move>);
  static_assert(!std::is_copy_constructible_v<Move>);
  ObservableContainer<QList<Move>> source;  // the declaration is accepted
  CHECK(source.size() == 0 && !source.contains(0));
  source.resetWith(QList<Move>{});
  CHECK(source.size() == 0 && !source.clear());
}

// D-21: a value without a default constructor works wherever the container does not default-construct it: insert with
// an explicit value, and for a list also replace, editValue and the model. A map's or hash's replace goes through
// operator[], which needs T(), so replace does not compile, and neither does ContainerModel over them (its setData
// is virtual, so it always instantiates replace). Verified with g++ 16.2 and Qt 6.10 while writing this row; the
// run below covers what compiles: the container without a model.
void a_value_without_a_default_constructor() {
  static_assert(!std::is_default_constructible_v<NoDefault>);
  ObservableContainer<QMap<int, NoDefault>> map;
  CHECK(map.insert(1, NoDefault{1}) && map.insert(2, NoDefault{2}));
  CHECK(map.value(2).v == 2 && map.remove(1) && map.size() == 1);
  QMap<int, NoDefault> entries;
  entries.insert(5, NoDefault{5});
  CHECK(map.insertMany(entries) == 1);
  CHECK(map.takeMany({5}).size() == 1);
  ObservableContainer<QHash<int, NoDefault>> hash;
  CHECK(hash.insert(1, NoDefault{1}) && hash.remove(1));
  Rig<QList<NoDefault>> list;
  CHECK(list.source.insert(0, NoDefault{1}) && list.source.replace(0, NoDefault{2}));
  CHECK(list.source.editValue(0, [](NoDefault& value) { value.v = 3; }));
  CHECK(list.source.value(0).v == 3 && list.model.rowCount() == 1);
}
}  // namespace

int main() {
  copies_per_operation();
  a_throwing_copy_in_editValue_changes_nothing();
  a_throwing_copy_in_takeMany_changes_nothing();
  a_snapshot_survives_every_change();
  a_move_only_value_gets_through_the_gate_and_not_far_beyond();
  a_value_without_a_default_constructor();
  return rqt_test::finish("capability_edge_data_values");
}
