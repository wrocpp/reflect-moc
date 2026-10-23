// SPEC 12.5 and 12.6: the matrix and its table model, binding B.
#include <obs/matrix.h>
#include <obs/matrix_table_model.h>

#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <reflect_moc/qt/qt.hpp>

#include <cmath>
#include <random>
#include <limits>

namespace {

constexpr auto kTesterMode = QAbstractItemModelTester::FailureReportingMode::QtTest;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

using Mat = obs::Matrix<QString, QString, double>;
using Cells = QList<std::tuple<QString, QString, double>>;

QString keysText(const QList<QString> & keys)
{
    return QLatin1Char('[') + keys.join(QLatin1Char(',')) + QLatin1Char(']');
}

// One object connected to EVERY signal of the matrix, appending the name and the payload.
class Recorder {
public:
    explicit Recorder(Mat & m)
    {
        const auto keyIndex = [&](const char * name, auto signal) {
            QObject::connect(&m, signal, &context, [this, name](const QString & k, qsizetype i) { log << QStringLiteral("%1(%2,%3)").arg(QLatin1String(name), k).arg(i); });
        };
        const auto range = [&](const char * name, auto signal) {
            QObject::connect(&m, signal, &context, [this, name](qsizetype f, qsizetype l, const QList<QString> & ks) {
                log << QStringLiteral("%1(%2,%3,%4)").arg(QLatin1String(name)).arg(f).arg(l).arg(keysText(ks));
            });
        };
        const auto rename = [&](const char * name, auto signal) {
            QObject::connect(&m, signal, &context, [this, name](const QString & a, const QString & b, qsizetype i) {
                log << QStringLiteral("%1(%2,%3,%4)").arg(QLatin1String(name), a, b).arg(i);
            });
        };
        const auto cell = [&](const char * name, auto signal) {
            QObject::connect(&m, signal, &context, [this, name](const QString & r, const QString & c, qsizetype i, qsizetype j) {
                log << QStringLiteral("%1(%2,%3,%4,%5)").arg(QLatin1String(name), r, c).arg(i).arg(j);
            });
        };
        const auto rect = [&](const char * name, auto signal) {
            QObject::connect(&m, signal, &context, [this, name](qsizetype t, qsizetype l, qsizetype b, qsizetype r) {
                log << QStringLiteral("%1(%2,%3,%4,%5)").arg(QLatin1String(name)).arg(t).arg(l).arg(b).arg(r);
            });
        };
        const auto none = [&](const char * name, auto signal) { QObject::connect(&m, signal, &context, [this, name] { log << QLatin1String(name); }); };

        keyIndex("rowAboutToBeAdded", &Mat::rowAboutToBeAdded);
        keyIndex("rowAdded", &Mat::rowAdded);
        keyIndex("columnAboutToBeAdded", &Mat::columnAboutToBeAdded);
        keyIndex("columnAdded", &Mat::columnAdded);
        keyIndex("rowAboutToBeRemoved", &Mat::rowAboutToBeRemoved);
        keyIndex("rowRemoved", &Mat::rowRemoved);
        keyIndex("columnAboutToBeRemoved", &Mat::columnAboutToBeRemoved);
        keyIndex("columnRemoved", &Mat::columnRemoved);
        range("rowsAboutToBeAdded", &Mat::rowsAboutToBeAdded);
        range("rowsAdded", &Mat::rowsAdded);
        range("columnsAboutToBeAdded", &Mat::columnsAboutToBeAdded);
        range("columnsAdded", &Mat::columnsAdded);
        range("rowsAboutToBeRemoved", &Mat::rowsAboutToBeRemoved);
        range("rowsRemoved", &Mat::rowsRemoved);
        range("columnsAboutToBeRemoved", &Mat::columnsAboutToBeRemoved);
        range("columnsRemoved", &Mat::columnsRemoved);
        rename("rowAboutToBeRenamed", &Mat::rowAboutToBeRenamed);
        rename("rowRenamed", &Mat::rowRenamed);
        rename("columnAboutToBeRenamed", &Mat::columnAboutToBeRenamed);
        rename("columnRenamed", &Mat::columnRenamed);
        cell("cellAboutToChange", &Mat::cellAboutToChange);
        cell("cellChanged", &Mat::cellChanged);
        rect("cellsAboutToChange", &Mat::cellsAboutToChange);
        rect("cellsChanged", &Mat::cellsChanged);
        none("aboutToBeCleared", &Mat::aboutToBeCleared);
        none("cleared", &Mat::cleared);
    }

