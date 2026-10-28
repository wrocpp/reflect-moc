// SPEC 12.4: the wired list model, binding B. Every test attaches QAbstractItemModelTester before the first mutation.
#include <obs/container_list_model.h>

#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <reflect_moc/qt/qt.hpp>

#include <algorithm>
#include <functional>
#include <random>

namespace {

constexpr auto kTesterMode = QAbstractItemModelTester::FailureReportingMode::QtTest;

template <class K>
QString keyText(const K & key)
{
    return QVariant::fromValue(key).toString();
}

template <class K>
QString keysText(const QList<K> & keys)
{
    QStringList parts;
    for (const K & k : keys)
        parts << keyText(k);
    return QLatin1Char('[') + parts.join(QLatin1Char(',')) + QLatin1Char(']');
}

// The test-local model: DisplayRole is the value
template <class C, class Less = std::less<obs::KeyOf<C>>>
class TestModel : public obs::ContainerListModel<C, Less> {
public:
    using Base = obs::ContainerListModel<C, Less>;
    using Base::Base;

    QHash<int, QByteArray> roleNames() const override { return {{Qt::DisplayRole, "display"}}; }

protected:
    QVariant valueData(const typename Base::Key &, const typename Base::Value & value, int role) const override
    {
        return role == Qt::DisplayRole ? QVariant::fromValue(value) : QVariant{};
    }
};

// The source's structural notifications, in order
template <class C>
class SourceLog {
public:
    using Source = obs::Container<C>;
    using Key = typename Source::Key;

    explicit SourceLog(Source & source)
    {
        const auto single = [&](const char * name, auto signal) {
            QObject::connect(&source, signal, &context, [this, name](const Key & k) { log << QStringLiteral("%1(%2)").arg(QLatin1String(name), keyText(k)); });
        };
        const auto many = [&](const char * name, auto signal) {
            QObject::connect(&source, signal, &context, [this, name](const QList<Key> & ks) { log << QStringLiteral("%1(%2)").arg(QLatin1String(name), keysText(ks)); });
        };
        single("entryAboutToBeAdded", &Source::entryAboutToBeAdded);
        single("entryAdded", &Source::entryAdded);
        many("entriesAboutToBeAdded", &Source::entriesAboutToBeAdded);
        many("entriesAdded", &Source::entriesAdded);
    }

    QObject context;
    QStringList log;
};

// The model's structure notifications, in order
class ModelLog {
public:
    explicit ModelLog(QAbstractItemModel & m)
    {
        QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeInserted, &context, [this](const QModelIndex &, int f, int l) { add("rowsAboutToBeInserted", f, l); });
        QObject::connect(&m, &QAbstractItemModel::rowsInserted, &context, [this](const QModelIndex &, int f, int l) { add("rowsInserted", f, l); });
        QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeRemoved, &context, [this](const QModelIndex &, int f, int l) { add("rowsAboutToBeRemoved", f, l); });
        QObject::connect(&m, &QAbstractItemModel::rowsRemoved, &context, [this](const QModelIndex &, int f, int l) { add("rowsRemoved", f, l); });
        QObject::connect(&m, &QAbstractItemModel::modelAboutToBeReset, &context, [this] { log << QStringLiteral("modelAboutToBeReset"); });
        QObject::connect(&m, &QAbstractItemModel::modelReset, &context, [this] { log << QStringLiteral("modelReset"); });
        QObject::connect(&m, &QAbstractItemModel::dataChanged, &context, [this](const QModelIndex & a, const QModelIndex & b, const QList<int> & roles) {
            QStringList r;
            for (int role : roles)
                r << QString::number(role);
            log << QStringLiteral("dataChanged(%1,%2,[%3])").arg(a.row()).arg(b.row()).arg(r.join(QLatin1Char(',')));
        });
    }

    QObject context;
    QStringList log;

