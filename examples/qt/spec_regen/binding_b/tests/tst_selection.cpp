// SPEC 12.7 and 12.8: the selection tracker and the example hosting models, binding B (the hosts declare their
// notifications as reflect-moc data-member signals, so no moc runs).
#include <color_selection_models.h>

#include <sel/helpers.h>
#include <sel/tracker.h>

#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <reflect_moc/qt/qt.hpp>

#include <array>
#include <vector>

namespace {

constexpr auto kTesterMode = QAbstractItemModelTester::FailureReportingMode::QtTest;

constexpr int kRole1 = Qt::UserRole + 1;
constexpr int kRole2 = Qt::UserRole + 2;
constexpr std::array<int, 2> kRoles{kRole1, kRole2};

using SingleTracker = sel::Tracker<sel::Mode::Single, kRoles>;
using MultiTracker = sel::Tracker<sel::Mode::Multi, kRoles>;

bool is(const sel::Delta & d, bool presenceFlipped, bool selectionChanged, bool countChanged)
{
    return d.presenceFlipped == presenceFlipped && d.selectionChanged == selectionChanged && d.countChanged == countChanged;
}

// a container type derived from std::vector<QString> whose copy constructor counts
struct CountingVector : std::vector<QString> {
    static inline int copies = 0;

    CountingVector(std::initializer_list<QString> init) : std::vector<QString>(init) {}
    CountingVector(const CountingVector & other) : std::vector<QString>(other) { ++copies; }
    CountingVector(CountingVector &&) = default;
};

QList<ColorItem> colors(int n)
{
    QList<ColorItem> items;
    for (int i = 0; i < n; ++i)
        items.append(ColorItem{.name = QStringLiteral("color%1").arg(i), .hex = QStringLiteral("#%1").arg(i, 6, 16, QLatin1Char('0'))});
    return items;
}

// One object connected to every notification of a host: the model's standard signals and the host's own.
template <class Model>
class HostLog {
public:
    explicit HostLog(Model & m)
    {
        QObject::connect(&m, &QAbstractItemModel::dataChanged, &context, [this](const QModelIndex & a, const QModelIndex & b, const QList<int> & roles) {
            QStringList r;
            for (int role : roles)
                r << (role == Model::SelectedRole ? QStringLiteral("S") : QString::number(role));
            log << QStringLiteral("dataChanged(%1,%2,{%3})").arg(a.row()).arg(b.row()).arg(r.join(QLatin1Char(',')));
        });
        QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeInserted, &context, [this](const QModelIndex &, int f, int l) { log << QStringLiteral("rowsAboutToBeInserted(%1,%2)").arg(f).arg(l); });
        QObject::connect(&m, &QAbstractItemModel::rowsInserted, &context, [this](const QModelIndex &, int f, int l) { log << QStringLiteral("rowsInserted(%1,%2)").arg(f).arg(l); });
        QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeRemoved, &context, [this](const QModelIndex &, int f, int l) { log << QStringLiteral("rowsAboutToBeRemoved(%1,%2)").arg(f).arg(l); });
        QObject::connect(&m, &QAbstractItemModel::rowsRemoved, &context, [this](const QModelIndex &, int f, int l) { log << QStringLiteral("rowsRemoved(%1,%2)").arg(f).arg(l); });
        QObject::connect(&m, &QAbstractItemModel::modelAboutToBeReset, &context, [this] { log << QStringLiteral("modelAboutToBeReset"); });
        QObject::connect(&m, &QAbstractItemModel::modelReset, &context, [this] { log << QStringLiteral("modelReset"); });
        QObject::connect(&m, &Model::hasSelectionChanged, &context, [this] { log << QStringLiteral("hasSelectionChanged"); });
        if constexpr (requires { &Model::selectedRowChanged; })
            QObject::connect(&m, &Model::selectedRowChanged, &context, [this] { log << QStringLiteral("selectedRowChanged"); });
        if constexpr (requires { &Model::selectedCountChanged; })
            QObject::connect(&m, &Model::selectedCountChanged, &context, [this] { log << QStringLiteral("selectedCountChanged"); });
    }

    int count(const QString & prefix) const
    {
        int n = 0;
        for (const QString & line : log)
            n += line.startsWith(prefix) ? 1 : 0;
        return n;
    }

    QObject context;
    QStringList log;
};

template <class Model>
bool setSelected(Model & model, int row, bool selected)
{
    return model.setData(model.index(row), selected, Model::SelectedRole);
}

