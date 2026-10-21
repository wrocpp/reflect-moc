// SPEC 12.3: the one-dimensional container, binding B (typed signals declared on the template).
#include <obs/container.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <reflect_moc/qt/qt.hpp>

#include <map>
#include <vector>

namespace {

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

QString rolesText(const QList<int> & roles)
{
    QStringList parts;
    for (int r : roles)
        parts << QString::number(r);
    return QLatin1Char('[') + parts.join(QLatin1Char(',')) + QLatin1Char(']');
}

// One object connected to EVERY signal of the container, appending the name and the payload.
template <class C>
class Recorder {
public:
    using Source = obs::Container<C>;
    using Key = typename Source::Key;

    explicit Recorder(Source & source)
    {
        const auto single = [&](const char * name, auto signal) {
            QObject::connect(&source, signal, &context, [this, name](const Key & k) { log << QStringLiteral("%1(%2)").arg(QLatin1String(name), keyText(k)); });
        };
        const auto withRoles = [&](const char * name, auto signal) {
            QObject::connect(&source, signal, &context, [this, name](const Key & k, const QList<int> & roles) {
                log << QStringLiteral("%1(%2,%3)").arg(QLatin1String(name), keyText(k), rolesText(roles));
            });
        };
        const auto many = [&](const char * name, auto signal) {
            QObject::connect(&source, signal, &context, [this, name](const QList<Key> & ks) { log << QStringLiteral("%1(%2)").arg(QLatin1String(name), keysText(ks)); });
        };
        const auto none = [&](const char * name, auto signal) {
            QObject::connect(&source, signal, &context, [this, name] { log << QLatin1String(name); });
        };
        single("entryAboutToBeAdded", &Source::entryAboutToBeAdded);
        single("entryAdded", &Source::entryAdded);
        single("entryAboutToBeRemoved", &Source::entryAboutToBeRemoved);
        single("entryRemoved", &Source::entryRemoved);
        withRoles("entryDataAboutToChange", &Source::entryDataAboutToChange);
        withRoles("entryDataChanged", &Source::entryDataChanged);
        many("entriesAboutToBeAdded", &Source::entriesAboutToBeAdded);
        many("entriesAdded", &Source::entriesAdded);
        many("entriesAboutToBeRemoved", &Source::entriesAboutToBeRemoved);
        many("entriesRemoved", &Source::entriesRemoved);
        none("aboutToBeCleared", &Source::aboutToBeCleared);
        none("cleared", &Source::cleared);
        none("aboutToBeReset", &Source::aboutToBeReset);
        none("resetCompleted", &Source::resetCompleted);
    }

    QObject context;
    QStringList log;
};

// SPEC 4.1 last bullet, kept under binding B: a class derived from a specialisation adds signals of its own
class Store : public obs::Container<QHash<quint32, int>> {
    RQT_OBJECT

public:
    explicit Store(QObject * parent = nullptr) : obs::Container<QHash<quint32, int>>({}, parent) {}

    void renameUnit(int unit)
    {
        unit_ = unit;
        unitChanged(unit);
    }

signals:
    rqt::signal<void(int)> unitChanged;

private:
    int unit_ = 0;
};

}  // namespace

class TstContainer : public QObject {
    RQT_OBJECT

private slots:
    [[= rqt::slot]] void at_3_1_sequenceT1AndTheStateInsideEachReceiver()
    {
        obs::Container<QList<int>> c(QList<int>{10, 20, 30});
        Recorder<QList<int>> rec(c);
        qsizetype sizeAbout = -1, sizeTwin = -1;
        int secondAbout = 0, secondTwin = 0;
        QObject ctx;
        QObject::connect(&c, &obs::Container<QList<int>>::entryAboutToBeAdded, &ctx, [&] {
            sizeAbout = c.size();
            secondAbout = c.data()[1];
        });
        QObject::connect(&c, &obs::Container<QList<int>>::entryAdded, &ctx, [&] {
            sizeTwin = c.size();
            secondTwin = c.data()[1];
        });

        QVERIFY(c.insert(1, 15));

        QCOMPARE(rec.log, (QStringList{"entryAboutToBeAdded(1)", "entryAdded(1)"}));
        QCOMPARE(sizeAbout, 3);
        QCOMPARE(secondAbout, 20);
        QCOMPARE(sizeTwin, 4);
        QCOMPARE(secondTwin, 15);
    }

    [[= rqt::slot]] void at_3_1_appendIsInsertAtTheEnd()
    {
        obs::Container<QList<int>> c(QList<int>{1});
        Recorder<QList<int>> rec(c);
        QVERIFY(c.append(2));
        QCOMPARE(rec.log, (QStringList{"entryAboutToBeAdded(1)", "entryAdded(1)"}));
        QCOMPARE(c.data(), (QList<int>{1, 2}));
    }