    QObject context;
    QStringList log;
};

QList<QString> keys(const char * prefix, int n)
{
    QList<QString> out;
    for (int i = 0; i < n; ++i)
        out << QStringLiteral("%1%2").arg(QLatin1String(prefix)).arg(i);
    return out;
}

// the table model mirrors the matrix: counts, cells and headers
bool mirrors(const obs::MatrixTableModel<Mat> & model, const Mat & m)
{
    if (model.rowCount() != m.rowCount() || model.columnCount() != m.columnCount())
        return false;
    for (int r = 0; r < model.rowCount(); ++r) {
        if (model.headerData(r, Qt::Vertical).toString() != m.rowKeys()[r])
            return false;
        for (int c = 0; c < model.columnCount(); ++c) {
            const QVariant shown = model.data(model.index(r, c), Qt::DisplayRole);
            const double actual = m.valueAt(r, c);
            if (m.isUninitialized(actual) ? shown.isValid() : shown.toDouble() != actual)
                return false;
        }
    }
    for (int c = 0; c < model.columnCount(); ++c)
        if (model.headerData(c, Qt::Horizontal).toString() != m.columnKeys()[c])
            return false;
    return true;
}


// After every step of a random mix of appends, removals (single and batch), renames and clears, rowIndexOf and
// columnIndexOf agree with the key lists for every name of a small universe, present or not.
inline bool matrixIndexesAgree(unsigned seed)
{
    constexpr int kUniverse = 12;
    constexpr int kSteps = 600;
    Mat m(0.0);
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

class TstMatrix : public QObject {
    RQT_OBJECT

private slots:
    [[= rqt::slot]] void at_5_1_valueIsTotalAndReadingCreatesNothing()
    {
        Mat m(kNaN);
        m.addRow("r");
        m.addColumn("c");
        Recorder rec(m);
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

    [[= rqt::slot]] void at_5_2_setValueNotifiesOnlyARealChange()
    {
        Mat m(kNaN);
        m.addRows({"r0", "r1"});
        m.addColumns({"c0", "c1"});
        Recorder rec(m);

        QVERIFY(m.setValue("r1", "c0", 2.5));
        QCOMPARE(rec.log, (QStringList{"cellAboutToChange(r1,c0,1,0)", "cellChanged(r1,c0,1,0)"}));
        QCOMPARE(m.value("r1", "c0"), 2.5);

        rec.log.clear();
        QVERIFY(m.setValue("r1", "c0", 2.5));
        QVERIFY(rec.log.isEmpty());

        QVERIFY(m.setValue("r0", "c1", kNaN));  // a NaN over an uninitialised (NaN) cell
        QVERIFY(rec.log.isEmpty());
    }

    [[= rqt::slot]] void at_5_2_theAboutSeesTheOldValueAndTheTwinTheNewOne()
    {
        Mat m(kNaN);
        m.addRow("r");
        m.addColumn("c");
        m.setValue("r", "c", 1.0);
        double seenAbout = 0, seenTwin = 0;
        QObject ctx;
        QObject::connect(&m, &Mat::cellAboutToChange, &ctx, [&] { seenAbout = m.value("r", "c"); });
        QObject::connect(&m, &Mat::cellChanged, &ctx, [&] { seenTwin = m.value("r", "c"); });
        m.setValue("r", "c", 7.0);
        QCOMPARE(seenAbout, 1.0);
        QCOMPARE(seenTwin, 7.0);
    }

    [[= rqt::slot]] void at_5_3_addRowsIsOnePairOverTheNewRange()
    {
        Mat m(kNaN);
        m.addRows({"p", "q", "rx"});
        Recorder rec(m);
        QCOMPARE(m.addRows({"r1", "r2", "r1", "rx"}), 2);
        QCOMPARE(rec.log, (QStringList{"rowsAboutToBeAdded(3,4,[r1,r2])", "rowsAdded(3,4,[r1,r2])"}));
        QCOMPARE(m.rowKeys(), (QList<QString>{"p", "q", "rx", "r1", "r2"}));
        rec.log.clear();
        QCOMPARE(m.addRows({"p", "q"}), 0);
        QVERIFY(rec.log.isEmpty());
    }

    [[= rqt::slot]] void at_5_4_sequenceT5RemoveRowsIsOnePairPerRunFromTheLast()
    {
        Mat m(kNaN);
        m.addRows(keys("k", 8));
        Recorder rec(m);
        QCOMPARE(m.removeRows({"k1", "k2", "k5", "k7"}), 4);
        QCOMPARE(rec.log, (QStringList{"rowsAboutToBeRemoved(7,7,[k7])", "rowsRemoved(7,7,[k7])", "rowsAboutToBeRemoved(5,5,[k5])", "rowsRemoved(5,5,[k5])",
                                       "rowsAboutToBeRemoved(1,2,[k1,k2])", "rowsRemoved(1,2,[k1,k2])"}));
        QCOMPARE(m.rowKeys(), (QList<QString>{"k0", "k3", "k4", "k6"}));
    }

    [[= rqt::slot]] void at_5_5_renameRow()
    {
        Mat m(kNaN);
        m.addRows({"s1", "s2", "s3"});
        m.addColumn("c");
        m.setValue("s2", "c", 4.0);
        Recorder rec(m);

        QVERIFY(m.renameRow("s1", "s1"));
        QVERIFY(!m.renameRow("nope", "nope"));
        QVERIFY(!m.renameRow("nope", "x"));
        QVERIFY(!m.renameRow("s1", "s3"));
        QVERIFY(rec.log.isEmpty());
        QCOMPARE(m.rowKeys(), (QList<QString>{"s1", "s2", "s3"}));
        QCOMPARE(m.rowCount(), 3);

        QCOMPARE(m.rowIndexOf("s2"), 1);  // the lookup map is built, then the rename must stale it
        QVERIFY(m.renameRow("s2", "s2b"));
        QCOMPARE(rec.log, (QStringList{"rowAboutToBeRenamed(s2,s2b,1)", "rowRenamed(s2,s2b,1)"}));
        QCOMPARE(m.rowIndexOf("s2b"), 1);
        QCOMPARE(m.rowIndexOf("s2"), -1);
        QCOMPARE(m.value("s2b", "c"), 4.0);
        QCOMPARE(m.rowCount(), 3);
    }

    [[= rqt::slot]] void at_5_5_renameRowWithAReferenceIntoTheMatrixItself()
    {
        Mat m(kNaN);
        m.addRows({"s1", "s2"});
        Recorder rec(m);
        QVERIFY(m.renameRow(m.rowKeys()[1], "z"));
        QCOMPARE(rec.log, (QStringList{"rowAboutToBeRenamed(s2,z,1)", "rowRenamed(s2,z,1)"}));
    }

    [[= rqt::slot]] void at_5_5_renameColumn()
    {
        Mat m(kNaN);
        m.addRow("r");
        m.addColumns({"a", "b"});
        m.setValue("r", "b", 3.0);
        Recorder rec(m);
        QCOMPARE(m.columnIndexOf("b"), 1);
        QVERIFY(m.renameColumn("b", "b2"));
        QCOMPARE(rec.log, (QStringList{"columnAboutToBeRenamed(b,b2,1)", "columnRenamed(b,b2,1)"}));
        QCOMPARE(m.columnIndexOf("b"), -1);
        QCOMPARE(m.columnIndexOf("b2"), 1);
        QCOMPARE(m.value("r", "b2"), 3.0);
        QVERIFY(!m.renameColumn("a", "b2"));
        QVERIFY(m.renameColumn("a", "a"));
    }

    [[= rqt::slot]] void at_5_6_aDenseBlockIsOneRectangle()
    {
        Mat m(kNaN);
        m.addRows(keys("r", 4));
        m.addColumns(keys("c", 5));
        Recorder rec(m);
        Cells cells;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 4; ++j)
                cells << std::make_tuple(QStringLiteral("r%1").arg(i), QStringLiteral("c%1").arg(j), i * 10.0 + j);
        QCOMPARE(m.setValues(cells), 12);
        QCOMPARE(rec.log, (QStringList{"cellsAboutToChange(0,0,2,3)", "cellsChanged(0,0,2,3)"}));
        QCOMPARE(m.value("r2", "c3"), 23.0);
        QVERIFY(std::isnan(m.value("r3", "c0")));
        QVERIFY(std::isnan(m.value("r0", "c4")));
    }

    [[= rqt::slot]] void at_5_6_sequenceT6AnLShapeIsTwoRectangles()
    {
        Mat m(kNaN);
        m.addRows({"r0", "r1"});
        m.addColumns({"c0", "c1"});
        Recorder rec(m);
        QCOMPARE(m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r1", "c0", 3.0}}), 3);
        QCOMPARE(rec.log, (QStringList{"cellsAboutToChange(0,0,0,1)", "cellsAboutToChange(1,0,1,0)", "cellsChanged(0,0,0,1)", "cellsChanged(1,0,1,0)"}));
    }

    [[= rqt::slot]] void at_5_6_unchangedCellsAreNotCountedAndTheLastOfADuplicateWins()
    {
        Mat m(kNaN);
        m.addRows({"r0", "r1"});
        m.addColumns({"c0", "c1"});
        m.setValue("r0", "c0", 5.0);
        Recorder rec(m);

        QCOMPARE(m.setValues({{"r0", "c0", 5.0}, {"r1", "c1", 8.0}, {"nope", "c0", 1.0}, {"r0", "nope", 1.0}}), 1);
        QCOMPARE(rec.log, (QStringList{"cellsAboutToChange(1,1,1,1)", "cellsChanged(1,1,1,1)"}));

        rec.log.clear();
        QCOMPARE(m.setValues({{"r0", "c1", 1.0}, {"r0", "c1", 2.0}}), 1);
        QCOMPARE(m.value("r0", "c1"), 2.0);

        rec.log.clear();
        // the last occurrence wins even when it equals the current value: nothing changes
        QCOMPARE(m.setValues({{"r0", "c1", 9.0}, {"r0", "c1", 2.0}}), 0);
        QVERIFY(rec.log.isEmpty());
    }

    [[= rqt::slot]] void at_5_6_aGapBetweenRowsEndsARectangle()
    {
        Mat m(kNaN);
        m.addRows(keys("r", 4));
        m.addColumns({"c0", "c1"});
        Recorder rec(m);
        QCOMPARE(m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 1.0}, {"r1", "c0", 1.0}, {"r1", "c1", 1.0}, {"r3", "c0", 1.0}, {"r3", "c1", 1.0}}), 6);
        QCOMPARE(rec.log, (QStringList{"cellsAboutToChange(0,0,1,1)", "cellsAboutToChange(3,0,3,1)", "cellsChanged(0,0,1,1)", "cellsChanged(3,0,3,1)"}));
    }

