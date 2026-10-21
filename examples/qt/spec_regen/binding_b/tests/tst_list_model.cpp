// SPEC 12.4: the wired list model, binding B. Every test attaches QAbstractItemModelTester before the first mutation.
#include <obs/container_list_model.h>

#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <reflect_moc/qt/qt.hpp>

#include <functional>

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

using Letters = QMap<QString, QString>;

Letters lettersOf(const QStringList & keys)
{
    Letters m;
    for (const QString & k : keys)
        m.insert(k, k.toUpper());
    return m;
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

    [[= rqt::slot]] void capability_aRowInsideABatchRemovalCycleReadsAsInvalid()
    {
        obs::Container<Letters> c(lettersOf({"a", "b", "c"}));
        TestModel<Letters> model(c);
        QAbstractItemModelTester tester(&model, kTesterMode);
        bool valid = true;
        QObject ctx;
        QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &ctx, [&](const QModelIndex &, int f, int) {
            valid = model.data(model.index(f, 0), Qt::DisplayRole).isValid();
        });
        c.removeMany({"b"});
        QVERIFY(!valid);
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
};

QTEST_GUILESS_MAIN(TstListModel)
