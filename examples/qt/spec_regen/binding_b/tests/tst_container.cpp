// SPEC 12.3: the one-dimensional container, binding B (typed signals declared on the template).
#include <obs/container.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <reflect_moc/qt/qt.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace {

// SPEC addendum A.6: QHash<int, QString> unless a test says otherwise
using Names = QHash<int, QString>;
using NamesSource = obs::Container<Names>;

constexpr int kChainEnd = 100;  // AT-10.4: the last key of the chain of reactions

QList<int> sortedKeys(const NamesSource & c)
{
    QList<int> keys = c.data().keys();
    std::sort(keys.begin(), keys.end());
    return keys;
}

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

    // Added after mutation testing (a mutant that announced a batch after its inserts survived): the "about" of a batch sees the old
    // content and its twin the new one, for every batch operation.
    [[= rqt::slot]] void at_3_13_theAboutOfABatchSeesTheOldContent()
    {
        using Source = obs::Container<QHash<QString, int>>;
        Source c(QHash<QString, int>{{"present", 0}});
        QObject ctx;
        qsizetype atAbout = -1, atTwin = -1;
        QObject::connect(&c, &Source::entriesAboutToBeAdded, &ctx, [&] { atAbout = c.size(); });
        QObject::connect(&c, &Source::entriesAdded, &ctx, [&] { atTwin = c.size(); });
        QCOMPARE(c.insertMany(QHash<QString, int>{{"a", 1}, {"b", 2}}), 2);
        QCOMPARE(atAbout, 1);
        QCOMPARE(atTwin, 3);

        QObject::connect(&c, &Source::entriesAboutToBeRemoved, &ctx, [&] { atAbout = c.size(); });
        QObject::connect(&c, &Source::entriesRemoved, &ctx, [&] { atTwin = c.size(); });
        QCOMPARE(c.removeMany({"a", "b"}), 2);
        QCOMPARE(atAbout, 3);
        QCOMPARE(atTwin, 1);
        c.insertMany(QHash<QString, int>{{"x", 1}});
        QCOMPARE(c.takeMany({"x"}).size(), 1);
        QCOMPARE(atAbout, 2);
        QCOMPARE(atTwin, 1);
    }

    // SPEC addendum A.6 (section 12.10): a change made while the container is notifying is queued and applied once the
    // gesture is complete (N1 to N11). Qt runs receivers in connection order, so a test that must tell a queued change from an
    // immediate one connects the reacting receiver BEFORE the recorder: an immediate change would be logged ahead of the
    // notification that provoked it. A receiver that would provoke itself again acts the first time only.

    [[= rqt::slot]] void at_10_1_aNestedRemoveWaitsForTheOuterTwin()
    {
        NamesSource c(Names{{1, "one"}, {2, "two"}});
        QObject ctx;
        bool nestedResult = false;
        QObject::connect(&c, &NamesSource::entryAboutToBeRemoved, &ctx, [&](const int & key) {
            if (key == 1)
                nestedResult = c.remove(2);
        });
        Recorder<Names> rec(c);

        QVERIFY(c.remove(1));

        QVERIFY(c.data().isEmpty());
        QCOMPARE(rec.log, (QStringList{"entryAboutToBeRemoved(1)", "entryRemoved(1)", "entryAboutToBeRemoved(2)", "entryRemoved(2)"}));
        QVERIFY(nestedResult);
    }

    [[= rqt::slot]] void at_10_2_isNotifyingIsTrueInsideAReceiverAndFalseAfterTheOuterCall()
    {
        NamesSource c(Names{{1, "one"}});
        QObject ctx;
        bool atAbout = false, atTwin = false;
        QVERIFY(!c.isNotifying());
        QObject::connect(&c, &NamesSource::entryAboutToBeRemoved, &ctx, [&] { atAbout = c.isNotifying(); });
        QObject::connect(&c, &NamesSource::entryRemoved, &ctx, [&] { atTwin = c.isNotifying(); });

        QVERIFY(c.remove(1));

        QVERIFY(atAbout);
        QVERIFY(atTwin);  // N1: the scope closes when the call returns, so it is still open in the receiver of the twin
        QVERIFY(!c.isNotifying());
    }

    [[= rqt::slot]] void at_10_3_queuedCallsRunInTheOrderTheyWereQueued()
    {
        NamesSource c;
        QObject ctx;
        QObject::connect(&c, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 1) {
                c.insert(5, "five");
                c.remove(5);
            }
        });
        Recorder<Names> rec(c);

        QVERIFY(c.insert(1, "one"));

        QVERIFY(c.contains(1));
        QVERIFY(!c.contains(5));
        QCOMPARE(rec.log, (QStringList{"entryAboutToBeAdded(1)", "entryAdded(1)", "entryAboutToBeAdded(5)", "entryAdded(5)",
                                       "entryAboutToBeRemoved(5)", "entryRemoved(5)"}));
    }

    [[= rqt::slot]] void at_10_4_aChainOfReactionsNeverNestsDeeperThanOne()
    {
        NamesSource c;
        QObject ctx;
        int depth = 0, deepest = 0;
        QObject::connect(&c, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            deepest = std::max(deepest, ++depth);
            if (key < kChainEnd)
                c.insert(key + 1, QString::number(key + 1));
            --depth;
        });

        QVERIFY(c.insert(1, "1"));

        QCOMPARE(c.size(), kChainEnd);
        for (int key = 1; key <= kChainEnd; ++key)
            QVERIFY(c.contains(key));
        QCOMPARE(deepest, 1);
    }

    [[= rqt::slot]] void at_10_5_removingTheSameKeyInsideItsOwnAboutAnnouncesItOnce()
    {
        NamesSource c(Names{{1, "one"}});
        QSignalSpy removed(&c, &NamesSource::entryRemoved);
        QObject ctx;
        bool first = true;
        bool nestedResult = false;
        QObject::connect(&c, &NamesSource::entryAboutToBeRemoved, &ctx, [&] {
            if (first) {
                first = false;
                nestedResult = c.remove(1);
            }
        });

        QVERIFY(c.remove(1));

        QCOMPARE(removed.count(), 1);
        QVERIFY(c.data().isEmpty());
        QVERIFY(nestedResult);
    }

    [[= rqt::slot]] void at_10_5_takingTheSameKeyInsideItsOwnAboutIsRefused()
    {
        NamesSource c(Names{{1, "one"}});
        QObject ctx;
        bool first = true;
        std::optional<QString> nestedResult{QStringLiteral("untouched")};
        QObject::connect(&c, &NamesSource::entryAboutToBeRemoved, &ctx, [&] {
            if (first) {
                first = false;
                nestedResult = c.take(1);
            }
        });

        const std::optional<QString> outerResult = c.take(1);

        QVERIFY(!nestedResult.has_value());
        QVERIFY(outerResult.has_value());
        QCOMPARE(*outerResult, QString("one"));
        QVERIFY(c.data().isEmpty());
    }

    [[= rqt::slot]] void at_10_6_aNestedRemoveInsideABatchRemovalIsAnnouncedOnlyInTheBatch()
    {
        Names content;
        for (int key = 1; key <= 10; ++key)
            content.insert(key, QString::number(key));
        NamesSource c(content);
        QObject ctx;
        QObject::connect(&c, &NamesSource::entriesAboutToBeRemoved, &ctx, [&] { c.remove(5); });
        Recorder<Names> rec(c);

        QCOMPARE(c.removeMany({2, 5, 8}), 3);

        QCOMPARE(rec.log, (QStringList{"entriesAboutToBeRemoved([2,5,8])", "entriesRemoved([2,5,8])"}));
        QCOMPARE(sortedKeys(c), (QList<int>{1, 3, 4, 6, 7, 9, 10}));
    }

    [[= rqt::slot]] void at_10_7_takeManyDuringANotificationReturnsNothingAndQueuesNothing()
    {
        NamesSource c(Names{{2, "two"}, {3, "three"}});
        QObject ctx;
        Names taken{{0, "untouched"}};
        QObject::connect(&c, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 1)
                taken = c.takeMany({2, 3});
        });
        Recorder<Names> rec(c);

        QVERIFY(c.insert(1, "one"));

        QVERIFY(taken.isEmpty());
        QCOMPARE(sortedKeys(c), (QList<int>{1, 2, 3}));  // neither taken now nor queued for the drain
        QCOMPARE(rec.log, (QStringList{"entryAboutToBeAdded(1)", "entryAdded(1)"}));
    }

    [[= rqt::slot]] void at_10_8_aQueuedEditValueRunsAfterTheOuterTwinWithItsRoles()
    {
        const int role = Qt::UserRole + 5;
        NamesSource c(Names{{7, "seven"}});
        Recorder<Names> rec(c);
        QObject ctx;
        bool nestedResult = false;
        // f is queued and called at its turn, which is before the outer insert returns: the references stay alive until then
        QObject::connect(&c, &NamesSource::entryAboutToBeAdded, &ctx, [&] {
            nestedResult = c.editValue(7, [&rec](QString & value) {
                rec.log << QStringLiteral("f");
                value = QStringLiteral("SEVEN");
            }, {role});
        });

        QVERIFY(c.insert(1, "one"));

        QVERIFY(nestedResult);
        QCOMPARE(rec.log, (QStringList{"entryAboutToBeAdded(1)", "entryAdded(1)", "entryDataAboutToChange(7,[261])", "f", "entryDataChanged(7,[261])"}));
        QCOMPARE(*c.find(7), QString("SEVEN"));
    }

    [[= rqt::slot]] void at_10_9_aQueuedClearAndAQueuedResetAreAppliedAtTheirTurn()
    {
        QObject ctx;

        NamesSource toClear(Names{{1, "one"}, {2, "two"}});
        QObject::connect(&toClear, &NamesSource::entryAboutToBeAdded, &ctx, [&] { toClear.clear(); });
        Recorder<Names> clearRec(toClear);
        qsizetype sizeAtClear = -1;
        QObject::connect(&toClear, &NamesSource::aboutToBeCleared, &ctx, [&] { sizeAtClear = toClear.size(); });
        QVERIFY(toClear.insert(3, "three"));
        QCOMPARE(clearRec.log, (QStringList{"entryAboutToBeAdded(3)", "entryAdded(3)", "aboutToBeCleared", "cleared"}));
        QCOMPARE(sizeAtClear, 3);  // decided at its turn: the outer insert is complete by then
        QVERIFY(toClear.data().isEmpty());

        NamesSource toReset(Names{{1, "one"}});
        QObject::connect(&toReset, &NamesSource::entryAboutToBeAdded, &ctx, [&] { toReset.resetWith(Names{{9, "nine"}}); });
        Recorder<Names> resetRec(toReset);
        Names contentAtReset;
        QObject::connect(&toReset, &NamesSource::aboutToBeReset, &ctx, [&] { contentAtReset = toReset.data(); });
        QVERIFY(toReset.insert(3, "three"));
        QCOMPARE(resetRec.log, (QStringList{"entryAboutToBeAdded(3)", "entryAdded(3)", "aboutToBeReset", "resetCompleted"}));
        QCOMPARE(contentAtReset, (Names{{1, "one"}, {3, "three"}}));
        QCOMPARE(toReset.data(), (Names{{9, "nine"}}));
    }

    // Beyond the twelve: what the points of the addendum without an acceptance test of their own say.

    // N2 and N8: the call is kept, not applied where it is made, and it reports that it was queued
    [[= rqt::slot]] void capability_aQueuedCallIsKeptAndNotAppliedWhereItIsMade()
    {
        NamesSource c(Names{{1, "one"}});
        QObject ctx;
        bool queuedResult = false, presentInside = true;
        qsizetype sizeInside = -1;
        QObject::connect(&c, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 2) {
                queuedResult = c.insert(5, "five");
                presentInside = c.contains(5);
                sizeInside = c.size();
            }
        });

        QVERIFY(c.insert(2, "two"));

        QVERIFY(queuedResult);
        QVERIFY(!presentInside);
        QCOMPARE(sizeInside, 2);
        QVERIFY(c.contains(5));
    }

    // N6: a call on another container follows that container's own state
    [[= rqt::slot]] void capability_aCallOnAnotherContainerFollowsThatContainersOwnState()
    {
        NamesSource a, b;
        QObject ctx;
        bool appliedAtOnce = false, queuedInA = true, bBusyInsideA = true, aBusyInsideB = false;
        QObject::connect(&a, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 1) {
                bBusyInsideA = b.isNotifying();
                appliedAtOnce = b.insert(10, "ten") && b.contains(10);
            }
        });
        QObject::connect(&b, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 10) {
                aBusyInsideB = a.isNotifying();
                a.insert(2, "two");
                queuedInA = a.contains(2);
            }
        });

        QVERIFY(a.insert(1, "one"));

        QVERIFY(!bBusyInsideA);
        QVERIFY(appliedAtOnce);
        QVERIFY(aBusyInsideB);
        QVERIFY(!queuedInA);
        QCOMPARE(sortedKeys(a), (QList<int>{1, 2}));
        QVERIFY(!a.isNotifying());
        QVERIFY(!b.isNotifying());
    }

    // N4: the calls a queued call provokes go to the back of the same queue
    [[= rqt::slot]] void capability_callsQueuedByAQueuedCallGoToTheBackOfTheQueue()
    {
        NamesSource c;
        QObject ctx;
        QObject::connect(&c, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 1) {
                c.insert(2, "two");
                c.insert(3, "three");
            } else if (key == 2) {
                c.insert(4, "four");
            }
        });
        Recorder<Names> rec(c);

        QVERIFY(c.insert(1, "one"));

        QCOMPARE(rec.log.filter(QStringLiteral("entryAdded(")), (QStringList{"entryAdded(1)", "entryAdded(2)", "entryAdded(3)", "entryAdded(4)"}));
    }

    // N11: on a sequential container an index means the state when the call is applied
    [[= rqt::slot]] void capability_aQueuedSequentialIndexMeansTheStateAtItsTurn()
    {
        obs::Container<QList<int>> c(QList<int>{10, 20, 30});
        QObject ctx;
        bool first = true;
        QObject::connect(&c, &obs::Container<QList<int>>::entryRemoved, &ctx, [&] {
            if (first) {
                first = false;
                c.remove(0);
                c.append(99);
                c.remove(0);
            }
        });
        Recorder<QList<int>> rec(c);

        QVERIFY(c.remove(0));

        QCOMPARE(c.data(), (QList<int>{99}));
        QCOMPARE(rec.log, (QStringList{"entryAboutToBeRemoved(0)", "entryRemoved(0)", "entryAboutToBeRemoved(0)", "entryRemoved(0)",
                                       "entryAboutToBeAdded(1)", "entryAdded(1)", "entryAboutToBeRemoved(0)", "entryRemoved(0)"}));
    }

    // N10: a queued call owns what it was given, and what it was given may be move-only
    [[= rqt::slot]] void capability_aQueuedCallCanOwnAMoveOnlyValueAndAMoveOnlyCallable()
    {
        using Owned = std::map<int, std::unique_ptr<int>>;
        obs::Container<Owned> c;
        QObject ctx;
        int seen = 0;
        QObject::connect(&c, &obs::Container<Owned>::entryAdded, &ctx, [&](const int & key) {
            if (key == 1) {
                c.insert(2, std::make_unique<int>(20));
                c.editValue(2, [bonus = std::make_unique<int>(5), &seen](std::unique_ptr<int> & value) {
                    *value += *bonus;
                    seen = *value;
                });
            }
        });

        QVERIFY(c.insert(1, std::make_unique<int>(10)));

        QCOMPARE(seen, 25);
        QCOMPARE(c.size(), 2);
    }

    // Readings of mine where the addendum is silent (R2: change nothing and emit nothing).

    // N8: "true" and "the number of keys requested" report the queueing, so a call that cannot succeed still answers so
    [[= rqt::slot]] void reading_aQueuedCallAnswersQueuedEvenWhenItWillBeRejectedAtItsTurn()
    {
        obs::Container<QList<int>> list(QList<int>{1});
        QObject ctx;
        bool insertBeyond = false, removeNegative = false, replaceBeyond = false, editBeyond = false;
        bool editRan = false;
        QObject::connect(&list, &obs::Container<QList<int>>::entryAdded, &ctx, [&](const qsizetype key) {
            if (key == 1) {
                insertBeyond = list.insert(50, 0);
                removeNegative = list.remove(-1);
                replaceBeyond = list.replace(50, 0);
                editBeyond = list.editValue(50, [&editRan](int &) { editRan = true; });
            }
        });
        Recorder<QList<int>> rec(list);

        QVERIFY(list.append(2));

        QVERIFY(insertBeyond && removeNegative && replaceBeyond && editBeyond);
        QVERIFY(!editRan);
        QCOMPARE(list.data(), (QList<int>{1, 2}));
        QCOMPARE(rec.log, (QStringList{"entryAboutToBeAdded(1)", "entryAdded(1)"}));

        NamesSource c(Names{{1, "one"}});
        QObject::connect(&c, &NamesSource::entryAboutToBeAdded, &ctx, [&] {
            // "requested" is the size of the argument: a present key, a duplicate and an absent key all count
            QCOMPARE(c.insertMany(Names{{1, "again"}, {2, "two"}, {3, "three"}}), 3);
            QCOMPARE(c.removeMany({1, 1, 99}), 3);
            QCOMPARE(c.insertMany(Names{}), 0);
        });
        QVERIFY(c.insert(7, "seven"));
        QCOMPARE(sortedKeys(c), (QList<int>{2, 3, 7}));
    }

    // A receiver that throws: the gesture ends by unwinding, the container is not left notifying, and the calls still queued
    // are dropped, not applied by a later gesture (nothing is emitted from a destructor while unwinding).
    [[= rqt::slot]] void reading_aReceiverThatThrowsLeavesTheContainerUsableAndDropsTheQueue()
    {
        NamesSource c;
        QObject ctx;
        QObject::connect(&c, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 1) {
                c.insert(2, "two");
                throw key;
            }
        });

        bool thrown = false;
        try {
            c.insert(1, "one");
        } catch (int) {
            thrown = true;
        }

        QVERIFY(thrown);
        QVERIFY(!c.isNotifying());
        QVERIFY(c.contains(1));
        QVERIFY(!c.contains(2));
        QVERIFY(c.insert(3, "three"));  // applied at once, and it does not revive the dropped call
        QCOMPARE(sortedKeys(c), (QList<int>{1, 3}));
    }

    // The same when the receiver that throws belongs to a drained call: the exception leaves the outer call, and what was still
    // queued behind the failing call is dropped.
    [[= rqt::slot]] void reading_aReceiverOfADrainedCallThatThrowsDropsTheRestOfTheQueue()
    {
        NamesSource c;
        QObject ctx;
        QObject::connect(&c, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 1) {
                c.insert(2, "two");
                c.insert(3, "three");
            } else if (key == 2) {
                throw key;
            }
        });

        bool thrown = false;
        try {
            c.insert(1, "one");
        } catch (int) {
            thrown = true;
        }

        QVERIFY(thrown);
        QVERIFY(!c.isNotifying());
        QCOMPARE(sortedKeys(c), (QList<int>{1, 2}));  // 3 was queued behind 2 and is dropped
        QVERIFY(c.insert(4, "four"));
        QCOMPARE(sortedKeys(c), (QList<int>{1, 2, 4}));
    }

    // A receiver that destroys the container, in the outer call and in a drained call: the gesture ends without touching it.
    // (Destroying it from a receiver of an "about" stays undefined: the mutation still has to follow.)
    [[= rqt::slot]] void reading_aContainerDestroyedByAReceiverOfATwinEndsTheGestureQuietly()
    {
        QObject ctx;

        auto * outer = new NamesSource;
        QObject::connect(outer, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 1) {
                outer->insert(2, "two");  // queued, and never applied: the container is gone before the drain
                delete outer;
            }
        });
        QVERIFY(outer->insert(1, "one"));

        auto * drained = new NamesSource;
        QObject::connect(drained, &NamesSource::entryAdded, &ctx, [&](const int & key) {
            if (key == 1)
                drained->insert(2, "two");
            else if (key == 2)
                delete drained;
        });
        QVERIFY(drained->insert(1, "one"));
    }
};

QTEST_GUILESS_MAIN(TstContainer)
