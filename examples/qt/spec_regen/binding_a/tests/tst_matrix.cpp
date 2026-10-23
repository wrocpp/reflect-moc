#include "obs/matrix.h"
#include "obs/matrix_table_model.h"

#include <QAbstractItemModelTester>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QtTest>

#include <cmath>
#include <random>
#include <limits>
#include <tuple>

namespace {

using MS = obs::MatrixSignals;
using Log = QList<QString>;
using Sheet = obs::Matrix<QString, QString, double>;
using Cell = std::tuple<QString, QString, double>;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

QString text(const QVariant & v)
{
    return v.value<QString>();
}

QString text(qsizetype i)
{
    return QString::number(i);
}

QString text(const QList<QVariant> & keys)
{
    QString out = QStringLiteral("[");
    for (qsizetype i = 0; i < keys.size(); ++i)
        out += (i ? QStringLiteral(",") : QString()) + text(keys[i]);
    return out + QStringLiteral("]");
}

// One recorder: connects to EVERY signal of the matrix (the keys are strings).
class Recorder : public QObject {
public:
    Log log;

    explicit Recorder(MS * s)
    {
        rec(s, &MS::rowAboutToBeAdded, "rowAboutToBeAdded");
        rec(s, &MS::rowAdded, "rowAdded");
        rec(s, &MS::columnAboutToBeAdded, "columnAboutToBeAdded");
        rec(s, &MS::columnAdded, "columnAdded");
        rec(s, &MS::rowsAboutToBeAdded, "rowsAboutToBeAdded");
        rec(s, &MS::rowsAdded, "rowsAdded");
        rec(s, &MS::columnsAboutToBeAdded, "columnsAboutToBeAdded");
        rec(s, &MS::columnsAdded, "columnsAdded");
        rec(s, &MS::rowAboutToBeRemoved, "rowAboutToBeRemoved");
        rec(s, &MS::rowRemoved, "rowRemoved");
        rec(s, &MS::columnAboutToBeRemoved, "columnAboutToBeRemoved");
        rec(s, &MS::columnRemoved, "columnRemoved");
        rec(s, &MS::rowsAboutToBeRemoved, "rowsAboutToBeRemoved");
        rec(s, &MS::rowsRemoved, "rowsRemoved");
        rec(s, &MS::columnsAboutToBeRemoved, "columnsAboutToBeRemoved");
        rec(s, &MS::columnsRemoved, "columnsRemoved");
        rec(s, &MS::rowAboutToBeRenamed, "rowAboutToBeRenamed");
        rec(s, &MS::rowRenamed, "rowRenamed");
        rec(s, &MS::columnAboutToBeRenamed, "columnAboutToBeRenamed");
        rec(s, &MS::columnRenamed, "columnRenamed");
        rec(s, &MS::cellAboutToChange, "cellAboutToChange");
        rec(s, &MS::cellChanged, "cellChanged");
        rec(s, &MS::cellsAboutToChange, "cellsAboutToChange");
        rec(s, &MS::cellsChanged, "cellsChanged");
        rec(s, &MS::aboutToBeCleared, "aboutToBeCleared");
        rec(s, &MS::cleared, "cleared");
    }

private:
    template <class Sig>
    void rec(MS * s, Sig sig, const char * name)
    {
        connect(s, sig, this, [this, name](const auto &... args) {
            QString line = QString::fromLatin1(name);
            ((line += QStringLiteral(" ") + text(args)), ...);
            log.append(line);
        });
    }
};

// Records every signal of a table model.
class ModelRecorder : public QObject {
public:
    Log log;

