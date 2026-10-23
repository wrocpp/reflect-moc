#include "obs/container.h"
#include "obs/container_list_model.h"

#include <QAbstractItemModelTester>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QtTest>

#include <algorithm>
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

// The test-local model of 12.4: DisplayRole is the value itself.
template <class C>
class TestModel : public obs::ContainerListModel<C> {
public:
    using Base = obs::ContainerListModel<C>;
    using Base::Base;

    QHash<int, QByteArray> roleNames() const override
    {
        QHash<int, QByteArray> names = Base::roleNames();
        names.insert(Qt::UserRole + 1, "extra");
        return names;
    }

protected:
    QVariant valueData(const typename Base::Key &, const typename Base::Value & value, int role) const override
    {
        return role == Qt::DisplayRole ? QVariant::fromValue(value) : QVariant();
    }
};

// Records every structural and data signal of a model.
class ModelRecorder : public QObject {
public:
    Log log;

    explicit ModelRecorder(QAbstractItemModel * m)
    {
        connect(m, &QAbstractItemModel::rowsAboutToBeInserted, this,
                [this](const QModelIndex &, int f, int l) { add("rowsAboutToBeInserted", f, l); });
        connect(m, &QAbstractItemModel::rowsInserted, this,
                [this](const QModelIndex &, int f, int l) { add("rowsInserted", f, l); });
        connect(m, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                [this](const QModelIndex &, int f, int l) { add("rowsAboutToBeRemoved", f, l); });
        connect(m, &QAbstractItemModel::rowsRemoved, this,
                [this](const QModelIndex &, int f, int l) { add("rowsRemoved", f, l); });
        connect(m, &QAbstractItemModel::modelAboutToBeReset, this, [this, m] {
            log.append(QStringLiteral("modelAboutToBeReset rows=") + QString::number(m->rowCount()));
        });
        connect(m, &QAbstractItemModel::modelReset, this, [this, m] {
            log.append(QStringLiteral("modelReset rows=") + QString::number(m->rowCount()));
        });
        connect(m, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex & a, const QModelIndex & b, const QList<int> & roles) {
                    QString line = QStringLiteral("dataChanged ") + QString::number(a.row()) + QStringLiteral(" ")
                                   + QString::number(b.row()) + QStringLiteral(" {");
                    for (qsizetype i = 0; i < roles.size(); ++i)
                        line += (i ? QStringLiteral(",") : QString()) + QString::number(roles[i]);
                    log.append(line + QStringLiteral("}"));
                });
        connect(m, &QAbstractItemModel::layoutChanged, this, [this] { log.append(QStringLiteral("layoutChanged")); });
        connect(m, &QAbstractItemModel::rowsMoved, this, [this] { log.append(QStringLiteral("rowsMoved")); });
    }

private:
    void add(const char * name, int first, int last)
    {
        log.append(QString::fromLatin1(name) + QStringLiteral(" ") + QString::number(first) + QStringLiteral(" ")
                   + QString::number(last));
    }
};

// Records the source container's signals, keys as text.
template <class K>
class SourceRecorder : public QObject {
public:
    Log log;

    explicit SourceRecorder(CS * s)
    {
        connect(s, &CS::entriesAboutToBeAdded, this, [this](const QList<QVariant> & k) { add("entriesAboutToBeAdded", k); });
        connect(s, &CS::entriesAdded, this, [this](const QList<QVariant> & k) { add("entriesAdded", k); });
        connect(s, &CS::entriesAboutToBeRemoved, this, [this](const QList<QVariant> & k) { add("entriesAboutToBeRemoved", k); });
        connect(s, &CS::entriesRemoved, this, [this](const QList<QVariant> & k) { add("entriesRemoved", k); });
    }

private:
    void add(const char * name, const QList<QVariant> & keys)
    {
        QString line = QString::fromLatin1(name) + QStringLiteral(" [");
        for (qsizetype i = 0; i < keys.size(); ++i)
            line += (i ? QStringLiteral(",") : QString()) + keyText<K>(keys[i]);
        log.append(line + QStringLiteral("]"));
    }
};

QAbstractItemModelTester::FailureReportingMode constexpr kQtTest = QAbstractItemModelTester::FailureReportingMode::QtTest;

template <class M>
QString rowKeys(const M & model)
{
    QString out;
    for (int r = 0; r < model.rowCount(); ++r)
        out += model.keyAt(r);
    return out;
}

template <class M, class C>
void verifyRowsMatch(const M & model, const C & container)
{
    QCOMPARE(model.rowCount(), int(container.size()));
    for (int r = 0; r < model.rowCount(); ++r)
        QCOMPARE(model.data(model.index(r, 0), Qt::DisplayRole).template value<int>(), container[r]);
}

