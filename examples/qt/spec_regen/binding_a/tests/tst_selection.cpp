#include "color_selection_models.h"
#include "sel/helpers.h"
#include "sel/tracker.h"

#include <QAbstractItemModelTester>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QtTest>

#include <array>
#include <optional>
#include <vector>

namespace {

using Log = QList<QString>;

constexpr std::array<int, 2> kRoles{Qt::UserRole + 1, Qt::UserRole + 2};
using MultiTracker = sel::Tracker<sel::Mode::Multi, kRoles>;
using SingleTracker = sel::Tracker<sel::Mode::Single, kRoles>;

QAbstractItemModelTester::FailureReportingMode constexpr kQtTest = QAbstractItemModelTester::FailureReportingMode::QtTest;

void expectDelta(const sel::Delta & d, bool presenceFlipped, bool selectionChanged, bool countChanged)
{
    QCOMPARE(d.presenceFlipped, presenceFlipped);
    QCOMPARE(d.selectionChanged, selectionChanged);
    QCOMPARE(d.countChanged, countChanged);
}

bool sameRows(const std::vector<int> & a, std::initializer_list<int> b)
{
    return a == std::vector<int>(b);
}

// A container type whose copies are counted (AT-7.11).
struct CountingVector : std::vector<QString> {
    using std::vector<QString>::vector;
    CountingVector(const CountingVector & other) : std::vector<QString>(other) { ++copies; }
    CountingVector(CountingVector &&) = default;
    CountingVector & operator=(const CountingVector &) = default;
    CountingVector & operator=(CountingVector &&) = default;
    ~CountingVector() = default;
    static inline int copies = 0;
};

struct Person {
    QString name;
    std::optional<QString> nick;
    QString shout() const { return name.toUpper(); }
};

constexpr auto upperCase = [](const QString & s) { return s.toUpper(); };

QList<ColorItem> items(int count)
{
    QList<ColorItem> out;
    for (int i = 0; i < count; ++i)
        out.append(ColorItem{QStringLiteral("c") + QString::number(i), QStringLiteral("#00000") + QString::number(i)});
    return out;
}

// Records every signal of a hosting model: structure, data and host notifications.
template <class M>
class HostRecorder : public QObject {
public:
    Log log;

    explicit HostRecorder(M * m)
    {
        using Q = QAbstractItemModel;
        connect(m, &Q::rowsAboutToBeInserted, this, [this](const QModelIndex &, int f, int l) { add("rowsAboutToBeInserted", f, l); });
        connect(m, &Q::rowsInserted, this, [this](const QModelIndex &, int f, int l) { add("rowsInserted", f, l); });
        connect(m, &Q::rowsAboutToBeRemoved, this, [this](const QModelIndex &, int f, int l) { add("rowsAboutToBeRemoved", f, l); });
        connect(m, &Q::rowsRemoved, this, [this](const QModelIndex &, int f, int l) { add("rowsRemoved", f, l); });
        connect(m, &Q::modelAboutToBeReset, this, [this] { log.append(QStringLiteral("modelAboutToBeReset")); });
        connect(m, &Q::modelReset, this, [this] { log.append(QStringLiteral("modelReset")); });
        connect(m, &Q::dataChanged, this, [this](const QModelIndex & a, const QModelIndex & b, const QList<int> & roles) {
            QString line = QStringLiteral("dataChanged ") + QString::number(a.row()) + QStringLiteral(" ")
                           + QString::number(b.row()) + QStringLiteral(" {");
            for (qsizetype i = 0; i < roles.size(); ++i)
                line += (i ? QStringLiteral(",") : QString()) + QString::number(roles[i]);
            log.append(line + QStringLiteral("}"));
        });
        connect(m, &M::hasSelectionChanged, this, [this] { log.append(QStringLiteral("hasSelectionChanged")); });
        if constexpr (sel::SingleHost<M>)
            connect(m, &M::selectedRowChanged, this, [this] { log.append(QStringLiteral("selectedRowChanged")); });
        else
            connect(m, &M::selectedCountChanged, this, [this] { log.append(QStringLiteral("selectedCountChanged")); });
    }

