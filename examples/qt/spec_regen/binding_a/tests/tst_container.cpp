#include "obs/container.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QtTest>

#include <algorithm>
#include <map>
#include <type_traits>

namespace {

using CS = obs::ContainerSignals;
using Log = QList<QString>;

template <class K>
QString keyText(const QVariant & v)
{
    if constexpr (std::is_integral_v<K>)
        return QString::number(v.toLongLong());
    else
        return v.value<K>();
}

// One recorder per object: connects to EVERY signal, appends name and payload.
template <class K>
class Recorder : public QObject {
public:
    Log log;

    explicit Recorder(CS * s)
    {
        rec(s, &CS::entryAboutToBeAdded, "entryAboutToBeAdded");
        rec(s, &CS::entryAdded, "entryAdded");
        rec(s, &CS::entryAboutToBeRemoved, "entryAboutToBeRemoved");
        rec(s, &CS::entryRemoved, "entryRemoved");
        rec(s, &CS::entryDataAboutToChange, "entryDataAboutToChange");
        rec(s, &CS::entryDataChanged, "entryDataChanged");
        rec(s, &CS::entriesAboutToBeAdded, "entriesAboutToBeAdded");
        rec(s, &CS::entriesAdded, "entriesAdded");
        rec(s, &CS::entriesAboutToBeRemoved, "entriesAboutToBeRemoved");
        rec(s, &CS::entriesRemoved, "entriesRemoved");
        rec(s, &CS::aboutToBeCleared, "aboutToBeCleared");
        rec(s, &CS::cleared, "cleared");
        rec(s, &CS::aboutToBeReset, "aboutToBeReset");
        rec(s, &CS::resetCompleted, "resetCompleted");
    }

    int count(const QString & prefix) const
    {
        int n = 0;
        for (const QString & line : log)
            n += line.startsWith(prefix) ? 1 : 0;
        return n;
    }

private:
    QString text(const QVariant & v) const { return keyText<K>(v); }

    QString text(const QList<QVariant> & keys) const
    {
        QString out = QStringLiteral("[");
        for (qsizetype i = 0; i < keys.size(); ++i)
            out += (i ? QStringLiteral(",") : QString()) + keyText<K>(keys[i]);
        return out + QStringLiteral("]");
    }

    QString text(const QList<int> & roles) const
    {
        QString out = QStringLiteral("{");
        for (qsizetype i = 0; i < roles.size(); ++i)
            out += (i ? QStringLiteral(",") : QString()) + QString::number(roles[i]);
        return out + QStringLiteral("}");
    }

    template <class Sig>
    void rec(CS * s, Sig sig, const char * name)
    {
        connect(s, sig, this, [this, name](const auto &... args) {
            QString line = QString::fromLatin1(name);
            ((line += QStringLiteral(" ") + text(args)), ...);
            log.append(line);
        });
    }
};

QList<QString> sorted(QList<QString> keys)
{
    std::sort(keys.begin(), keys.end());
    return keys;
}

QList<QString> stringKeys(const QList<QVariant> & keys)
{
    QList<QString> out;
    for (const QVariant & k : keys)
        out.append(k.value<QString>());
    return out;
}

}  // namespace

class tst_container : public QObject {
    Q_OBJECT
private slots:
    void at_3_1_insertAnnouncesBeforeAndAfter()
    {
        obs::Container<QList<int>> c(QList<int>{10, 20, 30});
        Recorder<qsizetype> rec(&c);
        QObject ctx;
        qsizetype sizeAbout = -1, elementAbout = -1, sizeTwin = -1, elementTwin = -1;
        connect(&c, &CS::entryAboutToBeAdded, &ctx, [&] {
            sizeAbout = c.size();
            elementAbout = c.data()[1];
        });
        connect(&c, &CS::entryAdded, &ctx, [&] {
            sizeTwin = c.size();
            elementTwin = c.data()[1];
        });
        QVERIFY(c.insert(1, 15));
        QCOMPARE(rec.log, (Log{"entryAboutToBeAdded 1", "entryAdded 1"}));
        QCOMPARE(sizeAbout, 3);
        QCOMPARE(elementAbout, 20);
        QCOMPARE(sizeTwin, 4);
        QCOMPARE(elementTwin, 15);
    }

