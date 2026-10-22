// The selection tracker (the acceptance tests of the spec, SPEC 7): the deltas of a Multi and a Single tracker for a tick, an
// insertion, a removal and a clear, the role names, and selectedValue (which borrows the container and unwraps an optional).
#include "check.hpp"

#include <selection_tracker.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <array>
#include <optional>
#include <vector>

namespace {

constexpr int kRole1 = Qt::UserRole + 1;
constexpr int kRole2 = Qt::UserRole + 2;
constexpr std::array<int, 2> kRoles{kRole1, kRole2};

using SingleTracker = observable::SelectionTracker<observable::SelectionMode::Single, kRoles>;
using MultiTracker = observable::SelectionTracker<observable::SelectionMode::Multi, kRoles>;

bool is(const observable::SelectionDelta& d, bool presenceFlipped, bool selectionChanged, bool countChanged) {
  return d.presenceFlipped == presenceFlipped && d.selectionChanged == selectionChanged && d.countChanged == countChanged;
}

// a container type derived from std::vector<QString> whose copy constructor counts
struct CountingVector : std::vector<QString> {
  static inline int copies = 0;

  CountingVector(std::initializer_list<QString> init) : std::vector<QString>(init) {}
  CountingVector(const CountingVector& other) : std::vector<QString>(other) { ++copies; }
  CountingVector(CountingVector&&) = default;
};

void at_7_1_multiDeltasOfTheFirstAndSecondTick() {
  MultiTracker t;
  t.reset(3);
  CHECK(is(t.set(1, true), true, true, true));
  CHECK(is(t.set(2, true), false, true, true));
  CHECK((t.selectedRows()) == ((std::vector<int>{1, 2})));
  CHECK((t.count()) == (2));
}

void at_7_2_multiSetToTheCurrentValueChangesNothing() {
  MultiTracker t;
  t.reset(3);
  CHECK(is(t.set(0, false), false, false, false));
  t.set(0, true);
  CHECK(is(t.set(0, true), false, false, false));
  CHECK((t.count()) == (1));
}

void at_7_3_multiInsertionShiftsLaterRows() {
  MultiTracker t;
  t.reset(3);
  t.set(2, true);
  t.rowInserted(0);
  CHECK(t.isSelected(3));
  CHECK(!t.isSelected(2));
  t.rowInserted(4);
  CHECK(t.isSelected(3));
  CHECK((t.rowCount()) == (5));
}

void at_7_4_multiRemovalDeltas() {
  MultiTracker t;
  t.reset(3);
  t.set(0, true);
  t.set(2, true);
  CHECK(is(t.rowRemoved(1), false, false, false));
  CHECK(t.isSelected(0));
  CHECK(t.isSelected(1));
  CHECK(is(t.rowRemoved(1), false, true, true));
  CHECK(is(t.rowRemoved(0), true, true, true));
  CHECK(!t.hasSelection());
  CHECK((t.rowCount()) == (0));
}

void at_7_5_multiClearKeepsTheRowCount() {
  MultiTracker t;
  t.reset(3);
  t.set(0, true);
  t.set(1, true);
  t.clear();
  CHECK((t.rowCount()) == (3));
  for (int r = 0; r < 3; ++r)
    CHECK(!t.isSelected(r));
  CHECK(is(t.set(2, true), true, true, true));
}

void at_7_6_singleSetDeltas() {
  SingleTracker t;
  CHECK(is(t.set(2), true, true, true));
  CHECK(is(t.set(5), false, true, false));
  CHECK(is(t.set(std::nullopt), true, true, true));
  CHECK(is(t.set(std::nullopt), false, false, false));
}

void at_7_7_singleCountIsZeroOrOne() {
  SingleTracker t;
  CHECK((t.count()) == (0));
  t.set(1);
  CHECK((t.count()) == (1));
  t.set(4);
  CHECK((t.count()) == (1));
  t.clear();
  CHECK((t.count()) == (0));
}

void at_7_8_singleInsertionOnOrBeforeTheSelectedRowPushesItDown() {
  SingleTracker t;
  t.set(2);
  t.rowInserted(0);
  CHECK((t.selectedRow()) == (3));
  t.rowInserted(3);  // exactly at the selected index
  CHECK((t.selectedRow()) == (4));
  t.rowInserted(9);
  CHECK((t.selectedRow()) == (4));
}

void at_7_9_singleRemovalDeltas() {
  SingleTracker t;
  t.set(3);
  CHECK(is(t.rowRemoved(1), false, true, false));
  CHECK((t.selectedRow()) == (2));
  CHECK(is(t.rowRemoved(7), false, false, false));
  CHECK((t.selectedRow()) == (2));
  CHECK(is(t.rowRemoved(2), true, true, true));
  CHECK(!t.hasSelection());
  CHECK(is(t.rowRemoved(0), false, false, false));
}

void at_7_10_appendRoleNames() {
  QHash<int, QByteArray> names{{kRole1, "value"}, {kRole2, "count"}};
  SingleTracker::appendRoleNames(names);
  CHECK((names.size()) == (3));
  CHECK((names.value(SingleTracker::selectedRole())) == (QByteArray("isSelected")));
  CHECK((names.value(kRole1)) == (QByteArray("value")));
  CHECK((QByteArray(SingleTracker::selectedRoleName())) == (QByteArray("isSelected")));
}

void at_7_11_selectedValueBorrowsTheContainer() {
  constexpr auto upper = [](const QString & s) { return s.toUpper(); };
  SingleTracker t;
  const CountingVector names{"red", "green", "blue"};
  CountingVector::copies = 0;

  CHECK((observable::selectedValue<upper>(t, names)) == (QString()));
  CHECK((observable::selectedValue<upper>(t, static_cast<const CountingVector *>(nullptr))) == (QString()));  // not read
  t.set(1);
  CHECK((observable::selectedValue<upper>(t, names)) == (QString("GREEN")));
  CHECK((observable::selectedValue<upper>(t, &names)) == (QString("GREEN")));
  CountingVector mutableNames{"x", "y"};
  t.set(0);
  CHECK((observable::selectedValue<upper>(t, &mutableNames)) == (QString("X")));  // a non-const pointer works too
  CHECK((CountingVector::copies) == (0));
}

void at_7_11_selectedValueGetterForms() {
  struct Item {
    QString name;
    std::optional<int> maybe;
    int twice() const { return 2 * static_cast<int>(name.size()); }
  };
  SingleTracker t;
  const std::vector<Item> items{{"ab", std::nullopt}, {"cde", 7}};
  t.set(1);
  CHECK((observable::selectedValue<&Item::name>(t, items)) == (QString("cde")));
  CHECK((observable::selectedValue<&Item::twice>(t, items)) == (6));
  CHECK((observable::selectedValue<&Item::maybe>(t, items)) == (7));  // an optional is unwrapped
  t.set(0);
  CHECK((observable::selectedValue<&Item::maybe>(t, items)) == (0));  // an empty optional gives int{}
  const QList<QString> list{"p", "q"};
  CHECK((observable::selectedValue<[](const QString & s) -> QString { return s + s; }>(t, list)) == (QString("pp")));
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  at_7_1_multiDeltasOfTheFirstAndSecondTick();
  at_7_2_multiSetToTheCurrentValueChangesNothing();
  at_7_3_multiInsertionShiftsLaterRows();
  at_7_4_multiRemovalDeltas();
  at_7_5_multiClearKeepsTheRowCount();
  at_7_6_singleSetDeltas();
  at_7_7_singleCountIsZeroOrOne();
  at_7_8_singleInsertionOnOrBeforeTheSelectedRowPushesItDown();
  at_7_9_singleRemovalDeltas();
  at_7_10_appendRoleNames();
  at_7_11_selectedValueBorrowsTheContainer();
  at_7_11_selectedValueGetterForms();
  return rqt_test::finish("capability_observable_selection_tracker");
}