private:
    void add(const char * name, int first, int last) { log << QStringLiteral("%1(%2,%3)").arg(QLatin1String(name)).arg(first).arg(last); }
};

template <class Model>
QStringList displayedRows(const Model & model)
{
    QStringList rows;
    for (int r = 0; r < model.rowCount(); ++r)
        rows << model.data(model.index(r, 0), Qt::DisplayRole).toString();
    return rows;
}

// SPEC addendum A.6: QHash<int, QString> unless a test says otherwise
using Names = QHash<int, QString>;
using NamesSource = obs::Container<Names>;

QList<int> sortedKeys(const NamesSource & c)
{
    QList<int> keys = c.data().keys();
    std::sort(keys.begin(), keys.end());
    return keys;
}

template <class Model>
QList<int> rowKeys(const Model & model)
{
    QList<int> keys;
    for (int r = 0; r < model.rowCount(); ++r)
        keys << model.keyAt(r);
    return keys;
}

Names namesFromOneTo(int last)
{
    Names names;
    for (int key = 1; key <= last; ++key)
        names.insert(key, QString::number(key));
    return names;
}

using Letters = QMap<QString, QString>;

Letters lettersOf(const QStringList & keys)
{
    Letters m;
    for (const QString & k : keys)
        m.insert(k, k.toUpper());
    return m;
}


// Removes "b", "c" and "e" from "abcdef" (value: the upper-case letter) in one batch and reads every row a receiver of
// rowsAboutToBeRemoved is told about.
template <class C>
bool readsEveryDoomedRow(bool take)
{
    C content;
    for (const QString & k : QStringList{"a", "b", "c", "d", "e", "f"})
        content.insert(k, k.toUpper());
    obs::Container<C> c(content);
    TestModel<C> model(c);
    QAbstractItemModelTester tester(&model, kTesterMode);
    QStringList read;
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &model, [&](const QModelIndex &, int first, int last) {
        for (int row = first; row <= last; ++row)
            read.append(model.data(model.index(row, 0), Qt::DisplayRole).toString());
    });
    const QList<QString> doomed{"b", "c", "e"};
    const qsizetype removed = take ? c.takeMany(doomed).size() : c.removeMany(doomed);
    read.sort();
    return removed == 3 && read == QStringList{"B", "C", "E"} && model.rowCount() == 3;
}

// After every step of a random mix of inserts, removals, batches, replaces and resets, rowOf agrees with the sorted keys of
// the container for every key of a small universe, present or not.
template <class C>
bool rowOfAgreesWithTheRows(unsigned seed)
{
    obs::Container<C> c{C{}};
    TestModel<C> model(c);
    QAbstractItemModelTester tester(&model, kTesterMode);
    constexpr int kUniverse = 24;
    constexpr int kSteps = 400;
    std::mt19937 rng(seed);
    const auto key = [&] { return QString::number(static_cast<int>(rng() % kUniverse) + 100); };
    for (int step = 0; step < kSteps; ++step) {
        switch (rng() % 6) {
        case 0: c.insert(key(), QString::number(step)); break;
        case 1: c.remove(key()); break;
        case 2: c.replace(key(), QString::number(step)); break;
        case 3: {
            C extra;
            for (int i = 0; i < 3; ++i)
                extra.insert(key(), QString::number(step));
            c.insertMany(extra);
            break;
        }
        case 4: c.removeMany(QList<QString>{key(), key(), key()}); break;
        default:
            if (rng() % 8 == 0)
                c.clear();
            break;
        }
        QList<QString> sorted;
        for (int k = 0; k < kUniverse; ++k)
            if (c.contains(QString::number(k + 100)))
                sorted.append(QString::number(k + 100));
        std::sort(sorted.begin(), sorted.end());
        for (int k = 0; k < kUniverse; ++k) {
            const QString name = QString::number(k + 100);
            const int expected = sorted.indexOf(name);
            if (model.rowOf(name) != expected)
                return false;
        }
        if (model.rowCount() != sorted.size())
            return false;
    }
    return true;
}

}  // namespace