    [[= rqt::slot]] void at_3_2_associativeStateInsideEachReceiver()
    {
        obs::Container<QHash<QString, int>> c;
        QObject ctx;
        using Source = obs::Container<QHash<QString, int>>;
        bool inAbout = true, inTwin = false;
        QObject::connect(&c, &Source::entryAboutToBeAdded, &ctx, [&] { inAbout = c.contains("x"); });
        QObject::connect(&c, &Source::entryAdded, &ctx, [&] { inTwin = c.contains("x"); });
        QVERIFY(c.insert("x", 1));
        QVERIFY(!inAbout);
        QVERIFY(inTwin);

        bool removeAbout = false, removeTwin = true;
        QObject::connect(&c, &Source::entryAboutToBeRemoved, &ctx, [&] { removeAbout = c.contains("x"); });
        QObject::connect(&c, &Source::entryRemoved, &ctx, [&] { removeTwin = c.contains("x"); });
        QVERIFY(c.remove("x"));
        QVERIFY(removeAbout);
        QVERIFY(!removeTwin);
    }

    [[= rqt::slot]] void at_3_3_rejectedCallsEmitNothing()
    {
        obs::Container<QHash<QString, int>> hash(QHash<QString, int>{{"a", 1}});
        Recorder<QHash<QString, int>> hashRec(hash);
        QVERIFY(!hash.insert("a", 2));
        QVERIFY(!hash.remove("zz"));
        QVERIFY(!hash.replace("zz", 1));
        QVERIFY(!hash.editValue("zz", [](int & v) { v = 9; }));
        QVERIFY(!hash.take("zz").has_value());
        QCOMPARE(hash.data().value("a"), 1);
        QVERIFY(hashRec.log.isEmpty());

        obs::Container<QList<int>> list(QList<int>{1, 2, 3});
        Recorder<QList<int>> listRec(list);
        QVERIFY(!list.insert(5, 0));
        QVERIFY(!list.insert(-1, 0));
        QVERIFY(!list.remove(-1));
        QVERIFY(!list.remove(3));
        QVERIFY(!list.take(7).has_value());
        QVERIFY(!list.replace(3, 0));
        QVERIFY(listRec.log.isEmpty());
        QCOMPARE(list.data(), (QList<int>{1, 2, 3}));
    }

    [[= rqt::slot]] void at_3_4_clear()
    {
        obs::Container<QList<int>> c;
        Recorder<QList<int>> rec(c);
        c.clear();
        QVERIFY(rec.log.isEmpty());

        c.resetWith({1, 2});
        rec.log.clear();
        qsizetype sizeAbout = -1;
        QObject ctx;
        QObject::connect(&c, &obs::Container<QList<int>>::aboutToBeCleared, &ctx, [&] { sizeAbout = c.size(); });
        c.clear();
        QCOMPARE(rec.log, (QStringList{"aboutToBeCleared", "cleared"}));
        QCOMPARE(sizeAbout, 2);
        QCOMPARE(c.size(), 0);
    }

    [[= rqt::slot]] void at_3_5_resetWithIsAtomic()
    {
        obs::Container<QList<int>> c(QList<int>{1, 2});
        Recorder<QList<int>> rec(c);
        QList<int> inAbout, inTwin;
        QObject ctx;
        QObject::connect(&c, &obs::Container<QList<int>>::aboutToBeReset, &ctx, [&] { inAbout = c.data(); });
        QObject::connect(&c, &obs::Container<QList<int>>::resetCompleted, &ctx, [&] { inTwin = c.data(); });
        c.resetWith({7, 8, 9});
        QCOMPARE(rec.log, (QStringList{"aboutToBeReset", "resetCompleted"}));
        QCOMPARE(inAbout, (QList<int>{1, 2}));
        QCOMPARE(inTwin, (QList<int>{7, 8, 9}));
    }

    [[= rqt::slot]] void at_3_5_resetWithEmitsEvenWhenBothAreEmpty()
    {
        obs::Container<QList<int>> c;
        Recorder<QList<int>> rec(c);
        c.resetWith({});
        QCOMPARE(rec.log, (QStringList{"aboutToBeReset", "resetCompleted"}));
    }

    [[= rqt::slot]] void at_3_6_replaceAndEditValueRoles()
    {
        using Source = obs::Container<QHash<QString, int>>;
        Source c(QHash<QString, int>{{"k", 1}});
        Recorder<QHash<QString, int>> rec(c);
        QVERIFY(c.replace("k", 2));
        QCOMPARE(rec.log, (QStringList{"entryDataAboutToChange(k,[])", "entryDataChanged(k,[])"}));

        rec.log.clear();
        int seenAbout = 0, seenTwin = 0;
        QObject ctx;
        QObject::connect(&c, &Source::entryDataAboutToChange, &ctx, [&] { seenAbout = *c.find("k"); });
        QObject::connect(&c, &Source::entryDataChanged, &ctx, [&] { seenTwin = *c.find("k"); });
        const int role = Qt::UserRole + 5;
        QVERIFY(c.editValue("k", [](int & v) { v = 42; }, {role}));
        QCOMPARE(rec.log, (QStringList{"entryDataAboutToChange(k,[261])", "entryDataChanged(k,[261])"}));
        QCOMPARE(seenAbout, 2);
        QCOMPARE(seenTwin, 42);
    }

