// The matrix's signal contract (the acceptance tests of the spec, SPEC 5): every change is announced before it happens
// and confirmed after, a rename is its own pair, setValues announces full rectangles, a refused request says nothing.
#include "matrix_support.hpp"

#include <QtCore/QCoreApplication>

namespace {
using namespace matrix_test;

void at_5_1_valueIsTotalAndReadingCreatesNothing() {
  Mat m(kNaN);
  m.addRow("r");
  m.addColumn("c");
  Recorder rec(m);
  CHECK(std::isnan(m.value("absent", "c")));
  CHECK(std::isnan(m.value("r", "absent")));
  CHECK((m.rowCount()) == (1));
  CHECK((m.columnCount()) == (1));
  CHECK(!m.setValue("r", "absent", 1.0));
  CHECK(!m.setValue("absent", "c", 1.0));
  CHECK(rec.log.isEmpty());
  CHECK((m.rowCount()) == (1));
  CHECK((m.columnCount()) == (1));
}

void at_5_2_setValueNotifiesOnlyARealChange() {
  Mat m(kNaN);
  m.addRows({"r0", "r1"});
  m.addColumns({"c0", "c1"});
  Recorder rec(m);

  CHECK(m.setValue("r1", "c0", 2.5));
  CHECK((rec.log) == ((QStringList{"cellAboutToChange(r1,c0,1,0)", "cellChanged(r1,c0,1,0)"})));
  CHECK((m.value("r1", "c0")) == (2.5));

  rec.log.clear();
  CHECK(m.setValue("r1", "c0", 2.5));
  CHECK(rec.log.isEmpty());

  CHECK(m.setValue("r0", "c1", kNaN));  // a NaN over an uninitialised (NaN) cell
  CHECK(rec.log.isEmpty());
}

void at_5_2_theAboutSeesTheOldValueAndTheTwinTheNewOne() {
  Mat m(kNaN);
  m.addRow("r");
  m.addColumn("c");
  m.setValue("r", "c", 1.0);
  double seenAbout = 0, seenTwin = 0;
  QObject ctx;
  QObject::connect(&m, &Mat::cellAboutToChange, &ctx, [&] { seenAbout = m.value("r", "c"); });
  QObject::connect(&m, &Mat::cellChanged, &ctx, [&] { seenTwin = m.value("r", "c"); });
  m.setValue("r", "c", 7.0);
  CHECK((seenAbout) == (1.0));
  CHECK((seenTwin) == (7.0));
}

void at_5_3_addRowsIsOnePairOverTheNewRange() {
  Mat m(kNaN);
  m.addRows({"p", "q", "rx"});
  Recorder rec(m);
  CHECK((m.addRows({"r1", "r2", "r1", "rx"})) == (2));
  CHECK((rec.log) == ((QStringList{"rowsAboutToBeAdded(3,4,[r1,r2])", "rowsAdded(3,4,[r1,r2])"})));
  CHECK((m.rowKeys()) == ((QList<QString>{"p", "q", "rx", "r1", "r2"})));
  rec.log.clear();
  CHECK((m.addRows({"p", "q"})) == (0));
  CHECK(rec.log.isEmpty());
}

void at_5_4_sequenceT5RemoveRowsIsOnePairPerRunFromTheLast() {
  Mat m(kNaN);
  m.addRows(keys("k", 8));
  Recorder rec(m);
  CHECK((m.removeRows({"k1", "k2", "k5", "k7"})) == (4));
  CHECK((rec.log) == ((QStringList{"rowsAboutToBeRemoved(7,7,[k7])", "rowsRemoved(7,7,[k7])", "rowsAboutToBeRemoved(5,5,[k5])", "rowsRemoved(5,5,[k5])",
                 "rowsAboutToBeRemoved(1,2,[k1,k2])", "rowsRemoved(1,2,[k1,k2])"})));
  CHECK((m.rowKeys()) == ((QList<QString>{"k0", "k3", "k4", "k6"})));
}

void at_5_5_renameRow() {
  Mat m(kNaN);
  m.addRows({"s1", "s2", "s3"});
  m.addColumn("c");
  m.setValue("s2", "c", 4.0);
  Recorder rec(m);

  CHECK(m.renameRow("s1", "s1"));
  CHECK(!m.renameRow("nope", "nope"));
  CHECK(!m.renameRow("nope", "x"));
  CHECK(!m.renameRow("s1", "s3"));
  CHECK(rec.log.isEmpty());
  CHECK((m.rowKeys()) == ((QList<QString>{"s1", "s2", "s3"})));
  CHECK((m.rowCount()) == (3));

  CHECK((m.rowIndexOf("s2")) == (1));  // the lookup map is built, then the rename must stale it
  CHECK(m.renameRow("s2", "s2b"));
  CHECK((rec.log) == ((QStringList{"rowAboutToBeRenamed(s2,s2b,1)", "rowRenamed(s2,s2b,1)"})));
  CHECK((m.rowIndexOf("s2b")) == (1));
  CHECK((m.rowIndexOf("s2")) == (-1));
  CHECK((m.value("s2b", "c")) == (4.0));
  CHECK((m.rowCount()) == (3));
}

void at_5_5_renameRowWithAReferenceIntoTheMatrixItself() {
  Mat m(kNaN);
  m.addRows({"s1", "s2"});
  Recorder rec(m);
  CHECK(m.renameRow(m.rowKeys()[1], "z"));
  CHECK((rec.log) == ((QStringList{"rowAboutToBeRenamed(s2,z,1)", "rowRenamed(s2,z,1)"})));
}

void at_5_5_renameColumn() {
  Mat m(kNaN);
  m.addRow("r");
  m.addColumns({"a", "b"});
  m.setValue("r", "b", 3.0);
  Recorder rec(m);
  CHECK((m.columnIndexOf("b")) == (1));
  CHECK(m.renameColumn("b", "b2"));
  CHECK((rec.log) == ((QStringList{"columnAboutToBeRenamed(b,b2,1)", "columnRenamed(b,b2,1)"})));
  CHECK((m.columnIndexOf("b")) == (-1));
  CHECK((m.columnIndexOf("b2")) == (1));
  CHECK((m.value("r", "b2")) == (3.0));
  CHECK(!m.renameColumn("a", "b2"));
  CHECK(m.renameColumn("a", "a"));
}

void at_5_6_aDenseBlockIsOneRectangle() {
  Mat m(kNaN);
  m.addRows(keys("r", 4));
  m.addColumns(keys("c", 5));
  Recorder rec(m);
  Cells cells;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 4; ++j)
      cells << std::make_tuple(QStringLiteral("r%1").arg(i), QStringLiteral("c%1").arg(j), i * 10.0 + j);
  CHECK((m.setValues(cells)) == (12));
  CHECK((rec.log) == ((QStringList{"cellsAboutToChange(0,0,2,3)", "cellsChanged(0,0,2,3)"})));
  CHECK((m.value("r2", "c3")) == (23.0));
  CHECK(std::isnan(m.value("r3", "c0")));
  CHECK(std::isnan(m.value("r0", "c4")));
}