    int count(const QString & line) const { return int(log.count(line)); }

private:
    void add(const char * name, int first, int last)
    {
        log.append(QString::fromLatin1(name) + QStringLiteral(" ") + QString::number(first) + QStringLiteral(" ")
                   + QString::number(last));
    }
};

template <class M>
bool select(M & model, int row, bool on = true)
{
    return model.setData(model.index(row), on, M::SelectedRole);
}

template <class M>
bool rowSelected(const M & model, int row)
{
    return model.data(model.index(row), M::SelectedRole).toBool();
}

template <class M>
QString dataChangedLine(int first, int last)
{
    return QStringLiteral("dataChanged ") + QString::number(first) + QStringLiteral(" ") + QString::number(last)
           + QStringLiteral(" {") + QString::number(M::SelectedRole) + QStringLiteral("}");
}

}  // namespace

class tst_selection : public QObject {
    Q_OBJECT
private slots:
    void at_7_1_multiSetReturnsTheDelta()
    {
        MultiTracker t;
        t.reset(3);
        expectDelta(t.set(1, true), true, true, true);
        expectDelta(t.set(2, true), false, true, true);
        QVERIFY(sameRows(t.selectedRows(), {1, 2}));
        QCOMPARE(t.count(), 2);
    }

    void at_7_2_multiSetToTheCurrentValueChangesNothing()
    {
        MultiTracker t;
        t.reset(3);
        expectDelta(t.set(0, false), false, false, false);
        t.set(1, true);
        expectDelta(t.set(1, true), false, false, false);
        QCOMPARE(t.count(), 1);
    }

    void at_7_3_multiRowInserted()
    {
        MultiTracker t;
        t.reset(3);
        t.set(2, true);
        t.rowInserted(0);
        QVERIFY(t.isSelected(3));
        QVERIFY(!t.isSelected(2));
        t.rowInserted(4);
        QVERIFY(t.isSelected(3));
        QCOMPARE(t.rowCount(), 5);
    }

    void at_7_4_multiRowRemoved()
    {
        MultiTracker t;
        t.reset(3);
        t.set(0, true);
        t.set(2, true);
        expectDelta(t.rowRemoved(1), false, false, false);
        QVERIFY(sameRows(t.selectedRows(), {0, 1}));
        expectDelta(t.rowRemoved(1), false, true, true);
        expectDelta(t.rowRemoved(0), true, true, true);
        QVERIFY(!t.hasSelection());
    }

    void at_7_5_multiClearKeepsTheRowCount()
    {
        MultiTracker t;
        t.reset(3);
        t.set(1, true);
        t.clear();
        QCOMPARE(t.rowCount(), 3);
        for (int r = 0; r < 3; ++r)
            QVERIFY(!t.isSelected(r));
        expectDelta(t.set(2, true), true, true, true);
    }

    void at_7_6_singleSet()
    {
        SingleTracker t;
        expectDelta(t.set(2), true, true, true);
        expectDelta(t.set(5), false, true, false);
        expectDelta(t.set(std::nullopt), true, true, true);
        expectDelta(t.set(std::nullopt), false, false, false);
    }

    void at_7_7_singleCount()
    {
        SingleTracker t;
        QCOMPARE(t.count(), 0);
        t.set(1);
        QCOMPARE(t.count(), 1);
        t.set(4);
        QCOMPARE(t.count(), 1);
        t.clear();
        QCOMPARE(t.count(), 0);
    }

    void at_7_8_singleRowInserted()
    {
        SingleTracker t;
        t.set(2);
        t.rowInserted(0);
        QCOMPARE(t.selectedRow(), 3);
        t.rowInserted(3);
        QCOMPARE(t.selectedRow(), 4);
        t.rowInserted(9);
        QCOMPARE(t.selectedRow(), 4);
    }