template <class Model>
bool isRowSelected(const Model & model, int row)
{
    return model.data(model.index(row), Model::SelectedRole).toBool();
}

template <class Model>
QList<int> selectedRowsOf(const Model & model)
{
    QList<int> rows;
    for (int r = 0; r < model.rowCount(); ++r)
        if (isRowSelected(model, r))
            rows << r;
    return rows;
}

}  // namespace

class TstSelection : public QObject {
    RQT_OBJECT

private slots:
    [[= rqt::slot]] void at_7_1_multiDeltasOfTheFirstAndSecondTick()
    {
        MultiTracker t;
        t.reset(3);
        QVERIFY(is(t.set(1, true), true, true, true));
        QVERIFY(is(t.set(2, true), false, true, true));
        QCOMPARE(t.selectedRows(), (std::vector<int>{1, 2}));
        QCOMPARE(t.count(), 2);
    }

    [[= rqt::slot]] void at_7_2_multiSetToTheCurrentValueChangesNothing()
    {
        MultiTracker t;
        t.reset(3);
        QVERIFY(is(t.set(0, false), false, false, false));
        t.set(0, true);
        QVERIFY(is(t.set(0, true), false, false, false));
        QCOMPARE(t.count(), 1);
    }

    [[= rqt::slot]] void at_7_3_multiInsertionShiftsLaterRows()
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

    [[= rqt::slot]] void at_7_4_multiRemovalDeltas()
    {
        MultiTracker t;
        t.reset(3);
        t.set(0, true);
        t.set(2, true);
        QVERIFY(is(t.rowRemoved(1), false, false, false));
        QVERIFY(t.isSelected(0));
        QVERIFY(t.isSelected(1));
        QVERIFY(is(t.rowRemoved(1), false, true, true));
        QVERIFY(is(t.rowRemoved(0), true, true, true));
        QVERIFY(!t.hasSelection());
        QCOMPARE(t.rowCount(), 0);
    }

    [[= rqt::slot]] void at_7_5_multiClearKeepsTheRowCount()
    {
        MultiTracker t;
        t.reset(3);
        t.set(0, true);
        t.set(1, true);
        t.clear();
        QCOMPARE(t.rowCount(), 3);
        for (int r = 0; r < 3; ++r)
            QVERIFY(!t.isSelected(r));
        QVERIFY(is(t.set(2, true), true, true, true));
    }

    [[= rqt::slot]] void at_7_6_singleSetDeltas()
    {
        SingleTracker t;
        QVERIFY(is(t.set(2), true, true, true));
        QVERIFY(is(t.set(5), false, true, false));
        QVERIFY(is(t.set(std::nullopt), true, true, true));
        QVERIFY(is(t.set(std::nullopt), false, false, false));
    }

    [[= rqt::slot]] void at_7_7_singleCountIsZeroOrOne()
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

    [[= rqt::slot]] void at_7_8_singleInsertionOnOrBeforeTheSelectedRowPushesItDown()
    {
        SingleTracker t;
        t.set(2);
        t.rowInserted(0);
        QCOMPARE(t.selectedRow(), 3);
        t.rowInserted(3);  // exactly at the selected index
        QCOMPARE(t.selectedRow(), 4);
        t.rowInserted(9);
        QCOMPARE(t.selectedRow(), 4);
    }

    [[= rqt::slot]] void at_7_9_singleRemovalDeltas()
    {
        SingleTracker t;
        t.set(3);
        QVERIFY(is(t.rowRemoved(1), false, true, false));
        QCOMPARE(t.selectedRow(), 2);
        QVERIFY(is(t.rowRemoved(7), false, false, false));
        QCOMPARE(t.selectedRow(), 2);
        QVERIFY(is(t.rowRemoved(2), true, true, true));
        QVERIFY(!t.hasSelection());
        QVERIFY(is(t.rowRemoved(0), false, false, false));
    }

    [[= rqt::slot]] void at_7_10_appendRoleNames()
    {
        QHash<int, QByteArray> names{{kRole1, "value"}, {kRole2, "count"}};
        SingleTracker::appendRoleNames(names);
        QCOMPARE(names.size(), 3);
        QCOMPARE(names.value(SingleTracker::selectedRole()), QByteArray("isSelected"));
        QCOMPARE(names.value(kRole1), QByteArray("value"));
        QCOMPARE(QByteArray(SingleTracker::selectedRoleName()), QByteArray("isSelected"));
    }