    [[= rqt::slot]] void at_3_6_replaceWritesBetweenTheAboutAndTheTwin()
    {
        using Source = obs::Container<QHash<QString, int>>;
        Source c(QHash<QString, int>{{"k", 1}});
        int seenAbout = 0, seenTwin = 0;
        QObject ctx;
        QObject::connect(&c, &Source::entryDataAboutToChange, &ctx, [&] { seenAbout = *c.find("k"); });
        QObject::connect(&c, &Source::entryDataChanged, &ctx, [&] { seenTwin = *c.find("k"); });
        QVERIFY(c.replace("k", 2));
        QCOMPARE(seenAbout, 1);
        QCOMPARE(seenTwin, 2);
    }

    [[= rqt::slot]] void at_3_7_takeReturnsTheValueAndEmitsTheRemovalPair()
    {
        obs::Container<QHash<QString, int>> c(QHash<QString, int>{{"k", 5}});
        Recorder<QHash<QString, int>> rec(c);
        const std::optional<int> taken = c.take("k");
        QVERIFY(taken.has_value());
        QCOMPARE(*taken, 5);
        QCOMPARE(rec.log, (QStringList{"entryAboutToBeRemoved(k)", "entryRemoved(k)"}));
        QVERIFY(!c.contains("k"));

        obs::Container<QList<int>> list(QList<int>{4, 5, 6});
        Recorder<QList<int>> listRec(list);
        QCOMPARE(*list.take(1), 5);
        QCOMPARE(listRec.log, (QStringList{"entryAboutToBeRemoved(1)", "entryRemoved(1)"}));
        QCOMPARE(list.data(), (QList<int>{4, 6}));
    }