    void at_7_9_singleRowRemoved()
    {
        SingleTracker t;
        t.set(3);
        expectDelta(t.rowRemoved(1), false, true, false);
        QCOMPARE(t.selectedRow(), 2);
        expectDelta(t.rowRemoved(7), false, false, false);
        QCOMPARE(t.selectedRow(), 2);
        expectDelta(t.rowRemoved(2), true, true, true);
        QVERIFY(!t.hasSelection());
    }

    void at_7_10_appendRoleNames()
    {
        QHash<int, QByteArray> names{{Qt::UserRole + 1, "value"}, {Qt::UserRole + 2, "count"}};
        SingleTracker::appendRoleNames(names);
        QCOMPARE(names.size(), 3);
        QCOMPARE(names.value(SingleTracker::selectedRole()), QByteArray("isSelected"));
        QCOMPARE(QByteArray(SingleTracker::selectedRoleName()), QByteArray("isSelected"));
    }

    void at_7_11_selectedValueByReferenceAndPointer()
    {
        SingleTracker t;
        CountingVector names{"red", "green", "blue"};
        CountingVector::copies = 0;

        QCOMPARE(sel::selectedValue<upperCase>(t, names), QString());
        QCOMPARE(sel::selectedValue<upperCase>(t, static_cast<const CountingVector *>(nullptr)), QString());

        t.set(1);
        QCOMPARE(sel::selectedValue<upperCase>(t, names), QStringLiteral("GREEN"));
        QCOMPARE(sel::selectedValue<upperCase>(t, &names), QStringLiteral("GREEN"));
        QCOMPARE(CountingVector::copies, 0);
    }

    void extra_selectedValueGetterShapes()
    {
        SingleTracker t;
        t.set(1);
        const QList<Person> people{{"ann", std::nullopt}, {"bob", QStringLiteral("bobby")}};
        QCOMPARE(sel::selectedValue<&Person::name>(t, people), QStringLiteral("bob"));
        QCOMPARE(sel::selectedValue<&Person::shout>(t, people), QStringLiteral("BOB"));
        QCOMPARE(sel::selectedValue<&Person::nick>(t, people), QStringLiteral("bobby"));  // optional unwrapped
        t.set(0);
        QCOMPARE(sel::selectedValue<&Person::nick>(t, people), QString());  // empty optional gives X{}
    }

    void at_8_1_hostsExposeTheSelectedRole()
    {
        ColorSingleSelectionModel single;
        ColorMultiSelectionModel multi;
        QCOMPARE(single.roleNames().value(ColorSingleSelectionModel::SelectedRole), QByteArray("isSelected"));
        QCOMPARE(multi.roleNames().value(ColorMultiSelectionModel::SelectedRole), QByteArray("isSelected"));
        QCOMPARE(single.roleNames().value(ColorListModel::NameRole), QByteArray("name"));
        QCOMPARE(int(ColorSingleSelectionModel::SelectedRole), int(ColorListModel::HexRole) + 1);
        QCOMPARE(int(ColorMultiSelectionModel::SelectedRole), int(ColorListModel::HexRole) + 1);
    }

    void at_8_2_singleSelectionNotifications()
    {
        using M = ColorSingleSelectionModel;
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(6));
        HostRecorder<M> rec(&model);

        QVERIFY(select(model, 3));
        QCOMPARE(rec.count(dataChangedLine<M>(3, 3)), 1);
        QCOMPARE(rec.count(QStringLiteral("selectedRowChanged")), 1);
        QCOMPARE(rec.count(QStringLiteral("hasSelectionChanged")), 1);

        rec.log.clear();  // T8
        QVERIFY(select(model, 5));
        QCOMPARE(rec.log, (Log{"selectedRowChanged", dataChangedLine<M>(3, 3), dataChangedLine<M>(5, 5)}));