    void at_3_2_containsFlipsBetweenAboutAndTwin()
    {
        obs::Container<QHash<QString, int>> c;
        QObject ctx;
        bool inAbout = false, inTwin = false;
        QMetaObject::Connection a = connect(&c, &CS::entryAboutToBeAdded, &ctx, [&] { inAbout = c.contains("x"); });
        QMetaObject::Connection t = connect(&c, &CS::entryAdded, &ctx, [&] { inTwin = c.contains("x"); });
        QVERIFY(c.insert("x", 1));
        QVERIFY(!inAbout);
        QVERIFY(inTwin);
        disconnect(a);
        disconnect(t);
        connect(&c, &CS::entryAboutToBeRemoved, &ctx, [&] { inAbout = c.contains("x"); });
        connect(&c, &CS::entryRemoved, &ctx, [&] { inTwin = c.contains("x"); });
        QVERIFY(c.remove("x"));
        QVERIFY(inAbout);
        QVERIFY(!inTwin);
    }

    void at_3_3_rejectedMutationsEmitNothing()
    {
        obs::Container<QHash<QString, int>> h(QHash<QString, int>{{"a", 1}});
        Recorder<QString> hashRec(&h);
        QVERIFY(!h.insert("a", 2));
        QVERIFY(!h.remove("zz"));
        QVERIFY(!h.replace("zz", 1));
        QVERIFY(!h.editValue("zz", [](int & v) { ++v; }));
        QVERIFY(!h.take("zz").has_value());
        QVERIFY(hashRec.log.isEmpty());
        QCOMPARE(*h.find("a"), 1);

        obs::Container<QList<int>> l(QList<int>{1, 2, 3});
        Recorder<qsizetype> listRec(&l);
        QVERIFY(!l.insert(5, 9));
        QVERIFY(!l.remove(-1));
        QVERIFY(!l.take(3).has_value());
        QVERIFY(!l.replace(3, 1));
        QVERIFY(listRec.log.isEmpty());
        QCOMPARE(l.size(), 3);
    }

    void at_3_4_clear()
    {
        obs::Container<QList<int>> c;
        Recorder<qsizetype> rec(&c);
        c.clear();
        QVERIFY(rec.log.isEmpty());

        c.append(1);
        c.append(2);
        rec.log.clear();
        QObject ctx;
        qsizetype sizeAbout = -1;
        connect(&c, &CS::aboutToBeCleared, &ctx, [&] { sizeAbout = c.size(); });
        c.clear();
        QCOMPARE(rec.log, (Log{"aboutToBeCleared", "cleared"}));
        QCOMPARE(sizeAbout, 2);
        QCOMPARE(c.size(), 0);
    }

    void at_3_5_resetWithIsOnePair()
    {
        obs::Container<QList<int>> c(QList<int>{1, 2});
        Recorder<qsizetype> rec(&c);
        QObject ctx;
        QList<int> inAbout, inTwin;
        connect(&c, &CS::aboutToBeReset, &ctx, [&] { inAbout = c.data(); });
        connect(&c, &CS::resetCompleted, &ctx, [&] { inTwin = c.data(); });
        c.resetWith(QList<int>{7, 8, 9});
        QCOMPARE(rec.log, (Log{"aboutToBeReset", "resetCompleted"}));
        QCOMPARE(inAbout, (QList<int>{1, 2}));
        QCOMPARE(inTwin, (QList<int>{7, 8, 9}));

        // always emits its pair, even when old and new content are both empty
        obs::Container<QList<int>> empty;
        Recorder<qsizetype> emptyRec(&empty);
        empty.resetWith({});
        QCOMPARE(emptyRec.log, (Log{"aboutToBeReset", "resetCompleted"}));
    }

    void at_3_6_replaceAndEditValueRoles()
    {
        obs::Container<QHash<QString, int>> c(QHash<QString, int>{{"k", 1}});
        Recorder<QString> rec(&c);
        QObject ctx;
        int oldInAbout = -1, newInTwin = -1;
        connect(&c, &CS::entryDataAboutToChange, &ctx, [&] { oldInAbout = *c.find("k"); });
        connect(&c, &CS::entryDataChanged, &ctx, [&] { newInTwin = *c.find("k"); });

        QVERIFY(c.replace("k", 5));
        QCOMPARE(rec.log, (Log{"entryDataAboutToChange k {}", "entryDataChanged k {}"}));
        QCOMPARE(oldInAbout, 1);
        QCOMPARE(newInTwin, 5);

        rec.log.clear();
        const int role = Qt::UserRole + 1;
        QVERIFY(c.editValue("k", [](int & v) { v += 10; }, QList<int>{role}));
        const QString roleText = QString::number(role);
        QCOMPARE(rec.log, (Log{"entryDataAboutToChange k {" + roleText + "}", "entryDataChanged k {" + roleText + "}"}));
        QCOMPARE(oldInAbout, 5);
        QCOMPARE(newInTwin, 15);
    }