    explicit ModelRecorder(QAbstractItemModel * m)
    {
        using M = QAbstractItemModel;
        connect(m, &M::rowsInserted, this, [this](const QModelIndex &, int f, int l) { add("rowsInserted", f, l); });
        connect(m, &M::rowsRemoved, this, [this](const QModelIndex &, int f, int l) { add("rowsRemoved", f, l); });
        connect(m, &M::columnsInserted, this, [this](const QModelIndex &, int f, int l) { add("columnsInserted", f, l); });
        connect(m, &M::columnsRemoved, this, [this](const QModelIndex &, int f, int l) { add("columnsRemoved", f, l); });
        connect(m, &M::rowsMoved, this, [this] { log.append(QStringLiteral("rowsMoved")); });
        connect(m, &M::modelReset, this, [this] { log.append(QStringLiteral("modelReset")); });
        connect(m, &M::headerDataChanged, this, [this](Qt::Orientation o, int f, int l) {
            add(o == Qt::Vertical ? "headerDataChanged Vertical" : "headerDataChanged Horizontal", f, l);
        });
        connect(m, &M::dataChanged, this, [this](const QModelIndex & a, const QModelIndex & b) {
            log.append(QStringLiteral("dataChanged ") + QString::number(a.row()) + QStringLiteral(",")
                       + QString::number(a.column()) + QStringLiteral("-") + QString::number(b.row())
                       + QStringLiteral(",") + QString::number(b.column()));
        });
    }

private:
    void add(const char * name, int first, int last)
    {
        log.append(QString::fromLatin1(name) + QStringLiteral(" ") + QString::number(first) + QStringLiteral(" ")
                   + QString::number(last));
    }
};

Sheet makeSheet()
{
    return Sheet(kNaN);
}

QList<QString> keysNamed(const QString & prefix, int count)
{
    QList<QString> keys;
    for (int i = 0; i < count; ++i)
        keys.append(prefix + QString::number(i));
    return keys;
}

QAbstractItemModelTester::FailureReportingMode constexpr kQtTest = QAbstractItemModelTester::FailureReportingMode::QtTest;

void verifyModelMatches(const QAbstractItemModel & model, const Sheet & sheet)
{
    QCOMPARE(model.rowCount(), int(sheet.rowCount()));
    QCOMPARE(model.columnCount(), int(sheet.columnCount()));
    for (int r = 0; r < model.rowCount(); ++r) {
        QCOMPARE(model.headerData(r, Qt::Vertical).toString(), sheet.rowKeys()[r]);
        for (int c = 0; c < model.columnCount(); ++c) {
            const QVariant shown = model.data(model.index(r, c));
            const double stored = sheet.valueAt(r, c);
            QCOMPARE(shown.isValid(), !std::isnan(stored));
            if (shown.isValid())
                QCOMPARE(shown.toDouble(), stored);
        }
    }
    for (int c = 0; c < model.columnCount(); ++c)
        QCOMPARE(model.headerData(c, Qt::Horizontal).toString(), sheet.columnKeys()[c]);
}


// After every step of a random mix of appends, removals (single and batch), renames and clears, rowIndexOf and
// columnIndexOf agree with the key lists for every name of a small universe, present or not.
inline bool matrixIndexesAgree(unsigned seed)
{
    constexpr int kUniverse = 12;
    constexpr int kSteps = 600;
    Sheet m(0.0);
    std::mt19937 rng(seed);
    const auto name = [&](char prefix) { return QString(prefix) + QString::number(static_cast<int>(rng() % kUniverse)); };
    const auto names = [&](char prefix) {
        QList<QString> out;
        for (int i = 0; i < 3; ++i)
            out.append(name(prefix));
        return out;
    };
    for (int step = 0; step < kSteps; ++step) {
        switch (rng() % 11) {
        case 0: m.addRow(name('r')); break;
        case 1: m.addColumn(name('c')); break;
        case 2: m.addRows(names('r')); break;
        case 3: m.addColumns(names('c')); break;
        case 4: m.removeRow(name('r')); break;
        case 5: m.removeColumn(name('c')); break;
        case 6: m.removeRows(names('r')); break;
        case 7: m.removeColumns(names('c')); break;
        case 8: m.renameRow(name('r'), name('r')); break;
        case 9: m.renameColumn(name('c'), name('c')); break;
        default:
            if (rng() % 10 == 0)
                m.clear();
            break;
        }
        for (int k = 0; k < kUniverse; ++k) {
            const QString r = QStringLiteral("r") + QString::number(k);
            const QString c = QStringLiteral("c") + QString::number(k);
            if (m.rowIndexOf(r) != m.rowKeys().indexOf(r) || m.columnIndexOf(c) != m.columnKeys().indexOf(c))
                return false;
        }
    }
    return true;
}

}  // namespace