    [[= rqt::slot]] void at_7_11_selectedValueBorrowsTheContainer()
    {
        constexpr auto upper = [](const QString & s) { return s.toUpper(); };
        SingleTracker t;
        const CountingVector names{"red", "green", "blue"};
        CountingVector::copies = 0;

        QCOMPARE(sel::selectedValue<upper>(t, names), QString());
        QCOMPARE(sel::selectedValue<upper>(t, static_cast<const CountingVector *>(nullptr)), QString());  // not read
        t.set(1);
        QCOMPARE(sel::selectedValue<upper>(t, names), QString("GREEN"));
        QCOMPARE(sel::selectedValue<upper>(t, &names), QString("GREEN"));
        CountingVector mutableNames{"x", "y"};
        t.set(0);
        QCOMPARE(sel::selectedValue<upper>(t, &mutableNames), QString("X"));  // a non-const pointer works too
        QCOMPARE(CountingVector::copies, 0);
    }

    [[= rqt::slot]] void at_7_11_selectedValueGetterForms()
    {
        struct Item {
            QString name;
            std::optional<int> maybe;
            int twice() const { return 2 * static_cast<int>(name.size()); }
        };
        SingleTracker t;
        const std::vector<Item> items{{"ab", std::nullopt}, {"cde", 7}};
        t.set(1);
        QCOMPARE(sel::selectedValue<&Item::name>(t, items), QString("cde"));
        QCOMPARE(sel::selectedValue<&Item::twice>(t, items), 6);
        QCOMPARE(sel::selectedValue<&Item::maybe>(t, items), 7);  // an optional is unwrapped
        t.set(0);
        QCOMPARE(sel::selectedValue<&Item::maybe>(t, items), 0);  // an empty optional gives int{}
        const QList<QString> list{"p", "q"};
        QCOMPARE(sel::selectedValue<[](const QString & s) -> QString { return s + s; }>(t, list), QString("pp"));
    }

    [[= rqt::slot]] void at_8_1_bothModelsNameTheSelectedRole()
    {
        ColorSingleSelectionModel single;
        ColorMultiSelectionModel multi;
        QCOMPARE(single.roleNames().value(ColorSingleSelectionModel::SelectedRole), QByteArray("isSelected"));
        QCOMPARE(multi.roleNames().value(ColorMultiSelectionModel::SelectedRole), QByteArray("isSelected"));
        QCOMPARE(ColorSingleSelectionModel::SelectedRole, static_cast<int>(ColorListModel::HexRole) + 1);
        QCOMPARE(ColorMultiSelectionModel::SelectedRole, static_cast<int>(ColorListModel::HexRole) + 1);
        QCOMPARE(single.roleNames().value(ColorListModel::NameRole), QByteArray("name"));
        QCOMPARE(single.roleNames().value(ColorListModel::HexRole), QByteArray("hex"));
    }

    [[= rqt::slot]] void at_8_2_singleSelectThenSequenceT8()
    {
        ColorSingleSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(6));
        HostLog<ColorSingleSelectionModel> log(model);

        QVERIFY(setSelected(model, 3, true));
        QCOMPARE(log.log, (QStringList{"selectedRowChanged", "dataChanged(3,3,{S})", "hasSelectionChanged"}));
        QCOMPARE(log.count("dataChanged"), 1);

        log.log.clear();  // T8
        QVERIFY(setSelected(model, 5, true));
        QCOMPARE(log.log, (QStringList{"selectedRowChanged", "dataChanged(3,3,{S})", "dataChanged(5,5,{S})"}));