    void at_3_7_takeReturnsTheValueAndEmitsTheRemovalPair()
    {
        obs::Container<QHash<QString, int>> c(QHash<QString, int>{{"k", 42}});
        Recorder<QString> rec(&c);
        const std::optional<int> taken = c.take("k");
        QVERIFY(taken.has_value());
        QCOMPARE(*taken, 42);
        QCOMPARE(rec.log, (Log{"entryAboutToBeRemoved k", "entryRemoved k"}));
        QVERIFY(!c.contains("k"));
    }

    void at_3_8_insertManyIsOnePair()
    {
        obs::Container<QHash<QString, int>> c(QHash<QString, int>{{"present", 0}});
        Recorder<QString> rec(&c);
        QObject ctx;
        QList<QString> aboutKeys, twinKeys;
        connect(&c, &CS::entriesAboutToBeAdded, &ctx, [&](const QList<QVariant> & k) { aboutKeys = stringKeys(k); });
        connect(&c, &CS::entriesAdded, &ctx, [&](const QList<QVariant> & k) { twinKeys = stringKeys(k); });

        const qsizetype added = c.insertMany(QHash<QString, int>{{"a", 1}, {"b", 2}, {"c", 3}, {"present", 9}});
        QCOMPARE(added, 3);
        QCOMPARE(rec.count("entriesAboutToBeAdded"), 1);
        QCOMPARE(rec.count("entriesAdded"), 1);
        QCOMPARE(rec.log.size(), 2);
        QCOMPARE(sorted(aboutKeys), (QList<QString>{"a", "b", "c"}));
        QCOMPARE(sorted(twinKeys), (QList<QString>{"a", "b", "c"}));
        QCOMPARE(*c.find("present"), 0);  // a present key is skipped, not replaced

        rec.log.clear();
        QCOMPARE(c.insertMany(QHash<QString, int>{{"a", 1}}), 0);
        QVERIFY(rec.log.isEmpty());
    }

    void at_3_9_removeManyIsOnePair()
    {
        obs::Container<QHash<QString, int>> c(QHash<QString, int>{{"a", 1}, {"b", 2}});
        Recorder<QString> rec(&c);
        QCOMPARE(c.removeMany(QList<QString>{"a", "a", "zz"}), 1);
        QCOMPARE(rec.log, (Log{"entriesAboutToBeRemoved [a]", "entriesRemoved [a]"}));
        QCOMPARE(rec.count("entryAboutToBeRemoved"), 0);

        rec.log.clear();
        QCOMPARE(c.removeMany(QList<QString>{"zz", "yy"}), 0);
        QVERIFY(rec.log.isEmpty());
    }

    void at_3_10_takeManyMovesEntriesBetweenContainers()
    {
        obs::Container<QHash<QString, int>> from(QHash<QString, int>{{"a", 1}, {"b", 2}, {"c", 3}});
        obs::Container<QHash<QString, int>> to;
        Recorder<QString> fromRec(&from);
        Recorder<QString> toRec(&to);
        const qsizetype moved = to.insertMany(from.takeMany(QList<QString>{"a", "b"}));
        QCOMPARE(moved, 2);
        QCOMPARE(*to.find("a"), 1);
        QCOMPARE(*to.find("b"), 2);
        QVERIFY(!from.contains("a"));
        QVERIFY(!from.contains("b"));
        QVERIFY(from.contains("c"));
        QCOMPARE(fromRec.log, (Log{"entriesAboutToBeRemoved [a,b]", "entriesRemoved [a,b]"}));
        QCOMPARE(toRec.count("entriesAboutToBeAdded"), 1);
        QCOMPARE(toRec.count("entriesAdded"), 1);
        QCOMPARE(toRec.log.size(), 2);

        // nothing removed gives an empty container
        QVERIFY(from.takeMany(QList<QString>{"zz"}).isEmpty());
    }

    void at_3_11_onStructureChangedCounts()
    {
        obs::Container<QHash<QString, int>> c(QHash<QString, int>{{"seed", 0}});
        QObject ctx;
        int calls = 0;
        obs::onStructureChanged(&c, &ctx, [&calls] { ++calls; });

        c.insert("x", 1);
        QCOMPARE(calls, 1);
        c.insertMany(QHash<QString, int>{{"a", 1}, {"b", 2}, {"c", 3}});
        QCOMPARE(calls, 2);
        c.removeMany(QList<QString>{"a", "b"});
        QCOMPARE(calls, 3);
        c.clear();
        QCOMPARE(calls, 4);
        c.resetWith(QHash<QString, int>{{"r", 1}});
        QCOMPARE(calls, 5);
        c.replace("r", 2);
        QCOMPARE(calls, 5);
        c.editValue("r", [](int & v) { ++v; });
        QCOMPARE(calls, 5);
    }