        rec.log.clear();
        QVERIFY(select(model, 5));
        QVERIFY(rec.log.isEmpty());
    }

    void at_8_3_deselectingAnotherRowChangesNothing()
    {
        using M = ColorSingleSelectionModel;
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(6));
        select(model, 3);
        HostRecorder<M> rec(&model);

        QVERIFY(select(model, 5, false));  // T9
        QVERIFY(rec.log.isEmpty());
        QVERIFY(rowSelected(model, 3));
        QCOMPARE(model.selectedName(), QStringLiteral("c3"));

        QVERIFY(select(model, 3, false));
        QCOMPARE(rec.log, (Log{"selectedRowChanged", dataChangedLine<M>(3, 3), "hasSelectionChanged"}));
        QVERIFY(!model.hasSelection());
        QCOMPARE(model.selectedName(), QString());
    }

    void at_8_4_selectionFollowsInsertionsAndRemovals()
    {
        using M = ColorSingleSelectionModel;
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(5));
        select(model, 2);
        HostRecorder<M> rec(&model);

        model.insertItem(0, ColorItem{"new", "#ffffff"});  // T14
        QCOMPARE(rec.log, (Log{"rowsAboutToBeInserted 0 0", "rowsInserted 0 0", "selectedRowChanged"}));
        QVERIFY(rowSelected(model, 3));
        QCOMPARE(model.selectedName(), QStringLiteral("c2"));

        rec.log.clear();  // T13 on the new selected row
        model.removeItem(3);
        QCOMPARE(rec.log, (Log{"rowsAboutToBeRemoved 3 3", "rowsRemoved 3 3", "selectedRowChanged", "hasSelectionChanged"}));
        for (int r = 0; r < model.rowCount(); ++r)
            QVERIFY(!rowSelected(model, r));
    }

    void at_8_5_clearSelection()
    {
        using M = ColorSingleSelectionModel;
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(4));
        select(model, 1);
        HostRecorder<M> rec(&model);

        model.clearSelection();  // T10
        QCOMPARE(rec.log, (Log{dataChangedLine<M>(1, 1), "hasSelectionChanged", "selectedRowChanged"}));
        rec.log.clear();
        model.clearSelection();
        QVERIFY(rec.log.isEmpty());
    }

    void at_8_6_multiClearNotifiesOneRangePerRun()
    {
        using M = ColorMultiSelectionModel;
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(10));
        for (const int row : {8, 0, 2, 1, 5, 7})
            QVERIFY(select(model, row));
        HostRecorder<M> rec(&model);

        model.clearSelection();  // T11
        QCOMPARE(rec.log, (Log{dataChangedLine<M>(0, 2), dataChangedLine<M>(5, 5), dataChangedLine<M>(7, 8),
                               "hasSelectionChanged", "selectedCountChanged"}));
        QCOMPARE(model.selectedCount(), 0);
    }

    void at_8_7_multiNotificationsFollowTheDelta()
    {
        using M = ColorMultiSelectionModel;
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(3));
        HostRecorder<M> rec(&model);

        select(model, 1, true);
        QCOMPARE(rec.log, (Log{dataChangedLine<M>(1, 1), "selectedCountChanged", "hasSelectionChanged"}));  // T12
        select(model, 1, true);
        select(model, 2, true);
        select(model, 0, false);
        int dataChanges = 0;
        for (const QString & line : rec.log)
            dataChanges += line.startsWith("dataChanged") ? 1 : 0;
        QCOMPARE(dataChanges, 2);
        QCOMPARE(rec.count(QStringLiteral("selectedCountChanged")), 2);
        QCOMPARE(rec.count(QStringLiteral("hasSelectionChanged")), 1);
    }

    void at_8_8_multiSelectionFollowsRemovals()
    {
        using M = ColorMultiSelectionModel;
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(4));
        select(model, 1);
        select(model, 3);
        HostRecorder<M> rec(&model);

        model.removeItem(0);
        QCOMPARE(rec.log, (Log{"rowsAboutToBeRemoved 0 0", "rowsRemoved 0 0"}));
        QVERIFY(rowSelected(model, 0));
        QVERIFY(rowSelected(model, 2));

        rec.log.clear();
        model.removeItem(0);
        QCOMPARE(rec.count(QStringLiteral("selectedCountChanged")), 1);
        QCOMPARE(rec.count(QStringLiteral("hasSelectionChanged")), 0);

        model.removeItem(1);
        QCOMPARE(rec.count(QStringLiteral("selectedCountChanged")), 2);
        QCOMPARE(rec.count(QStringLiteral("hasSelectionChanged")), 1);
        QVERIFY(!model.hasSelection());
        // the aggregate notifications come after the end of the removal cycle
        QVERIFY(rec.log.indexOf("rowsRemoved 1 1") < rec.log.lastIndexOf("selectedCountChanged"));
    }

    void at_8_9_resetDropsTheSelection()
    {
        using M = ColorMultiSelectionModel;
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(3));
        select(model, 2);
        HostRecorder<M> rec(&model);

        model.resetWith(items(5));
        QCOMPARE(model.rowCount(), 5);
        QCOMPARE(model.selectedCount(), 0);
        QCOMPARE(rec.count(QStringLiteral("selectedCountChanged")), 1);
        QCOMPARE(rec.count(QStringLiteral("hasSelectionChanged")), 1);
        QVERIFY(rec.log.indexOf("selectedCountChanged") < rec.log.indexOf("hasSelectionChanged"));
        QVERIFY(rec.log.indexOf("modelReset") < rec.log.indexOf("selectedCountChanged"));
        QVERIFY(select(model, 4));
        QCOMPARE(model.selectedCount(), 1);
    }

    void at_8_10_singleModelPassesTheTester()
    {
        exerciseTheTester<ColorSingleSelectionModel>();
    }

    void at_8_10_multiModelPassesTheTester()
    {
        exerciseTheTester<ColorMultiSelectionModel>();
    }

    // A reset without a selection announces no host notification.
    void extra_resetWithoutSelectionIsSilent()
    {
        using M = ColorSingleSelectionModel;
        M model;
        model.resetWith(items(3));
        HostRecorder<M> rec(&model);
        model.resetWith(items(2));
        QCOMPARE(rec.log, (Log{"modelAboutToBeReset", "modelReset"}));
    }

    // A selection lower than the insertion point does not move, so nothing is announced.
    void extra_insertionAfterTheSelectionIsSilent()
    {
        using M = ColorSingleSelectionModel;
        M model;
        model.resetWith(items(4));
        select(model, 1);
        HostRecorder<M> rec(&model);
        model.insertItem(3, ColorItem{"x", "#000000"});
        QCOMPARE(rec.log, (Log{"rowsAboutToBeInserted 3 3", "rowsInserted 3 3"}));
        QVERIFY(rowSelected(model, 1));
    }

    void extra_setDataRejectsOtherRolesAndInvalidIndexes()
    {
        ColorSingleSelectionModel model;
        model.resetWith(items(2));
        QVERIFY(!model.setData(model.index(0), true, Qt::EditRole));
        QVERIFY(!model.setData(QModelIndex(), true, ColorSingleSelectionModel::SelectedRole));
        QVERIFY(model.flags(model.index(0)) & Qt::ItemIsEditable);
        QVERIFY(!(model.flags(QModelIndex()) & Qt::ItemIsEditable));
    }

private:
    template <class M>
    void exerciseTheTester()
    {
        M model;
        QAbstractItemModelTester tester(&model, kQtTest);
        model.resetWith(items(5));
        select(model, 1);
        model.insertItem(0, ColorItem{"x", "#111111"});
        model.removeItem(2);
        select(model, 2);
        model.clearSelection();
        model.resetWith(items(2));
        QCOMPARE(model.rowCount(), 2);
    }
};

QTEST_GUILESS_MAIN(tst_selection)
#include "tst_selection.moc"