class TstListModel : public QObject {
    RQT_OBJECT

private:
    // after every step: the model mirrors the container
    static bool mirrors(const TestModel<QList<int>> & model, const obs::Container<QList<int>> & c)
    {
        if (model.rowCount() != c.size())
            return false;
        for (int r = 0; r < model.rowCount(); ++r)
            if (model.data(model.index(r, 0), Qt::DisplayRole).toInt() != c.data()[r])
                return false;
        return true;
    }

private slots:
    [[= rqt::slot]] void at_4_1_sequentialSourceEveryStepMirrors()
    {
        obs::Container<QList<int>> c(QList<int>{10, 20, 30});
        TestModel<QList<int>> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QVERIFY(mirrors(model, c));

        c.append(40);
        QVERIFY(mirrors(model, c));
        c.insert(0, 5);
        QVERIFY(mirrors(model, c));
        c.remove(1);
        QVERIFY(mirrors(model, c));
        c.replace(1, 99);
        QVERIFY(mirrors(model, c));
        c.clear();
        QVERIFY(mirrors(model, c));
        QCOMPARE(model.rowCount(), 0);
        c.resetWith({1, 2, 3});
        QVERIFY(mirrors(model, c));
        QCOMPARE(model.rowCount(), 3);
    }

    [[= rqt::slot]] void at_4_2_associativeRowsAreSortedByKey()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c(Map{{"c", 3}, {"a", 1}});
        TestModel<Map> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QCOMPARE(displayedRows(model), (QStringList{"1", "3"}));
        QCOMPARE(model.keyAt(0), QString("a"));
        QCOMPARE(model.keyAt(1), QString("c"));