    void at_3_12_onKeysAddedReceivesTheKeys()
    {
        obs::Container<QHash<QString, int>> c;
        QObject ctx;
        QList<QList<QString>> received;
        obs::onKeysAdded(&c, &ctx, [&](const QList<QVariant> & keys) { received.append(sorted(stringKeys(keys))); });

        c.insert("k", 1);
        QCOMPARE(received.size(), 1);
        QCOMPARE(received[0], (QList<QString>{"k"}));

        c.insertMany(QHash<QString, int>{{"a", 1}, {"b", 2}, {"c", 3}});
        QCOMPARE(received.size(), 2);
        QCOMPARE(received[1], (QList<QString>{"a", "b", "c"}));
    }

    // Not an acceptance test: the other container kinds work the same way.
    void extra_sequentialAppendInsertTake()
    {
        obs::Container<std::vector<QString>> c;
        Recorder<qsizetype> rec(&c);
        QVERIFY(c.append("a"));
        QVERIFY(c.insert(0, "b"));
        QCOMPARE(c.size(), 2);
        QCOMPARE(c.data()[0], QStringLiteral("b"));
        const std::optional<QString> taken = c.take(0);
        QVERIFY(taken.has_value());
        QCOMPARE(*taken, QStringLiteral("b"));
        QCOMPARE(rec.log, (Log{"entryAboutToBeAdded 0", "entryAdded 0", "entryAboutToBeAdded 0", "entryAdded 0",
                               "entryAboutToBeRemoved 0", "entryRemoved 0"}));
    }

    void extra_stdMapAndQMapWork()
    {
        obs::Container<std::map<int, QString>> m;
        QVERIFY(m.insert(2, "two"));
        QVERIFY(!m.insert(2, "again"));
        QCOMPARE(m.insertMany(std::map<int, QString>{{1, "one"}, {2, "x"}, {3, "three"}}), 2);
        QVERIFY(m.replace(1, "uno"));
        QCOMPARE(*m.find(1), QStringLiteral("uno"));
        const std::map<int, QString> taken = m.takeMany(QList<int>{1, 3});
        QCOMPARE(taken.size(), std::size_t{2});
        QCOMPARE(m.removeMany(QList<int>{2}), 1);
        QCOMPARE(m.size(), 0);

        obs::Container<QMap<QString, int>> q(QMap<QString, int>{{"a", 1}});
        QVERIFY(q.editValue("a", [](int & v) { v = 7; }));
        QCOMPARE(*q.find("a"), 7);
        QCOMPARE(q.insertMany(QMap<QString, int>{{"b", 2}}), 1);
        QCOMPARE(q.removeMany(QList<QString>{"a", "b"}), 2);
    }

    // A copy of the data taken before a change must not see the change.
    void extra_editValueDetachesFromSharedCopies()
    {
        const QHash<QString, int> original{{"a", 1}};
        obs::Container<QHash<QString, int>> c(original);
        QVERIFY(c.editValue("a", [](int & v) { v = 2; }));
        QCOMPARE(original.value("a"), 1);
        QCOMPARE(*c.find("a"), 2);
    }

    // Added after mutation testing (a mutant that announced a batch after its inserts survived): the "about" of a batch sees the
    // old content and its twin the new one, for every batch operation.
    void at_3_13_theAboutOfABatchSeesTheOldContent()
    {
        obs::Container<QHash<QString, int>> c(QHash<QString, int>{{"present", 0}});
        QObject ctx;
        qsizetype atAbout = -1, atTwin = -1;
        connect(&c, &CS::entriesAboutToBeAdded, &ctx, [&] { atAbout = c.size(); });
        connect(&c, &CS::entriesAdded, &ctx, [&] { atTwin = c.size(); });
        QCOMPARE(c.insertMany(QHash<QString, int>{{"a", 1}, {"b", 2}}), 2);
        QCOMPARE(atAbout, 1);
        QCOMPARE(atTwin, 3);

        connect(&c, &CS::entriesAboutToBeRemoved, &ctx, [&] { atAbout = c.size(); });
        connect(&c, &CS::entriesRemoved, &ctx, [&] { atTwin = c.size(); });
        QCOMPARE(c.removeMany(QList<QString>{"a", "b"}), 2);
        QCOMPARE(atAbout, 3);
        QCOMPARE(atTwin, 1);
        c.insertMany(QHash<QString, int>{{"x", 1}});
        const QHash<QString, int> taken = c.takeMany(QList<QString>{"x"});
        QCOMPARE(taken.size(), 1);
        QCOMPARE(atAbout, 2);
        QCOMPARE(atTwin, 1);
    }
};

QTEST_GUILESS_MAIN(tst_container)
#include "tst_container.moc"