using StringIntMap = QMap<QString, int>;
using LetterContainer = obs::Container<StringIntMap>;

// keys "a".."h" with value 0
StringIntMap letters(const QString & keys)
{
    StringIntMap m;
    for (const QChar ch : keys)
        m.insert(QString(ch), 0);
    return m;
}


// Removes "b", "c" and "e" from "abcdef" (value: the letter's code) in one batch and reads every row a receiver of
// rowsAboutToBeRemoved is told about.
template <class C>
bool readsEveryDoomedRow(bool take)
{
    C content;
    for (const QChar ch : QStringLiteral("abcdef"))
        content.insert(QString(ch), ch.unicode());
    obs::Container<C> c(content);
    TestModel<C> model(c);
    QAbstractItemModelTester tester(&model, kQtTest);
    QList<int> read;
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &model, [&](const QModelIndex &, int first, int last) {
        for (int row = first; row <= last; ++row)
            read.append(model.data(model.index(row, 0), Qt::DisplayRole).toInt());
    });
    const QList<QString> doomed{"b", "c", "e"};
    const qsizetype removed = take ? c.takeMany(doomed).size() : c.removeMany(doomed);
    std::sort(read.begin(), read.end());
    return removed == 3 && read == QList<int>{'b', 'c', 'e'} && model.rowCount() == 3;
}
}  // namespace

class tst_list_model : public QObject {
    Q_OBJECT
private slots:
    void at_4_1_sequentialSourceKeepsRowsInStep()
    {
        obs::Container<QList<int>> c;
        TestModel<QList<int>> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);