    [[= rqt::slot]] void at_5_6_theAboutsAllPrecedeTheWrites()
    {
        Mat m(kNaN);
        m.addRows({"r0", "r1"});
        m.addColumns({"c0", "c1"});
        QList<double> seenAtAbouts, seenAtTwins;
        QObject ctx;
        QObject::connect(&m, &Mat::cellsAboutToChange, &ctx, [&] { seenAtAbouts << m.valueAt(1, 0) << m.valueAt(0, 0); });
        QObject::connect(&m, &Mat::cellsChanged, &ctx, [&] { seenAtTwins << m.valueAt(1, 0) << m.valueAt(0, 0); });
        m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r1", "c0", 3.0}});
        for (double v : seenAtAbouts)
            QVERIFY(std::isnan(v));
        for (double v : seenAtTwins)
            QVERIFY(!std::isnan(v));
        QCOMPARE(seenAtAbouts.size(), 4);
    }

    [[= rqt::slot]] void at_5_7_clear()
    {
        Mat m(kNaN);
        Recorder rec(m);
        m.clear();
        QVERIFY(rec.log.isEmpty());

        m.addRow("r");
        m.addColumn("c");
        rec.log.clear();
        m.clear();
        QCOMPARE(rec.log, (QStringList{"aboutToBeCleared", "cleared"}));
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.columnCount(), 0);
        QCOMPARE(m.rowIndexOf("r"), -1);
        QVERIFY(m.addRow("r"));  // the lookup map was staled
    }

    // The storage is row-major: every structural change must keep the values where they belong.
    [[= rqt::slot]] void capability_columnsAndRowsKeepTheirValuesThroughStructuralChanges()
    {
        Mat m(kNaN);
        m.addRows({"a", "b", "c"});
        m.addColumns({"x", "y"});
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 2; ++j)
                m.setValue(m.rowKeys()[i], m.columnKeys()[j], 10.0 * (i + 1) + j);

        m.addColumn("z");
        QCOMPARE(m.value("b", "y"), 21.0);
        QVERIFY(std::isnan(m.value("b", "z")));
        m.setValue("c", "z", 99.0);

        m.addColumns({"p", "q"});
        QCOMPARE(m.value("c", "z"), 99.0);
        QCOMPARE(m.value("a", "x"), 10.0);
        QVERIFY(std::isnan(m.value("c", "q")));

        QCOMPARE(m.removeColumns({"y", "q", "p"}), 3);  // two runs: [1,1] and [3,4]
        QCOMPARE(m.columnKeys(), (QList<QString>{"x", "z"}));
        QCOMPARE(m.value("a", "x"), 10.0);
        QCOMPARE(m.value("b", "x"), 20.0);
        QCOMPARE(m.value("c", "z"), 99.0);

        QVERIFY(m.removeRow("b"));
        QCOMPARE(m.value("c", "x"), 30.0);
        QCOMPARE(m.value("c", "z"), 99.0);
        m.addRow("d");
        QVERIFY(std::isnan(m.value("d", "x")));
        QVERIFY(m.removeColumn("x"));
        QCOMPARE(m.value("c", "z"), 99.0);
        QCOMPARE(m.columnCount(), 1);
    }

    [[= rqt::slot]] void capability_removeColumnsRunsGoFromTheLastToTheFirst()
    {
        Mat m(kNaN);
        m.addRow("r");
        m.addColumns(keys("c", 8));
        Recorder rec(m);
        QCOMPARE(m.removeColumns({"c1", "c2", "c5", "c7"}), 4);
        QCOMPARE(rec.log, (QStringList{"columnsAboutToBeRemoved(7,7,[c7])", "columnsRemoved(7,7,[c7])", "columnsAboutToBeRemoved(5,5,[c5])", "columnsRemoved(5,5,[c5])",
                                       "columnsAboutToBeRemoved(1,2,[c1,c2])", "columnsRemoved(1,2,[c1,c2])"}));
    }

    [[= rqt::slot]] void capability_aNonNaNSentinelUsesEquality()
    {
        obs::Matrix<int, int, int> m(-1);
        m.addRow(1);
        m.addColumn(2);
        QVERIFY(m.isUninitialized(-1));
        QVERIFY(!m.isUninitialized(0));
        QSignalSpy spy(&m, &obs::Matrix<int, int, int>::cellChanged);
        QVERIFY(m.setValue(1, 2, -1));
        QCOMPARE(spy.count(), 0);
        QVERIFY(m.setValue(1, 2, 5));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 2);
    }

    [[= rqt::slot]] void at_6_1_everyStructuralStepKeepsTheModelInStep()
    {
        Mat m(kNaN);
        obs::MatrixTableModel<Mat> model(m);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QVERIFY(mirrors(model, m));

        m.addRow("r0");
        QVERIFY(mirrors(model, m));
        m.addColumn("c0");
        QVERIFY(mirrors(model, m));
        m.addRows({"r1", "r2", "r3"});
        QVERIFY(mirrors(model, m));
        m.addColumns({"c1", "c2"});
        QVERIFY(mirrors(model, m));
        m.setValue("r1", "c1", 4.0);
        QVERIFY(mirrors(model, m));
        m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r3", "c2", 3.0}});
        QVERIFY(mirrors(model, m));
        m.removeRow("r0");
        QVERIFY(mirrors(model, m));
        m.removeColumn("c1");
        QVERIFY(mirrors(model, m));
        m.addRows(keys("n", 6));
        m.removeRows({"n0", "n1", "n3", "n5", "r2"});
        QVERIFY(mirrors(model, m));
        m.addColumns(keys("m", 5));
        m.removeColumns({"m0", "m1", "m4", "c0"});
        QVERIFY(mirrors(model, m));
        m.renameRow("n2", "renamed");
        m.renameColumn("m2", "renamedColumn");
        QVERIFY(mirrors(model, m));
        QCOMPARE(model.headerData(m.rowIndexOf("renamed"), Qt::Vertical).toString(), QString("renamed"));
        m.clear();
        QVERIFY(mirrors(model, m));
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.columnCount(), 0);
    }

    [[= rqt::slot]] void at_6_2_sequenceT7ARenameIsOneHeaderChange()
    {
        Mat m(kNaN);
        m.addRows({"s1", "s2", "s3"});
        m.addColumn("c");
        obs::MatrixTableModel<Mat> model(m);
        QAbstractItemModelTester tester(&model, kTesterMode);
        Recorder source(m);
        QSignalSpy header(&model, &QAbstractItemModel::headerDataChanged);
        QSignalSpy structure1(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy structure2(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy structure3(&model, &QAbstractItemModel::rowsMoved);
        QSignalSpy structure4(&model, &QAbstractItemModel::modelReset);

        m.renameRow("s2", "s2b");

        QCOMPARE(source.log, (QStringList{"rowAboutToBeRenamed(s2,s2b,1)", "rowRenamed(s2,s2b,1)"}));
        QCOMPARE(header.count(), 1);
        QCOMPARE(header.at(0).at(0).value<Qt::Orientation>(), Qt::Vertical);
        QCOMPARE(header.at(0).at(1).toInt(), 1);
        QCOMPARE(header.at(0).at(2).toInt(), 1);
        QCOMPARE(structure1.count() + structure2.count() + structure3.count() + structure4.count(), 0);
        QCOMPARE(model.headerData(1, Qt::Vertical).toString(), QString("s2b"));
    }

    [[= rqt::slot]] void at_6_3_dataChangedCoversExactlyTheChangedCells()
    {
        Mat m(kNaN);
        m.addRows({"r0", "r1"});
        m.addColumns({"c0", "c1"});
        obs::MatrixTableModel<Mat> model(m);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

        m.setValue("r1", "c0", 1.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toModelIndex(), model.index(1, 0));
        QCOMPARE(spy.at(0).at(1).toModelIndex(), model.index(1, 0));

        spy.clear();
        m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r1", "c1", 9.0}, {"r1", "c0", 5.0}});
        // r0: c0..c1 and r1: c0..c1 are the same extent, so the block is ONE rectangle
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toModelIndex(), model.index(0, 0));
        QCOMPARE(spy.at(0).at(1).toModelIndex(), model.index(1, 1));

        spy.clear();
        Mat l(kNaN);
        l.addRows({"r0", "r1"});
        l.addColumns({"c0", "c1"});
        obs::MatrixTableModel<Mat> lModel(l);
        QAbstractItemModelTester lTester(&lModel, kTesterMode);
        QSignalSpy lSpy(&lModel, &QAbstractItemModel::dataChanged);
        l.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r1", "c0", 3.0}});
        QCOMPARE(lSpy.count(), 2);
        QCOMPARE(lSpy.at(0).at(0).toModelIndex(), lModel.index(0, 0));
        QCOMPARE(lSpy.at(0).at(1).toModelIndex(), lModel.index(0, 1));
        QCOMPARE(lSpy.at(1).at(0).toModelIndex(), lModel.index(1, 0));
        QCOMPARE(lSpy.at(1).at(1).toModelIndex(), lModel.index(1, 0));
    }

    [[= rqt::slot]] void at_6_4_anUninitialisedCellIsAnInvalidVariant()
    {
        Mat m(kNaN);
        m.addRow("r");
        m.addColumn("c");
        obs::MatrixTableModel<Mat> model(m);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QVERIFY(!model.data(model.index(0, 0), Qt::DisplayRole).isValid());
        m.setValue("r", "c", 2.0);
        QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toDouble(), 2.0);
        QVERIFY(!model.data(model.index(0, 0), Qt::EditRole).isValid());
    }

    [[= rqt::slot]] void capability_aBatchRowRemovalReachesTheModelAsOneCyclePerRun()
    {
        Mat m(kNaN);
        m.addRows(keys("k", 8));
        m.addColumn("c");
        obs::MatrixTableModel<Mat> model(m);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QStringList log;
        QObject ctx;
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &ctx, [&](const QModelIndex &, int f, int l) { log << QStringLiteral("%1-%2").arg(f).arg(l); });
        m.removeRows({"k1", "k2", "k5", "k7"});
        QCOMPARE(log, (QStringList{"7-7", "5-5", "1-2"}));
        QVERIFY(mirrors(model, m));
    }

    // Added after mutation testing (a mutant whose clear left the key map as it was survived: the map had never been built, so it
    // was stale anyway): the map is built first, then cleared.
    [[= rqt::slot]] void at_5_7_clearForgetsKeysThatWereLookedUp()
    {
        Mat m(kNaN);
        m.addRow("r");
        m.addColumn("c");
        QCOMPARE(m.rowIndexOf("r"), 0);  // builds the lookup map
        QCOMPARE(m.columnIndexOf("c"), 0);
        m.clear();
        QCOMPARE(m.rowIndexOf("r"), -1);
        QCOMPARE(m.columnIndexOf("c"), -1);
        QVERIFY(m.addRow("r"));
        QCOMPARE(m.rowIndexOf("r"), 0);
    }

    // The row and column indexes are kept in step with the key lists on every change, so a lookup never rebuilds them.
    [[= rqt::slot]] void capability_theKeyIndexesAgreeWithTheKeyListsAfterEveryStep()
    {
        for (unsigned seed = 1; seed <= 8; ++seed)
            QVERIFY(matrixIndexesAgree(seed));
    }
};

QTEST_GUILESS_MAIN(TstMatrix)