        log.log.clear();
        QVERIFY(setSelected(model, 5, true));
        QVERIFY(log.log.isEmpty());
    }

    [[= rqt::slot]] void at_8_2_theRolesOfDataChangedAreExactlyTheSelectedRole()
    {
        ColorSingleSelectionModel model;
        model.resetWith(colors(3));
        QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);
        setSelected(model, 1, true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(2).value<QList<int>>(), (QList<int>{ColorSingleSelectionModel::SelectedRole}));
    }

    [[= rqt::slot]] void at_8_3_sequenceT9DeselectingAnotherRowChangesNothing()
    {
        ColorSingleSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(6));
        setSelected(model, 3, true);
        const QString name = model.selectedName();
        QCOMPARE(name, QString("color3"));
        HostLog<ColorSingleSelectionModel> log(model);

        QVERIFY(setSelected(model, 5, false));
        QVERIFY(log.log.isEmpty());
        QVERIFY(isRowSelected(model, 3));
        QCOMPARE(model.selectedName(), name);

        QVERIFY(setSelected(model, 3, false));
        QCOMPARE(log.log, (QStringList{"selectedRowChanged", "dataChanged(3,3,{S})", "hasSelectionChanged"}));
        QVERIFY(!model.hasSelection());
    }

    [[= rqt::slot]] void at_8_4_sequenceT14InsertionThenSequenceT13Removal()
    {
        ColorSingleSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(6));
        setSelected(model, 2, true);
        const QString name = model.selectedName();
        HostLog<ColorSingleSelectionModel> log(model);

        model.insertItem(0, ColorItem{.name = "inserted", .hex = "#ffffff"});
        QCOMPARE(log.log, (QStringList{"rowsAboutToBeInserted(0,0)", "rowsInserted(0,0)", "selectedRowChanged"}));
        QVERIFY(isRowSelected(model, 3));
        QCOMPARE(model.selectedName(), name);
        QCOMPARE(selectedRowsOf(model), (QList<int>{3}));

        log.log.clear();  // T13 on the new selected row
        model.removeItem(3);
        QCOMPARE(log.log, (QStringList{"rowsAboutToBeRemoved(3,3)", "rowsRemoved(3,3)", "selectedRowChanged", "hasSelectionChanged"}));
        QVERIFY(selectedRowsOf(model).isEmpty());
        QVERIFY(!model.hasSelection());
        QCOMPARE(model.selectedName(), QString());
    }

    [[= rqt::slot]] void at_8_4_theSelectionAlreadyPointsRightWhenTheViewHearsTheInsertion()
    {
        ColorSingleSelectionModel model;
        model.resetWith(colors(4));
        setSelected(model, 1, true);
        QObject ctx;
        QString nameSeen;
        QObject::connect(&model, &QAbstractItemModel::rowsInserted, &ctx, [&] { nameSeen = model.selectedName(); });
        model.insertItem(0, ColorItem{.name = "new", .hex = "#000000"});
        QCOMPARE(nameSeen, QString("color1"));
    }

    [[= rqt::slot]] void at_8_5_sequenceT10ClearSelection()
    {
        ColorSingleSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(4));
        setSelected(model, 1, true);
        HostLog<ColorSingleSelectionModel> log(model);

        model.clearSelection();
        QCOMPARE(log.log, (QStringList{"dataChanged(1,1,{S})", "hasSelectionChanged", "selectedRowChanged"}));
        QVERIFY(!isRowSelected(model, 1));

        log.log.clear();
        model.clearSelection();
        QVERIFY(log.log.isEmpty());
    }

    [[= rqt::slot]] void at_8_5_clearSelectionIsInvokableThroughQtsMetaObject()
    {
        ColorSingleSelectionModel model;
        model.resetWith(colors(2));
        setSelected(model, 0, true);
        QVERIFY(QMetaObject::invokeMethod(&model, "clearSelection"));
        QVERIFY(!model.hasSelection());
        QCOMPARE(model.property("hasSelection").toBool(), false);
        setSelected(model, 1, true);
        QCOMPARE(model.property("hasSelection").toBool(), true);
        QCOMPARE(model.property("selectedName").toString(), QString("color1"));
    }

    [[= rqt::slot]] void at_8_6_multiSequenceT11ClearSelection()
    {
        ColorMultiSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(10));
        for (int row : {8, 0, 2, 1, 5, 7})
            QVERIFY(setSelected(model, row, true));
        QCOMPARE(model.selectedCount(), 6);
        HostLog<ColorMultiSelectionModel> log(model);

        model.clearSelection();
        QCOMPARE(log.log, (QStringList{"dataChanged(0,2,{S})", "dataChanged(5,5,{S})", "dataChanged(7,8,{S})", "hasSelectionChanged", "selectedCountChanged"}));
        QCOMPARE(log.count("modelReset") + log.count("modelAboutToBeReset"), 0);
        QCOMPARE(model.selectedCount(), 0);
        QVERIFY(selectedRowsOf(model).isEmpty());
    }

    [[= rqt::slot]] void at_8_7_multiTicksAndSequenceT12()
    {
        ColorMultiSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(3));
        HostLog<ColorMultiSelectionModel> log(model);

        setSelected(model, 1, true);  // T12
        QCOMPARE(log.log, (QStringList{"dataChanged(1,1,{S})", "selectedCountChanged", "hasSelectionChanged"}));

        setSelected(model, 1, true);
        setSelected(model, 2, true);
        setSelected(model, 0, false);
        QCOMPARE(log.count("dataChanged"), 2);
        QCOMPARE(log.count("selectedCountChanged"), 2);
        QCOMPARE(log.count("hasSelectionChanged"), 1);
    }

    [[= rqt::slot]] void at_8_8_multiRemovalNotifiesFromTheKeptDelta()
    {
        ColorMultiSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(4));
        setSelected(model, 1, true);
        setSelected(model, 3, true);
        HostLog<ColorMultiSelectionModel> log(model);

        model.removeItem(0);
        QCOMPARE(log.log, (QStringList{"rowsAboutToBeRemoved(0,0)", "rowsRemoved(0,0)"}));
        QCOMPARE(selectedRowsOf(model), (QList<int>{0, 2}));

        log.log.clear();
        model.removeItem(0);
        QCOMPARE(log.count("selectedCountChanged"), 1);
        QCOMPARE(log.count("hasSelectionChanged"), 0);

        log.log.clear();
        model.removeItem(1);
        QCOMPARE(log.count("selectedCountChanged"), 1);
        QCOMPARE(log.count("hasSelectionChanged"), 1);
        QVERIFY(!model.hasSelection());
    }

    [[= rqt::slot]] void at_8_9_multiResetDropsTheSelectionWithOneNotificationEach()
    {
        ColorMultiSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(3));
        setSelected(model, 2, true);
        HostLog<ColorMultiSelectionModel> log(model);

        model.resetWith(colors(5));
        QCOMPARE(model.rowCount(), 5);
        QCOMPARE(model.selectedCount(), 0);
        QCOMPARE(log.log, (QStringList{"modelAboutToBeReset", "modelReset", "selectedCountChanged", "hasSelectionChanged"}));
        QVERIFY(setSelected(model, 4, true));
        QVERIFY(isRowSelected(model, 4));
    }

    [[= rqt::slot]] void at_8_9_singleResetDropsTheSelectionToo()
    {
        ColorSingleSelectionModel model;
        QAbstractItemModelTester tester(&model, kTesterMode);
        model.resetWith(colors(3));
        setSelected(model, 2, true);
        HostLog<ColorSingleSelectionModel> log(model);
        model.resetWith(colors(2));
        QCOMPARE(log.log, (QStringList{"modelAboutToBeReset", "modelReset", "selectedRowChanged", "hasSelectionChanged"}));
        QVERIFY(!model.hasSelection());

        log.log.clear();
        model.resetWith(colors(4));  // nothing was selected: nothing to announce
        QCOMPARE(log.log, (QStringList{"modelAboutToBeReset", "modelReset"}));
    }

    [[= rqt::slot]] void at_8_10_bothModelsPassTheModelTester()
    {
        {
            ColorSingleSelectionModel model;
            QAbstractItemModelTester tester(&model, kTesterMode);
            model.resetWith(colors(5));
            setSelected(model, 3, true);
            model.insertItem(0, ColorItem{.name = "x", .hex = "#1"});
            model.removeItem(2);
            setSelected(model, 1, true);
            model.clearSelection();
            model.resetWith(colors(2));
        }
        {
            ColorMultiSelectionModel model;
            QAbstractItemModelTester tester(&model, kTesterMode);
            model.resetWith(colors(5));
            setSelected(model, 3, true);
            model.insertItem(0, ColorItem{.name = "x", .hex = "#1"});
            model.removeItem(2);
            setSelected(model, 1, true);
            model.clearSelection();
            model.resetWith(colors(2));
        }
    }

    [[= rqt::slot]] void capability_theHostConceptsAcceptTheDataMemberSignals()
    {
        static_assert(sel::SingleHost<ColorSingleSelectionModel>);
        static_assert(sel::MultiHost<ColorMultiSelectionModel>);
        static_assert(!sel::SingleHost<ColorMultiSelectionModel>);
        static_assert(!sel::MultiHost<ColorSingleSelectionModel>);
        static_assert(!sel::SingleHost<ColorListModel>);
        QVERIFY(true);
    }
};

QTEST_GUILESS_MAIN(TstSelection)