void at_5_6_sequenceT6AnLShapeIsTwoRectangles() {
  Mat m(kNaN);
  m.addRows({"r0", "r1"});
  m.addColumns({"c0", "c1"});
  Recorder rec(m);
  CHECK((m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r1", "c0", 3.0}})) == (3));
  CHECK((rec.log) == ((QStringList{"cellsAboutToChange(0,0,0,1)", "cellsAboutToChange(1,0,1,0)", "cellsChanged(0,0,0,1)", "cellsChanged(1,0,1,0)"})));
}

void at_5_6_unchangedCellsAreNotCountedAndTheLastOfADuplicateWins() {
  Mat m(kNaN);
  m.addRows({"r0", "r1"});
  m.addColumns({"c0", "c1"});
  m.setValue("r0", "c0", 5.0);
  Recorder rec(m);

  CHECK((m.setValues({{"r0", "c0", 5.0}, {"r1", "c1", 8.0}, {"nope", "c0", 1.0}, {"r0", "nope", 1.0}})) == (1));
  CHECK((rec.log) == ((QStringList{"cellsAboutToChange(1,1,1,1)", "cellsChanged(1,1,1,1)"})));

  rec.log.clear();
  CHECK((m.setValues({{"r0", "c1", 1.0}, {"r0", "c1", 2.0}})) == (1));
  CHECK((m.value("r0", "c1")) == (2.0));

  rec.log.clear();
  // the last occurrence wins even when it equals the current value: nothing changes
  CHECK((m.setValues({{"r0", "c1", 9.0}, {"r0", "c1", 2.0}})) == (0));
  CHECK(rec.log.isEmpty());
}

void at_5_6_aGapBetweenRowsEndsARectangle() {
  Mat m(kNaN);
  m.addRows(keys("r", 4));
  m.addColumns({"c0", "c1"});
  Recorder rec(m);
  CHECK((m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 1.0}, {"r1", "c0", 1.0}, {"r1", "c1", 1.0}, {"r3", "c0", 1.0}, {"r3", "c1", 1.0}})) == (6));
  CHECK((rec.log) == ((QStringList{"cellsAboutToChange(0,0,1,1)", "cellsAboutToChange(3,0,3,1)", "cellsChanged(0,0,1,1)", "cellsChanged(3,0,3,1)"})));
}