        ModelLog modelLog(model);
        c.insert("b", 2);
        QCOMPARE(modelLog.log, (QStringList{"rowsAboutToBeInserted(1,1)", "rowsInserted(1,1)"}));
        QCOMPARE(displayedRows(model), (QStringList{"1", "2", "3"}));
        QCOMPARE(model.rowOf("b"), 1);
        QCOMPARE(model.rowOf("zz"), -1);
    }

    [[= rqt::slot]] void at_4_2_aHashSourceIsSortedByKeyToo()
    {
        obs::Container<QHash<int, int>> c(QHash<int, int>{{30, 3}, {10, 1}, {20, 2}});
        TestModel<QHash<int, int>> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QCOMPARE(displayedRows(model), (QStringList{"1", "2", "3"}));
        c.insert(15, 15);
        c.remove(10);
        QCOMPARE(displayedRows(model), (QStringList{"15", "2", "3"}));
    }

    [[= rqt::slot]] void at_4_3_editValueAndReplaceRolesPassThrough()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c(Map{{"a", 1}, {"b", 2}});
        TestModel<Map> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        ModelLog modelLog(model);

        const int role = Qt::UserRole + 7;
        c.editValue("b", [](int & v) { v = 20; }, {role});
        QCOMPARE(modelLog.log, (QStringList{"dataChanged(1,1,[263])"}));

        modelLog.log.clear();
        c.replace("a", 10);
        QCOMPARE(modelLog.log, (QStringList{"dataChanged(0,0,[])"}));
        QCOMPARE(displayedRows(model), (QStringList{"10", "20"}));
    }

    [[= rqt::slot]] void at_4_4_sequenceT2InsertManyIsOneCyclePerRun()
    {
        obs::Container<Letters> c(lettersOf({"a", "c", "e", "g"}));
        TestModel<Letters> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        SourceLog<Letters> source(c);
        ModelLog modelLog(model);

        c.insertMany(lettersOf({"b", "d", "h"}));

        QCOMPARE(source.log, (QStringList{"entriesAboutToBeAdded([b,d,h])", "entriesAdded([b,d,h])"}));
        QCOMPARE(modelLog.log, (QStringList{"rowsAboutToBeInserted(1,1)", "rowsInserted(1,1)", "rowsAboutToBeInserted(3,3)", "rowsInserted(3,3)",
                                            "rowsAboutToBeInserted(6,6)", "rowsInserted(6,6)"}));
        QCOMPARE(displayedRows(model), (QStringList{"A", "B", "C", "D", "E", "G", "H"}));
    }

    [[= rqt::slot]] void at_4_5_aContiguousBatchIsOneCycle()
    {
        obs::Container<Letters> c(lettersOf({"a", "e"}));
        TestModel<Letters> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        ModelLog modelLog(model);
        c.insertMany(lettersOf({"b", "c", "d"}));
        QCOMPARE(modelLog.log, (QStringList{"rowsAboutToBeInserted(1,3)", "rowsInserted(1,3)"}));
        QCOMPARE(displayedRows(model), (QStringList{"A", "B", "C", "D", "E"}));
    }

    [[= rqt::slot]] void at_4_6_sequenceT3RemoveManyIsOneCyclePerRunFromTheBottom()
    {
        obs::Container<Letters> c(lettersOf({"a", "b", "c", "d", "e", "f", "g", "h"}));
        TestModel<Letters> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        ModelLog modelLog(model);

        c.removeMany({"b", "c", "f", "h"});

        QCOMPARE(modelLog.log, (QStringList{"rowsAboutToBeRemoved(7,7)", "rowsRemoved(7,7)", "rowsAboutToBeRemoved(5,5)", "rowsRemoved(5,5)",
                                            "rowsAboutToBeRemoved(1,2)", "rowsRemoved(1,2)"}));
        QCOMPARE(displayedRows(model), (QStringList{"A", "D", "E", "G"}));
    }

    [[= rqt::slot]] void at_4_7_clearIsOneResetAndResetWithIsSequenceT4()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c(Map{{"a", 1}, {"b", 2}});
        TestModel<Map> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        ModelLog modelLog(model);

        c.clear();
        QCOMPARE(modelLog.log, (QStringList{"modelAboutToBeReset", "modelReset"}));
        QCOMPARE(model.rowCount(), 0);

        c.resetWith(Map{{"x", 1}, {"y", 2}});
        modelLog.log.clear();
        int rowsDuringAbout = -1, rowsAfterReset = -1;
        QObject::connect(&model, &QAbstractItemModel::modelAboutToBeReset, &modelLog.context, [&] { rowsDuringAbout = model.rowCount(); });
        QObject::connect(&model, &QAbstractItemModel::modelReset, &modelLog.context, [&] { rowsAfterReset = model.rowCount(); });
        c.resetWith(Map{{"p", 1}, {"q", 2}, {"r", 3}});
        QCOMPARE(modelLog.log, (QStringList{"modelAboutToBeReset", "modelReset"}));
        QCOMPARE(rowsDuringAbout, 2);
        QCOMPARE(rowsAfterReset, 3);
    }

    // Beyond SPEC 12.4
    [[= rqt::slot]] void capability_aCustomLessOrdersTheRows()
    {
        using Map = QMap<int, int>;
        obs::Container<Map> c(Map{{1, 10}, {3, 30}});
        TestModel<Map, std::greater<int>> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QCOMPARE(displayedRows(model), (QStringList{"30", "10"}));
        c.insert(2, 20);
        QCOMPARE(displayedRows(model), (QStringList{"30", "20", "10"}));
        c.insertMany(Map{{0, 0}, {5, 50}});
        QCOMPARE(displayedRows(model), (QStringList{"50", "30", "20", "10", "0"}));
    }

    [[= rqt::slot]] void capability_aRowInsideABatchRemovalCycleReadsItsValue()
    {
        obs::Container<Letters> c(lettersOf({"a", "b", "c"}));
        TestModel<Letters> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QString read;
        QObject ctx;
        QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &ctx, [&](const QModelIndex &, int f, int) {
            read = model.data(model.index(f, 0), Qt::DisplayRole).toString();
        });
        c.removeMany({"b"});
        QCOMPARE(read, QString("B"));
        QCOMPARE(displayedRows(model), (QStringList{"A", "C"}));
    }

    [[= rqt::slot]] void capability_aRowCountOutsideACycleNeverReadsTheContainersSize()
    {
        obs::Container<QList<int>> c(QList<int>{1, 2});
        TestModel<QList<int>> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        int sizeAbout = -1, sizeTwin = -1;
        QObject ctx;
        // the model connected first: its count moves between the "about" and the twin, the container's size leads it
        QObject::connect(&c, &obs::Container<QList<int>>::entryAboutToBeAdded, &ctx, [&] { sizeAbout = model.rowCount(); });
        QObject::connect(&c, &obs::Container<QList<int>>::entryAdded, &ctx, [&] { sizeTwin = model.rowCount(); });
        c.append(3);
        QCOMPARE(sizeAbout, 2);
        QCOMPARE(sizeTwin, 3);
    }

    // 8.3 as the spec's author meant it: a batch removal is announced while the container still holds the entries, so a
    // view reads every row it is losing, in a map and in a hash, through removeMany and through takeMany.
    [[= rqt::slot]] void at_4_10_aBatchRemovalLetsAViewReadTheRowsItLoses()
    {
        QVERIFY(readsEveryDoomedRow<Letters>(false));
        QVERIFY(readsEveryDoomedRow<Letters>(true));
        QVERIFY((readsEveryDoomedRow<QHash<QString, QString>>(false)));
        QVERIFY((readsEveryDoomedRow<QHash<QString, QString>>(true)));
    }

    // The container has erased a removed entry when it announces entryRemoved, and the model drops the row in its own receiver of
    // that signal. A receiver connected before the model runs in between and finds the row still shown: it reads nothing, not a
    // dangling value.
    [[= rqt::slot]] void at_4_12_aReceiverAheadOfTheModelReadsNothingForTheRemovedEntry()
    {
        obs::Container<Letters> c(lettersOf({"a", "b", "c"}));
        QVariant read = QStringLiteral("untouched");
        QAbstractItemModel * watched = nullptr;
        QObject ctx;
        QObject::connect(&c, &obs::Container<Letters>::entryRemoved, &ctx, [&](const QString &) {
            read = watched->data(watched->index(1, 0), Qt::DisplayRole);
        });
        TestModel<Letters> model(c);
        watched = &model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        QVERIFY(c.remove("b"));
        QVERIFY(!read.isValid());
        QCOMPARE(model.rowCount(), 2);
    }

    // The row lookup is a binary search over the sorted row keys: it agrees with the container after every step of a random
    // mix, in a map and in a hash (whose rows are sorted by the same Less).
    [[= rqt::slot]] void at_4_11_theRowLookupAgreesWithTheRowsAfterEveryStep()
    {
        for (unsigned seed = 1; seed <= 5; ++seed) {
            QVERIFY(rowOfAgreesWithTheRows<Letters>(seed));
            QVERIFY((rowOfAgreesWithTheRows<QHash<QString, QString>>(seed)));
        }
    }

    // Added after mutation testing (a mutant that left the key-to-row map fresh after an insert survived): a lookup after inserts
    // before a key finds the key's new row.
    [[= rqt::slot]] void at_4_8_theRowMapFollowsInsertions()
    {
        obs::Container<Letters> c(lettersOf({"a", "c", "e"}));
        TestModel<Letters> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);
        QVERIFY(c.replace("e", "1"));  // builds the row map: "e" is row 2
        QCOMPARE(spy.takeLast().at(0).toModelIndex().row(), 2);
        QVERIFY(c.insert("b", "x"));
        QVERIFY(c.insert("d", "y"));
        QVERIFY(c.replace("e", "2"));  // "e" is row 4 now
        QCOMPARE(spy.takeLast().at(0).toModelIndex().row(), 4);
        QCOMPARE(model.rowOf("e"), 4);
    }

    // SPEC addendum A.6 (section 12.10): the model's cycles run inside the container's notification scope, so a change a view
    // makes while the model is announcing a batch is queued and arrives after the last cycle. A view that would provoke
    // itself again acts the first time only.

    [[= rqt::slot]] void at_10_10_aViewReactingToABatchRemovalIsAppliedAfterTheLastCycle()
    {
        NamesSource c(namesFromOneTo(10));
        TestModel<Names> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QObject ctx;
        bool first = true;
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &ctx, [&] {
            if (first) {
                first = false;
                c.remove(3);
            }
        });

        QCOMPARE(c.removeMany({2, 5, 8}), 3);

        QCOMPARE(sortedKeys(c), (QList<int>{1, 4, 6, 7, 9, 10}));
        QCOMPARE(rowKeys(model), sortedKeys(c));
        QCOMPARE(displayedRows(model), (QStringList{"1", "4", "6", "7", "9", "10"}));
    }

    [[= rqt::slot]] void at_10_11_aViewReactingToABatchInsertionIsAppliedAfterTheLastCycle()
    {
        NamesSource c(Names{{10, "10"}, {20, "20"}, {30, "30"}});
        TestModel<Names> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QObject ctx;
        bool first = true;
        QObject::connect(&model, &QAbstractItemModel::rowsInserted, &ctx, [&] {
            if (first) {
                first = false;
                c.insert(17, "17");
            }
        });

        QCOMPARE(c.insertMany(Names{{5, "5"}, {15, "15"}, {25, "25"}}), 3);

        QCOMPARE(rowKeys(model), (QList<int>{5, 10, 15, 17, 20, 25, 30}));
        QCOMPARE(displayedRows(model), (QStringList{"5", "10", "15", "17", "20", "25", "30"}));
    }

    // A.5: every cycle of the model, for a single change and for both batches, runs inside the container's notification scope
    [[= rqt::slot]] void capability_everyCycleOfTheModelRunsInsideTheContainersScope()
    {
        NamesSource c(namesFromOneTo(6));
        TestModel<Names> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        QObject ctx;
        int cycleSignals = 0, outsideTheScope = 0;
        const auto probe = [&] {
            ++cycleSignals;
            outsideTheScope += c.isNotifying() ? 0 : 1;
        };
        QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &ctx, probe);
        QObject::connect(&model, &QAbstractItemModel::rowsInserted, &ctx, probe);
        QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &ctx, probe);
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &ctx, probe);

        QVERIFY(c.insert(7, "7"));                                // one cycle
        QVERIFY(c.remove(1));                                     // one cycle
        QCOMPARE(c.insertMany(Names{{10, "10"}, {12, "12"}}), 2);  // rows 6 and 7 of the final list: one run
        QCOMPARE(c.removeMany({2, 3, 10}), 3);                    // rows 0 and 1, and row 6: two runs

        QCOMPARE(cycleSignals, 10);
        QCOMPARE(outsideTheScope, 0);
        QCOMPARE(rowKeys(model), (QList<int>{4, 5, 6, 7, 12}));
        QVERIFY(!c.isNotifying());
    }

    [[= rqt::slot]] void at_10_12_aReceiverAheadOfTheModelRemovingTheNewKeyLeavesNoGhostRow()
    {
        NamesSource c;
        QObject ctx;
        const int added = 4;
        // connected before the model, so it runs before the model has seen entryAdded
        QObject::connect(&c, &NamesSource::entryAdded, &ctx, [&](const int & key) { c.remove(key); });
        TestModel<Names> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);

        QVERIFY(c.insert(added, "four"));

        QVERIFY(!c.contains(added));
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.rowOf(added), -1);
    }
};

QTEST_GUILESS_MAIN(TstListModel)