class tst_matrix : public QObject {
    Q_OBJECT
private slots:
    void at_5_1_readingNeverCreatesStructure()
    {
        Sheet m = makeSheet();
        m.addRow("r");
        m.addColumn("c");
        Recorder rec(&m);
        QVERIFY(std::isnan(m.value("absent", "c")));
        QVERIFY(std::isnan(m.value("r", "absent")));
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.columnCount(), 1);
        QVERIFY(!m.setValue("r", "absent", 1.0));
        QVERIFY(!m.setValue("absent", "c", 1.0));
        QVERIFY(rec.log.isEmpty());
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.columnCount(), 1);
    }

    void at_5_2_setValue()
    {
        Sheet m = makeSheet();
        m.addRows(QList<QString>{"r0", "r1"});
        m.addColumns(QList<QString>{"c0", "c1"});
        Recorder rec(&m);

        QVERIFY(m.setValue("r1", "c1", 2.5));
        QCOMPARE(rec.log, (Log{"cellAboutToChange r1 c1 1 1", "cellChanged r1 c1 1 1"}));
        QCOMPARE(m.value("r1", "c1"), 2.5);

        rec.log.clear();
        QVERIFY(m.setValue("r1", "c1", 2.5));
        QVERIFY(rec.log.isEmpty());

        QVERIFY(m.setValue("r0", "c0", kNaN));  // NaN over an uninitialised (NaN) cell
        QVERIFY(rec.log.isEmpty());
        QVERIFY(m.isUninitialized(m.value("r0", "c0")));
    }

    void at_5_3_addRowsIsOnePair()
    {
        Sheet m = makeSheet();
        m.addRows(QList<QString>{"rx", "ry"});
        Recorder rec(&m);

        QCOMPARE(m.addRows(QList<QString>{"r1", "r2", "r1", "rx"}), 2);
        QCOMPARE(rec.log, (Log{"rowsAboutToBeAdded 2 3 [r1,r2]", "rowsAdded 2 3 [r1,r2]"}));
        QCOMPARE(m.rowCount(), 4);

        rec.log.clear();
        QCOMPARE(m.addRows(QList<QString>{"rx", "rx"}), 0);
        QVERIFY(rec.log.isEmpty());
    }

    void at_5_4_removeRowsIsOnePairPerRunFromTheLast()
    {
        Sheet m = makeSheet();
        m.addRows(keysNamed("k", 8));
        Recorder rec(&m);

        QCOMPARE(m.removeRows(QList<QString>{"k1", "k2", "k5", "k7", "k5", "zz"}), 4);
        QCOMPARE(rec.log, (Log{"rowsAboutToBeRemoved 7 7 [k7]", "rowsRemoved 7 7 [k7]", "rowsAboutToBeRemoved 5 5 [k5]",
                               "rowsRemoved 5 5 [k5]", "rowsAboutToBeRemoved 1 2 [k1,k2]", "rowsRemoved 1 2 [k1,k2]"}));
        QCOMPARE(m.rowKeys(), (QList<QString>{"k0", "k3", "k4", "k6"}));
    }

    void at_5_5_renameRow()
    {
        Sheet m = makeSheet();
        m.addRows(QList<QString>{"s1", "s2", "s3"});
        m.addColumn("c");
        m.setValue("s2", "c", 4.0);
        Recorder rec(&m);

        QVERIFY(m.renameRow("s1", "s1"));
        QVERIFY(!m.renameRow("nope", "nope"));
        QVERIFY(rec.log.isEmpty());
        QVERIFY(!m.renameRow("nope", "fresh"));
        QCOMPARE(m.rowCount(), 3);
        QVERIFY(!m.renameRow("s1", "s3"));
        QCOMPARE(m.rowKeys(), (QList<QString>{"s1", "s2", "s3"}));
        QVERIFY(rec.log.isEmpty());

        QCOMPARE(m.rowIndexOf("s2"), 1);  // primes the lookup map just before the rename
        QVERIFY(m.renameRow("s2", "s2b"));
        QCOMPARE(rec.log, (Log{"rowAboutToBeRenamed s2 s2b 1", "rowRenamed s2 s2b 1"}));
        QCOMPARE(m.rowIndexOf("s2b"), 1);
        QCOMPARE(m.rowIndexOf("s2"), -1);
        QCOMPARE(m.value("s2b", "c"), 4.0);
    }

    void at_5_5_renameColumnMirrorsRow()
    {
        Sheet m = makeSheet();
        m.addColumns(QList<QString>{"a", "b"});
        QCOMPARE(m.columnIndexOf("a"), 0);
        QVERIFY(m.renameColumn("a", "z"));
        QCOMPARE(m.columnIndexOf("z"), 0);
        QCOMPARE(m.columnIndexOf("a"), -1);
        QVERIFY(!m.renameColumn("z", "b"));
    }

    void at_5_6_setValuesDecomposesIntoFullRectangles()
    {
        Sheet m = makeSheet();
        m.addRows(keysNamed("r", 3));
        m.addColumns(keysNamed("c", 4));
        Recorder rec(&m);

        QList<Cell> dense;
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                dense.append(Cell{"r" + QString::number(r), "c" + QString::number(c), r * 10.0 + c});
        QCOMPARE(m.setValues(dense), 12);
        QCOMPARE(rec.log, (Log{"cellsAboutToChange 0 0 2 3", "cellsChanged 0 0 2 3"}));

        // sequence T6, on a fresh matrix: the L shape
        Sheet l = makeSheet();
        l.addRows(keysNamed("r", 2));
        l.addColumns(keysNamed("c", 2));
        Recorder lrec(&l);
        QCOMPARE(l.setValues(QList<Cell>{Cell{"r0", "c0", 1.0}, Cell{"r0", "c1", 2.0}, Cell{"r1", "c0", 3.0}}), 3);
        QCOMPARE(lrec.log, (Log{"cellsAboutToChange 0 0 0 1", "cellsAboutToChange 1 0 1 0", "cellsChanged 0 0 0 1",
                                "cellsChanged 1 0 1 0"}));

        // unchanged cells are neither counted nor announced; absent keys are skipped
        lrec.log.clear();
        QCOMPARE(l.setValues(QList<Cell>{Cell{"r0", "c0", 1.0}, Cell{"r0", "c1", 20.0}, Cell{"zz", "c0", 1.0}}), 1);
        QCOMPARE(lrec.log, (Log{"cellsAboutToChange 0 1 0 1", "cellsChanged 0 1 0 1"}));

        // a cell given twice takes the last value
        lrec.log.clear();
        QCOMPARE(l.setValues(QList<Cell>{Cell{"r1", "c1", 5.0}, Cell{"r1", "c1", 6.0}}), 1);
        QCOMPARE(l.value("r1", "c1"), 6.0);
        QCOMPARE(lrec.log.size(), 2);

        // nothing left: nothing emitted
        lrec.log.clear();
        QCOMPARE(l.setValues(QList<Cell>{Cell{"r1", "c1", 6.0}}), 0);
        QVERIFY(lrec.log.isEmpty());
    }

    void at_5_7_clear()
    {
        Sheet m = makeSheet();
        Recorder rec(&m);
        m.clear();
        QVERIFY(rec.log.isEmpty());

        m.addRow("r");
        m.addColumn("c");
        rec.log.clear();
        m.clear();
        QCOMPARE(rec.log, (Log{"aboutToBeCleared", "cleared"}));
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.columnCount(), 0);
    }

    void at_6_1_modelFollowsEveryStructuralStep()
    {
        Sheet m = makeSheet();
        obs::MatrixTableModel<Sheet> model(m);
        QAbstractItemModelTester tester(&model, kQtTest);

        m.addRow("r0");
        verifyModelMatches(model, m);
        m.addColumn("c0");
        verifyModelMatches(model, m);
        m.addRows(QList<QString>{"r1", "r2", "r3"});
        m.addColumns(QList<QString>{"c1", "c2"});
        verifyModelMatches(model, m);
        m.setValue("r1", "c1", 1.5);
        verifyModelMatches(model, m);
        m.removeRow("r0");
        m.removeColumn("c0");
        verifyModelMatches(model, m);
        m.removeRows(QList<QString>{"r1", "r3"});
        m.removeColumns(QList<QString>{"c1", "c2"});
        verifyModelMatches(model, m);
        m.clear();
        verifyModelMatches(model, m);
    }

    void at_6_2_renameIsOnlyAHeaderChange()
    {
        Sheet m = makeSheet();
        m.addRows(QList<QString>{"s1", "s2", "s3"});
        obs::MatrixTableModel<Sheet> model(m);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);
        Recorder source(&m);

        QVERIFY(m.renameRow("s2", "s2b"));
        QCOMPARE(source.log, (Log{"rowAboutToBeRenamed s2 s2b 1", "rowRenamed s2 s2b 1"}));
        QCOMPARE(rec.log, (Log{"headerDataChanged Vertical 1 1"}));
        QCOMPARE(model.headerData(1, Qt::Vertical).toString(), QStringLiteral("s2b"));
    }

    void at_6_3_dataChangedMatchesTheRectangles()
    {
        Sheet m = makeSheet();
        m.addRows(QList<QString>{"r0", "r1"});
        m.addColumns(QList<QString>{"c0", "c1"});
        obs::MatrixTableModel<Sheet> model(m);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);

        m.setValue("r1", "c0", 3.0);
        QCOMPARE(rec.log, (Log{"dataChanged 1,0-1,0"}));
        rec.log.clear();
        m.setValues(QList<Cell>{Cell{"r0", "c0", 1.0}, Cell{"r0", "c1", 2.0}, Cell{"r1", "c0", 9.0}});
        QCOMPARE(rec.log, (Log{"dataChanged 0,0-0,1", "dataChanged 1,0-1,0"}));
    }

    void at_6_4_uninitialisedCellShowsNothing()
    {
        Sheet m = makeSheet();
        m.addRow("r");
        m.addColumn("c");
        obs::MatrixTableModel<Sheet> model(m);
        QAbstractItemModelTester tester(&model, kQtTest);
        QVERIFY(!model.data(model.index(0, 0), Qt::DisplayRole).isValid());
        m.setValue("r", "c", 0.0);
        QVERIFY(model.data(model.index(0, 0), Qt::DisplayRole).isValid());
        QVERIFY(!model.data(model.index(0, 0), Qt::EditRole).isValid());
    }

    // Not acceptance tests: a column added after rows, and removal of runs of columns.
    void extra_columnsAreInsertedIntoEveryRow()
    {
        Sheet m = makeSheet();
        m.addRows(QList<QString>{"r0", "r1", "r2"});
        m.addColumn("c0");
        m.setValue("r0", "c0", 1.0);
        m.setValue("r1", "c0", 2.0);
        m.setValue("r2", "c0", 3.0);
        m.addColumns(QList<QString>{"c1", "c2"});
        QCOMPARE(m.value("r0", "c0"), 1.0);
        QCOMPARE(m.value("r1", "c0"), 2.0);
        QCOMPARE(m.value("r2", "c0"), 3.0);
        QVERIFY(std::isnan(m.value("r2", "c2")));
        m.setValue("r1", "c2", 7.0);
        QCOMPARE(m.removeColumns(QList<QString>{"c0", "c1"}), 2);
        QCOMPARE(m.columnCount(), 1);
        QCOMPARE(m.value("r1", "c2"), 7.0);
        QVERIFY(std::isnan(m.value("r0", "c2")));
    }

    void extra_columnRunsAreAnnouncedFromTheLast()
    {
        Sheet m = makeSheet();
        m.addColumns(keysNamed("c", 6));
        Recorder rec(&m);
        QCOMPARE(m.removeColumns(QList<QString>{"c0", "c4", "c5", "c1"}), 4);
        QCOMPARE(rec.log, (Log{"columnsAboutToBeRemoved 4 5 [c4,c5]", "columnsRemoved 4 5 [c4,c5]",
                               "columnsAboutToBeRemoved 0 1 [c0,c1]", "columnsRemoved 0 1 [c0,c1]"}));
    }

    // A non-NaN sentinel is compared with ==.
    void extra_integerSentinel()
    {
        obs::Matrix<int, int, int> m(-1);
        m.addRow(1);
        m.addColumn(2);
        QVERIFY(m.isUninitialized(m.value(1, 2)));
        QVERIFY(m.setValue(1, 2, 5));
        QVERIFY(!m.isUninitialized(m.value(1, 2)));
        QVERIFY(m.setValue(1, 2, -1));
        QVERIFY(m.isUninitialized(m.value(1, 2)));
    }

    // Added after mutation testing (three mutants survived: a rectangle joined across a gap of rows, a removed middle column took the
    // wrong cells, a cell was written before its "about").
    void at_5_6_aGapBetweenRowsEndsARectangle()
    {
        Sheet m = makeSheet();
        m.addRows(keysNamed("r", 4));
        m.addColumns(keysNamed("c", 2));
        Recorder rec(&m);
        QCOMPARE(m.setValues(QList<Cell>{Cell{"r0", "c0", 1.0}, Cell{"r0", "c1", 1.0}, Cell{"r1", "c0", 1.0}, Cell{"r1", "c1", 1.0},
                                         Cell{"r3", "c0", 1.0}, Cell{"r3", "c1", 1.0}}),
                 6);
        QCOMPARE(rec.log, (Log{"cellsAboutToChange 0 0 1 1", "cellsAboutToChange 3 0 3 1", "cellsChanged 0 0 1 1", "cellsChanged 3 0 3 1"}));
    }

    void capability_aMiddleColumnRemovalKeepsTheValues()
    {
        Sheet m = makeSheet();
        m.addRows(keysNamed("r", 3));
        m.addColumns(keysNamed("c", 4));
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                m.setValue("r" + QString::number(r), "c" + QString::number(c), r * 10.0 + c);
        QVERIFY(m.removeColumn("c1"));
        QCOMPARE(m.value("r0", "c0"), 0.0);
        QCOMPARE(m.value("r1", "c2"), 12.0);
        QCOMPARE(m.value("r2", "c3"), 23.0);
        QCOMPARE(m.removeColumns(QList<QString>{"c2", "c3"}), 2);
        QCOMPARE(m.value("r1", "c0"), 10.0);
        QCOMPARE(m.value("r2", "c0"), 20.0);
    }

    void at_5_2_theAboutSeesTheOldValueAndTheTwinTheNew()
    {
        Sheet m = makeSheet();
        m.addRows(keysNamed("r", 1));
        m.addColumns(keysNamed("c", 1));
        m.setValue("r0", "c0", 1.0);
        double atAbout = 0, atTwin = 0;
        QObject ctx;
        connect(&m, &MS::cellAboutToChange, &ctx, [&] { atAbout = m.value("r0", "c0"); });
        connect(&m, &MS::cellChanged, &ctx, [&] { atTwin = m.value("r0", "c0"); });
        QVERIFY(m.setValue("r0", "c0", 7.0));
        QCOMPARE(atAbout, 1.0);
        QCOMPARE(atTwin, 7.0);
    }

    void at_5_7_clearForgetsKeysThatWereLookedUp()
    {
        Sheet m = makeSheet();
        m.addRows(keysNamed("r", 1));
        m.addColumns(keysNamed("c", 1));
        QCOMPARE(m.rowIndexOf("r0"), 0);
        QCOMPARE(m.columnIndexOf("c0"), 0);
        m.clear();
        QCOMPARE(m.rowIndexOf("r0"), -1);
        QCOMPARE(m.columnIndexOf("c0"), -1);
    }

    // The row and column indexes are kept in step with the key lists on every change, so a lookup never rebuilds them.
    void capability_theKeyIndexesAgreeWithTheKeyListsAfterEveryStep()
    {
        for (unsigned seed = 1; seed <= 8; ++seed)
            QVERIFY(matrixIndexesAgree(seed));
    }
};

QTEST_GUILESS_MAIN(tst_matrix)
#include "tst_matrix.moc"