void at_5_6_theAboutsAllPrecedeTheWrites() {
  Mat m(kNaN);
  m.addRows({"r0", "r1"});
  m.addColumns({"c0", "c1"});
  QList<double> seenAtAbouts, seenAtTwins;
  QObject ctx;
  QObject::connect(&m, &Mat::cellsAboutToChange, &ctx, [&] { seenAtAbouts << m.valueAt(1, 0) << m.valueAt(0, 0); });
  QObject::connect(&m, &Mat::cellsChanged, &ctx, [&] { seenAtTwins << m.valueAt(1, 0) << m.valueAt(0, 0); });
  m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r1", "c0", 3.0}});
  for (double v : seenAtAbouts)
    CHECK(std::isnan(v));
  for (double v : seenAtTwins)
    CHECK(!std::isnan(v));
  CHECK((seenAtAbouts.size()) == (4));
}

void at_5_7_clear() {
  Mat m(kNaN);
  Recorder rec(m);
  m.clear();
  CHECK(rec.log.isEmpty());

  m.addRow("r");
  m.addColumn("c");
  rec.log.clear();
  m.clear();
  CHECK((rec.log) == ((QStringList{"aboutToBeCleared", "cleared"})));
  CHECK((m.rowCount()) == (0));
  CHECK((m.columnCount()) == (0));
  CHECK((m.rowIndexOf("r")) == (-1));
  CHECK(m.addRow("r"));  // the lookup map was staled
}

void capability_columnsAndRowsKeepTheirValuesThroughStructuralChanges() {
  Mat m(kNaN);
  m.addRows({"a", "b", "c"});
  m.addColumns({"x", "y"});
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j)
      m.setValue(m.rowKeys()[i], m.columnKeys()[j], 10.0 * (i + 1) + j);

  m.addColumn("z");
  CHECK((m.value("b", "y")) == (21.0));
  CHECK(std::isnan(m.value("b", "z")));
  m.setValue("c", "z", 99.0);

  m.addColumns({"p", "q"});
  CHECK((m.value("c", "z")) == (99.0));
  CHECK((m.value("a", "x")) == (10.0));
  CHECK(std::isnan(m.value("c", "q")));

  CHECK((m.removeColumns({"y", "q", "p"})) == (3));  // two runs: [1,1] and [3,4]
  CHECK((m.columnKeys()) == ((QList<QString>{"x", "z"})));
  CHECK((m.value("a", "x")) == (10.0));
  CHECK((m.value("b", "x")) == (20.0));
  CHECK((m.value("c", "z")) == (99.0));

  CHECK(m.removeRow("b"));
  CHECK((m.value("c", "x")) == (30.0));
  CHECK((m.value("c", "z")) == (99.0));
  m.addRow("d");
  CHECK(std::isnan(m.value("d", "x")));
  CHECK(m.removeColumn("x"));
  CHECK((m.value("c", "z")) == (99.0));
  CHECK((m.columnCount()) == (1));
}

void capability_removeColumnsRunsGoFromTheLastToTheFirst() {
  Mat m(kNaN);
  m.addRow("r");
  m.addColumns(keys("c", 8));
  Recorder rec(m);
  CHECK((m.removeColumns({"c1", "c2", "c5", "c7"})) == (4));
  CHECK((rec.log) == ((QStringList{"columnsAboutToBeRemoved(7,7,[c7])", "columnsRemoved(7,7,[c7])", "columnsAboutToBeRemoved(5,5,[c5])", "columnsRemoved(5,5,[c5])",
                 "columnsAboutToBeRemoved(1,2,[c1,c2])", "columnsRemoved(1,2,[c1,c2])"})));
}

void capability_aNonNaNSentinelUsesEquality() {
  observable::ObservableMatrix<int, int, int> m(-1);
  m.addRow(1);
  m.addColumn(2);
  CHECK(m.isUninitialized(-1));
  CHECK(!m.isUninitialized(0));
  QSignalSpy spy(&m, &observable::ObservableMatrix<int, int, int>::cellChanged);
  CHECK(m.setValue(1, 2, -1));
  CHECK((spy.count()) == (0));
  CHECK(m.setValue(1, 2, 5));
  CHECK((spy.count()) == (1));
  CHECK((spy.at(0).at(0).toInt()) == (1));
  CHECK((spy.at(0).at(1).toInt()) == (2));
}