        c.append(1);
        verifyRowsMatch(model, c.data());
        c.insert(0, 5);
        verifyRowsMatch(model, c.data());
        c.append(2);
        c.remove(1);
        verifyRowsMatch(model, c.data());
        c.replace(0, 9);
        verifyRowsMatch(model, c.data());
        c.editValue(1, [](int & v) { v = 4; });
        verifyRowsMatch(model, c.data());
        c.clear();
        verifyRowsMatch(model, c.data());
        c.resetWith(QList<int>{7, 8, 9});
        verifyRowsMatch(model, c.data());
        QCOMPARE(model.keyAt(2), qsizetype{2});
        QCOMPARE(model.rowOf(1), 1);
        QCOMPARE(model.rowOf(3), -1);
    }

    void at_4_2_associativeSourceSortsByKey()
    {
        LetterContainer c(StringIntMap{{"c", 3}, {"a", 1}});
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);

        QCOMPARE(rowKeys(model), QStringLiteral("ac"));
        QVERIFY(c.insert("b", 2));
        QCOMPARE(rec.log, (Log{"rowsAboutToBeInserted 1 1", "rowsInserted 1 1"}));
        QCOMPARE(rowKeys(model), QStringLiteral("abc"));
        QCOMPARE(model.rowOf("b"), 1);
        QCOMPARE(model.data(model.index(1, 0)).toInt(), 2);
    }

    void at_4_3_dataChangedCarriesTheRoles()
    {
        LetterContainer c(StringIntMap{{"a", 1}, {"b", 2}});
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);
        const int role = Qt::UserRole + 1;

        QVERIFY(c.editValue("b", [](int & v) { v = 20; }, QList<int>{role}));
        QCOMPARE(rec.log, (Log{"dataChanged 1 1 {" + QString::number(role) + "}"}));
        rec.log.clear();
        QVERIFY(c.replace("b", 30));
        QCOMPARE(rec.log, (Log{"dataChanged 1 1 {}"}));
        QCOMPARE(model.data(model.index(1, 0)).toInt(), 30);
    }

    void at_4_4_insertManyIsOneCyclePerRun()
    {
        LetterContainer c(letters("aceg"));
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);

        QCOMPARE(c.insertMany(letters("bdh")), 3);
        QCOMPARE(rec.log, (Log{"rowsAboutToBeInserted 1 1", "rowsInserted 1 1", "rowsAboutToBeInserted 3 3",
                               "rowsInserted 3 3", "rowsAboutToBeInserted 6 6", "rowsInserted 6 6"}));
        QCOMPARE(rowKeys(model), QStringLiteral("abcdegh"));
    }

    void at_4_5_adjacentNewKeysShareOneCycle()
    {
        LetterContainer c(letters("ae"));
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);

        QCOMPARE(c.insertMany(letters("bcd")), 3);
        QCOMPARE(rec.log, (Log{"rowsAboutToBeInserted 1 3", "rowsInserted 1 3"}));
        QCOMPARE(rowKeys(model), QStringLiteral("abcde"));
    }

    void at_4_6_removeManyIsOneCyclePerRunFromTheBottom()
    {
        LetterContainer c(letters("abcdefgh"));
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);

        QCOMPARE(c.removeMany(QList<QString>{"b", "c", "f", "h"}), 4);
        QCOMPARE(rec.log, (Log{"rowsAboutToBeRemoved 7 7", "rowsRemoved 7 7", "rowsAboutToBeRemoved 5 5",
                               "rowsRemoved 5 5", "rowsAboutToBeRemoved 1 2", "rowsRemoved 1 2"}));
        QCOMPARE(rowKeys(model), QStringLiteral("adeg"));
    }

    void at_4_7_clearAndResetWithAreOneReset()
    {
        LetterContainer c(letters("ab"));
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);

        c.clear();
        QCOMPARE(rec.log, (Log{"modelAboutToBeReset rows=2", "modelReset rows=0"}));
        QCOMPARE(model.rowCount(), 0);

        c.resetWith(letters("xy"));
        rec.log.clear();
        c.resetWith(letters("pqr"));
        QCOMPARE(rec.log, (Log{"modelAboutToBeReset rows=2", "modelReset rows=3"}));
        QCOMPARE(rowKeys(model), QStringLiteral("pqr"));
    }

    // Sequence T2: the source announces first, the model translates on the twin.
    void extra_t2_sourceAndModelOrderInSeparateLists()
    {
        LetterContainer c(letters("aceg"));
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder modelRec(&model);
        SourceRecorder<QString> sourceRec(&c);

        c.insertMany(letters("bdh"));
        QCOMPARE(sourceRec.log, (Log{"entriesAboutToBeAdded [b,d,h]", "entriesAdded [b,d,h]"}));
        QCOMPARE(modelRec.log.size(), 6);
    }

    // Sequence T1.
    void extra_t1_singleInsertTranslatedOnAboutAndTwin()
    {
        obs::Container<QList<int>> c(QList<int>{10, 20, 30});
        TestModel<QList<int>> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        ModelRecorder rec(&model);
        QObject ctx;
        Log seenByModelAtAbout;
        connect(&c, &CS::entryAboutToBeAdded, &ctx, [&] { seenByModelAtAbout.append(QString::number(model.rowCount())); });
        connect(&c, &CS::entryAdded, &ctx, [&] { seenByModelAtAbout.append(QString::number(model.rowCount())); });
        c.insert(1, 15);
        QCOMPARE(rec.log, (Log{"rowsAboutToBeInserted 1 1", "rowsInserted 1 1"}));
        QCOMPARE(model.rowCount(), 4);
    }

    // The model reads data() through the key, so an absent key gives an invalid QVariant.
    void extra_removalDoesNotLeaveGhostRows()
    {
        LetterContainer c(letters("abc"));
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        QVERIFY(c.remove("b"));
        QCOMPARE(rowKeys(model), QStringLiteral("ac"));
        QVERIFY(c.take("a").has_value());
        QCOMPARE(rowKeys(model), QStringLiteral("c"));
    }

    // 8.3 as the spec's author meant it: a batch removal is announced while the container still holds the entries, so a
    // view reads every row it is losing, in a map and in a hash, through removeMany and through takeMany.
    void at_4_8_aBatchRemovalLetsAViewReadTheRowsItLoses()
    {
        QVERIFY(readsEveryDoomedRow<StringIntMap>(false));
        QVERIFY(readsEveryDoomedRow<StringIntMap>(true));
        QVERIFY((readsEveryDoomedRow<QHash<QString, int>>(false)));
        QVERIFY((readsEveryDoomedRow<QHash<QString, int>>(true)));
    }

    // Added after mutation testing (a mutant that left the key-to-row map fresh after an insert survived): a lookup after inserts
    // before a key finds the key's new row.
    void at_4_9_theRowMapFollowsInsertions()
    {
        LetterContainer c(letters("ace"));
        TestModel<StringIntMap> model(c);
        QAbstractItemModelTester tester(&model, kQtTest);
        QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);
        QVERIFY(c.replace("e", 1));  // builds the row map: "e" is row 2
        QCOMPARE(spy.takeLast().at(0).toModelIndex().row(), 2);
        QVERIFY(c.insert("b", 0));
        QVERIFY(c.insert("d", 0));
        QVERIFY(c.replace("e", 2));  // "e" is row 4 now
        QCOMPARE(spy.takeLast().at(0).toModelIndex().row(), 4);
        QCOMPARE(model.rowOf("e"), 4);
    }
};

QTEST_GUILESS_MAIN(tst_list_model)
#include "tst_list_model.moc"