    [[= rqt::slot]] void at_3_8_insertManyIsOneEvent()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c(Map{{"a", 1}});
        Recorder<Map> rec(c);
        const qsizetype added = c.insertMany(Map{{"a", 99}, {"b", 2}, {"c", 3}, {"d", 4}});
        QCOMPARE(added, 3);
        QCOMPARE(rec.log, (QStringList{"entriesAboutToBeAdded([b,c,d])", "entriesAdded([b,c,d])"}));
        QCOMPARE(c.data().value("a"), 1);  // the present key was skipped, not replaced
        QCOMPARE(c.size(), 4);
    }

    [[= rqt::slot]] void at_3_8_insertManyOfNothingNewEmitsNothing()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c(Map{{"a", 1}});
        Recorder<Map> rec(c);
        QCOMPARE(c.insertMany(Map{{"a", 2}}), 0);
        QCOMPARE(c.insertMany(Map{}), 0);
        QVERIFY(rec.log.isEmpty());
    }

    [[= rqt::slot]] void at_3_9_removeManyIgnoresDuplicatesAndAbsentKeys()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c(Map{{"a", 1}, {"b", 2}});
        Recorder<Map> rec(c);
        QCOMPARE(c.removeMany({"a", "a", "zz"}), 1);
        QCOMPARE(rec.log, (QStringList{"entriesAboutToBeRemoved([a])", "entriesRemoved([a])"}));
        rec.log.clear();
        QCOMPARE(c.removeMany({"zz", "yy"}), 0);
        QVERIFY(rec.log.isEmpty());
    }

    [[= rqt::slot]] void at_3_9_removeManyKeepsTheCallersOrder()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c(Map{{"a", 1}, {"b", 2}, {"c", 3}});
        Recorder<Map> rec(c);
        QCOMPARE(c.removeMany({"c", "a", "c"}), 2);
        QCOMPARE(rec.log, (QStringList{"entriesAboutToBeRemoved([c,a])", "entriesRemoved([c,a])"}));
    }

    [[= rqt::slot]] void at_3_10_entriesMoveBetweenContainersKeysIntact()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> from(Map{{"a", 1}, {"b", 2}, {"c", 3}});
        obs::Container<Map> to;
        Recorder<Map> fromRec(from);
        Recorder<Map> toRec(to);
        to.insertMany(from.takeMany({"a", "b"}));
        QCOMPARE(to.data(), (Map{{"a", 1}, {"b", 2}}));
        QCOMPARE(from.data(), (Map{{"c", 3}}));
        QCOMPARE(fromRec.log, (QStringList{"entriesAboutToBeRemoved([a,b])", "entriesRemoved([a,b])"}));
        QCOMPARE(toRec.log, (QStringList{"entriesAboutToBeAdded([a,b])", "entriesAdded([a,b])"}));
    }

    [[= rqt::slot]] void at_3_11_onStructureChangedCountsStructuralChangesOnly()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c;
        QObject ctx;
        int calls = 0;
        obs::onStructureChanged(&c, &ctx, [&] { ++calls; });

        c.insert("a", 1);
        QCOMPARE(calls, 1);
        c.insertMany(Map{{"b", 2}, {"c", 3}, {"d", 4}});
        QCOMPARE(calls, 2);
        c.removeMany({"b", "c"});
        QCOMPARE(calls, 3);
        c.clear();
        QCOMPARE(calls, 4);
        c.resetWith(Map{{"x", 1}});
        QCOMPARE(calls, 5);
        c.replace("x", 2);
        QCOMPARE(calls, 5);
        c.editValue("x", [](int & v) { ++v; });
        QCOMPARE(calls, 5);
        c.remove("x");
        QCOMPARE(calls, 6);
    }

    [[= rqt::slot]] void at_3_11_aStatefulCallableSeesEveryCall()
    {
        obs::Container<QList<int>> c;
        QObject ctx;
        int seen = 0;
        obs::onStructureChanged(&c, &ctx, [&seen, own = 0]() mutable { seen = ++own; });
        c.append(1);
        c.append(2);
        c.remove(0);
        QCOMPARE(seen, 3);
    }

    [[= rqt::slot]] void at_3_12_onKeysAddedReceivesTheNewKeys()
    {
        using Map = QMap<QString, int>;
        obs::Container<Map> c;
        QObject ctx;
        QList<QString> got;
        int calls = 0;
        obs::onKeysAdded(&c, &ctx, [&](const QList<QString> & keys) {
            got = keys;
            ++calls;
        });
        c.insert("k", 1);
        QCOMPARE(got, (QList<QString>{"k"}));
        c.insertMany(Map{{"x", 1}, {"y", 2}, {"z", 3}});
        QCOMPARE(got, (QList<QString>{"x", "y", "z"}));
        QCOMPARE(calls, 2);
        c.remove("k");
        QCOMPARE(calls, 2);
    }

    // Beyond SPEC 12.3: what the binding itself must keep true.
    [[= rqt::slot]] void capability_derivedClassAddsItsOwnSignals()
    {
        Store store;
        QSignalSpy structure(&store, &Store::entryAdded);
        QSignalSpy unit(&store, &Store::unitChanged);
        store.insert(7u, 70);
        store.renameUnit(3);
        QCOMPARE(structure.count(), 1);
        QCOMPARE(structure.at(0).at(0).value<quint32>(), 7u);
        QCOMPARE(unit.count(), 1);
        QCOMPARE(unit.at(0).at(0).toInt(), 3);
    }

    [[= rqt::slot]] void capability_signalsReachQSignalSpyWithTypedKeys()
    {
        obs::Container<QHash<QString, int>> c;
        QSignalSpy added(&c, &obs::Container<QHash<QString, int>>::entriesAdded);
        c.insertMany(QHash<QString, int>{{"p", 1}, {"q", 2}});
        QCOMPARE(added.count(), 1);
        QList<QString> keys = added.at(0).at(0).value<QList<QString>>();
        std::sort(keys.begin(), keys.end());
        QCOMPARE(keys, (QList<QString>{"p", "q"}));
    }

    [[= rqt::slot]] void capability_stdVectorAndStdMapAreSupported()
    {
        obs::Container<std::vector<int>> v;
        Recorder<std::vector<int>> vecRec(v);
        v.append(5);
        v.insert(0, 4);
        QCOMPARE(*v.take(0), 4);
        QCOMPARE(vecRec.log, (QStringList{"entryAboutToBeAdded(0)", "entryAdded(0)", "entryAboutToBeAdded(0)", "entryAdded(0)",
                                          "entryAboutToBeRemoved(0)", "entryRemoved(0)"}));

        obs::Container<std::map<int, QString>> m;
        Recorder<std::map<int, QString>> mapRec(m);
        QVERIFY(m.insert(2, "two"));
        QVERIFY(!m.insert(2, "again"));
        std::map<int, QString> more{{3, "three"}, {4, "four"}};
        QCOMPARE(m.insertMany(std::move(more)), 2);
        const auto taken = m.takeMany({4, 2});
        QCOMPARE(taken.size(), std::size_t{2});
        QCOMPARE(m.size(), 1);
        QCOMPARE(mapRec.log.last(), QString("entriesRemoved([4,2])"));
    }
};

QTEST_GUILESS_MAIN(TstContainer)