// The "about" signal of every change sees the matrix as it was and its twin as it is: counts, keys and the index of a key.
void capability_aboutSeesTheOldStateAndTheTwinTheNew() {
  Mat m(kNaN);
  m.addRows({"r0", "r1", "r2"});
  m.addColumns({"c0", "c1"});
  struct Seen {
    qsizetype rows, columns, rowOfNew, rowOfOld;
  };
  Seen about{}, done{};
  const auto look = [&](Seen& into, const QString& oldKey, const QString& newKey) {
    into = {m.rowCount(), m.columnCount(), m.rowIndexOf(newKey), m.rowIndexOf(oldKey)};
  };
  QObject ctx;
  QString oldKey = "r1";
  QString newKey = "x";
  const auto watch = [&](auto aboutSignal, auto doneSignal) {
    QObject::connect(&m, aboutSignal, &ctx, [&](auto&&...) { look(about, oldKey, newKey); });
    QObject::connect(&m, doneSignal, &ctx, [&](auto&&...) { look(done, oldKey, newKey); });
  };
  watch(&Mat::rowAboutToBeAdded, &Mat::rowAdded);
  watch(&Mat::rowsAboutToBeAdded, &Mat::rowsAdded);
  watch(&Mat::columnAboutToBeAdded, &Mat::columnAdded);
  watch(&Mat::columnsAboutToBeAdded, &Mat::columnsAdded);
  watch(&Mat::rowAboutToBeRemoved, &Mat::rowRemoved);
  watch(&Mat::rowsAboutToBeRemoved, &Mat::rowsRemoved);
  watch(&Mat::columnAboutToBeRemoved, &Mat::columnRemoved);
  watch(&Mat::columnsAboutToBeRemoved, &Mat::columnsRemoved);
  watch(&Mat::rowAboutToBeRenamed, &Mat::rowRenamed);
  watch(&Mat::aboutToBeCleared, &Mat::cleared);

  CHECK(m.addRow("x"));
  CHECK(about.rows == 3 && about.rowOfNew == -1 && done.rows == 4 && done.rowOfNew == 3);
  CHECK(m.addRows({"y", "z"}) == 2);
  CHECK(about.rows == 4 && done.rows == 6);
  CHECK(m.addColumn("cx"));
  CHECK(about.columns == 2 && done.columns == 3);
  CHECK(m.addColumns({"cy", "cz"}) == 2);
  CHECK(about.columns == 3 && done.columns == 5);
  CHECK(m.removeRow("y"));
  CHECK(about.rows == 6 && done.rows == 5);
  CHECK(m.removeRows({"z", "r0"}) == 2);
  CHECK(about.rows == 4);  // two runs: the second is announced after the first took its row
  CHECK(done.rows == 3);
  CHECK(m.removeColumn("cx"));
  CHECK(about.columns == 5 && done.columns == 4);
  CHECK(m.removeColumns({"cy", "cz"}) == 2);
  CHECK(about.columns == 4 && done.columns == 2);
  oldKey = "r1";
  newKey = "renamed";
  CHECK(m.renameRow("r1", "renamed"));
  CHECK(about.rowOfOld == 0 && about.rowOfNew == -1 && done.rowOfOld == -1 && done.rowOfNew == 0);
  m.clear();
  CHECK(about.rows == 3 && about.columns == 2 && done.rows == 0 && done.columns == 0);
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  at_5_1_valueIsTotalAndReadingCreatesNothing();
  at_5_2_setValueNotifiesOnlyARealChange();
  at_5_2_theAboutSeesTheOldValueAndTheTwinTheNewOne();
  at_5_3_addRowsIsOnePairOverTheNewRange();
  at_5_4_sequenceT5RemoveRowsIsOnePairPerRunFromTheLast();
  at_5_5_renameRow();
  at_5_5_renameRowWithAReferenceIntoTheMatrixItself();
  at_5_5_renameColumn();
  at_5_6_aDenseBlockIsOneRectangle();
  at_5_6_sequenceT6AnLShapeIsTwoRectangles();
  at_5_6_unchangedCellsAreNotCountedAndTheLastOfADuplicateWins();
  at_5_6_aGapBetweenRowsEndsARectangle();
  at_5_6_theAboutsAllPrecedeTheWrites();
  at_5_7_clear();
  capability_aboutSeesTheOldStateAndTheTwinTheNew();
  capability_columnsAndRowsKeepTheirValuesThroughStructuralChanges();
  capability_removeColumnsRunsGoFromTheLastToTheFirst();
  capability_aNonNaNSentinelUsesEquality();
  return rqt_test::finish("capability_observable_matrix_signals");
}
