// The hosts of the selection tracker (the acceptance tests of the spec, SPEC 8): a Single and a Multi model over an observable
// container of colours announce a change of the selection in the order the contract asks for, the tracker follows the rows through
// an insertion, a removal and a reset, and both models pass QAbstractItemModelTester in fatal mode.
#include "check.hpp"
#include "selection_hosts.hpp"

#include <QtCore/QCoreApplication>
#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>

namespace {
using namespace selection_test;

constexpr auto kTesterMode = QAbstractItemModelTester::FailureReportingMode::Fatal;

inline QList<ColorItem> colors(int n) {
  QList<ColorItem> items;
  for (int i = 0; i < n; ++i) items.append(ColorItem{.name = QStringLiteral("color%1").arg(i), .hex = QStringLiteral("#%1").arg(i, 6, 16, QLatin1Char('0'))});
  return items;
}

// One object connected to every notification of a host: the model's standard signals and the host's own.
template <class Model>
class HostLog {
 public:
  explicit HostLog(Model& m) {
    QObject::connect(&m, &QAbstractItemModel::dataChanged, &context, [this](const QModelIndex& a, const QModelIndex& b, const QList<int>& roles) {
      QStringList r;
      for (int role : roles) r << (role == Model::SelectedRole ? QStringLiteral("S") : QString::number(role));
      log << QStringLiteral("dataChanged(%1,%2,{%3})").arg(a.row()).arg(b.row()).arg(r.join(QLatin1Char(',')));
    });
    QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeInserted, &context, [this](const QModelIndex&, int f, int l) { log << QStringLiteral("rowsAboutToBeInserted(%1,%2)").arg(f).arg(l); });
    QObject::connect(&m, &QAbstractItemModel::rowsInserted, &context, [this](const QModelIndex&, int f, int l) { log << QStringLiteral("rowsInserted(%1,%2)").arg(f).arg(l); });
    QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeRemoved, &context, [this](const QModelIndex&, int f, int l) { log << QStringLiteral("rowsAboutToBeRemoved(%1,%2)").arg(f).arg(l); });
    QObject::connect(&m, &QAbstractItemModel::rowsRemoved, &context, [this](const QModelIndex&, int f, int l) { log << QStringLiteral("rowsRemoved(%1,%2)").arg(f).arg(l); });
    QObject::connect(&m, &QAbstractItemModel::modelAboutToBeReset, &context, [this] { log << QStringLiteral("modelAboutToBeReset"); });
    QObject::connect(&m, &QAbstractItemModel::modelReset, &context, [this] { log << QStringLiteral("modelReset"); });
    QObject::connect(&m, &Model::hasSelectionChanged, &context, [this] { log << QStringLiteral("hasSelectionChanged"); });
    if constexpr (requires { &Model::selectedRowChanged; }) QObject::connect(&m, &Model::selectedRowChanged, &context, [this] { log << QStringLiteral("selectedRowChanged"); });
    if constexpr (requires { &Model::selectedCountChanged; }) QObject::connect(&m, &Model::selectedCountChanged, &context, [this] { log << QStringLiteral("selectedCountChanged"); });
  }

  int count(const QString& prefix) const {
    int n = 0;
    for (const QString& line : log) n += line.startsWith(prefix) ? 1 : 0;
    return n;
  }

  QObject context;
  QStringList log;
};

template <class Model>
bool setSelected(Model& model, int row, bool selected) {
  return model.setData(model.index(row), selected, Model::SelectedRole);
}

template <class Model>
bool isRowSelected(const Model& model, int row) {
  return model.data(model.index(row), Model::SelectedRole).toBool();
}

template <class Model>
QList<int> selectedRowsOf(const Model& model) {
  QList<int> rows;
  for (int r = 0; r < model.rowCount(); ++r)
    if (isRowSelected(model, r)) rows << r;
  return rows;
}

void at_8_1_bothModelsNameTheSelectedRole() {
  ColorSingleSelectionModel single;
  ColorMultiSelectionModel multi;
  CHECK((single.roleNames().value(ColorSingleSelectionModel::SelectedRole)) == (QByteArray("isSelected")));
  CHECK((multi.roleNames().value(ColorMultiSelectionModel::SelectedRole)) == (QByteArray("isSelected")));
  CHECK((ColorSingleSelectionModel::SelectedRole) == (static_cast<int>(selection_test::HexRole) + 1));
  CHECK((ColorMultiSelectionModel::SelectedRole) == (static_cast<int>(selection_test::HexRole) + 1));
  CHECK((single.roleNames().value(selection_test::NameRole)) == (QByteArray("name")));
  CHECK((single.roleNames().value(selection_test::HexRole)) == (QByteArray("hex")));
}

void at_8_2_singleSelectThenSequenceT8() {
  ColorSingleSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(6));
  HostLog<ColorSingleSelectionModel> log(model);

  CHECK(setSelected(model, 3, true));
  CHECK((log.log) == ((QStringList{"selectedRowChanged", "dataChanged(3,3,{S})", "hasSelectionChanged"})));
  CHECK((log.count("dataChanged")) == (1));

  log.log.clear();  // T8
  CHECK(setSelected(model, 5, true));
  CHECK((log.log) == ((QStringList{"selectedRowChanged", "dataChanged(3,3,{S})", "dataChanged(5,5,{S})"})));

  log.log.clear();
  CHECK(setSelected(model, 5, true));
  CHECK(log.log.isEmpty());
}

void at_8_2_theRolesOfDataChangedAreExactlyTheSelectedRole() {
  ColorSingleSelectionModel model;
  model.resetWith(colors(3));
  QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);
  setSelected(model, 1, true);
  CHECK((spy.count()) == (1));
  CHECK((spy.at(0).at(2).value<QList<int>>()) == ((QList<int>{ColorSingleSelectionModel::SelectedRole})));
}

void at_8_3_sequenceT9DeselectingAnotherRowChangesNothing() {
  ColorSingleSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(6));
  setSelected(model, 3, true);
  const QString name = model.selectedName();
  CHECK((name) == (QString("color3")));
  HostLog<ColorSingleSelectionModel> log(model);

  CHECK(setSelected(model, 5, false));
  CHECK(log.log.isEmpty());
  CHECK(isRowSelected(model, 3));
  CHECK((model.selectedName()) == (name));

  CHECK(setSelected(model, 3, false));
  CHECK((log.log) == ((QStringList{"selectedRowChanged", "dataChanged(3,3,{S})", "hasSelectionChanged"})));
  CHECK(!model.hasSelection());
}

void at_8_4_sequenceT14InsertionThenSequenceT13Removal() {
  ColorSingleSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(6));
  setSelected(model, 2, true);
  const QString name = model.selectedName();
  HostLog<ColorSingleSelectionModel> log(model);

  model.insertItem(0, ColorItem{.name = "inserted", .hex = "#ffffff"});
  CHECK((log.log) == ((QStringList{"rowsAboutToBeInserted(0,0)", "rowsInserted(0,0)", "selectedRowChanged"})));
  CHECK(isRowSelected(model, 3));
  CHECK((model.selectedName()) == (name));
  CHECK((selectedRowsOf(model)) == ((QList<int>{3})));

  log.log.clear();  // T13 on the new selected row
  model.removeItem(3);
  CHECK((log.log) == ((QStringList{"rowsAboutToBeRemoved(3,3)", "rowsRemoved(3,3)", "selectedRowChanged", "hasSelectionChanged"})));
  CHECK(selectedRowsOf(model).isEmpty());
  CHECK(!model.hasSelection());
  CHECK((model.selectedName()) == (QString()));
}

void at_8_4_theSelectionAlreadyPointsRightWhenTheViewHearsTheInsertion() {
  ColorSingleSelectionModel model;
  model.resetWith(colors(4));
  setSelected(model, 1, true);
  QObject ctx;
  QString nameSeen;
  QObject::connect(&model, &QAbstractItemModel::rowsInserted, &ctx, [&] { nameSeen = model.selectedName(); });
  model.insertItem(0, ColorItem{.name = "new", .hex = "#000000"});
  CHECK((nameSeen) == (QString("color1")));
}

void at_8_5_sequenceT10ClearSelection() {
  ColorSingleSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(4));
  setSelected(model, 1, true);
  HostLog<ColorSingleSelectionModel> log(model);

  model.clearSelection();
  CHECK((log.log) == ((QStringList{"dataChanged(1,1,{S})", "hasSelectionChanged", "selectedRowChanged"})));
  CHECK(!isRowSelected(model, 1));

  log.log.clear();
  model.clearSelection();
  CHECK(log.log.isEmpty());
}

void at_8_5_clearSelectionIsInvokableThroughQtsMetaObject() {
  ColorSingleSelectionModel model;
  model.resetWith(colors(2));
  setSelected(model, 0, true);
  CHECK(QMetaObject::invokeMethod(&model, "clearSelection"));
  CHECK(!model.hasSelection());
  CHECK((model.property("hasSelection").toBool()) == (false));
  setSelected(model, 1, true);
  CHECK((model.property("hasSelection").toBool()) == (true));
  CHECK((model.property("selectedName").toString()) == (QString("color1")));
}

void at_8_6_multiSequenceT11ClearSelection() {
  ColorMultiSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(10));
  for (int row : {8, 0, 2, 1, 5, 7})
    CHECK(setSelected(model, row, true));
  CHECK((model.selectedCount()) == (6));
  HostLog<ColorMultiSelectionModel> log(model);

  model.clearSelection();
  CHECK((log.log) == ((QStringList{"dataChanged(0,2,{S})", "dataChanged(5,5,{S})", "dataChanged(7,8,{S})", "hasSelectionChanged", "selectedCountChanged"})));
  CHECK((log.count("modelReset") + log.count("modelAboutToBeReset")) == (0));
  CHECK((model.selectedCount()) == (0));
  CHECK(selectedRowsOf(model).isEmpty());
}

void at_8_7_multiTicksAndSequenceT12() {
  ColorMultiSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(3));
  HostLog<ColorMultiSelectionModel> log(model);

  setSelected(model, 1, true);  // T12
  CHECK((log.log) == ((QStringList{"dataChanged(1,1,{S})", "selectedCountChanged", "hasSelectionChanged"})));

  setSelected(model, 1, true);
  setSelected(model, 2, true);
  setSelected(model, 0, false);
  CHECK((log.count("dataChanged")) == (2));
  CHECK((log.count("selectedCountChanged")) == (2));
  CHECK((log.count("hasSelectionChanged")) == (1));
}

void at_8_8_multiRemovalNotifiesFromTheKeptDelta() {
  ColorMultiSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(4));
  setSelected(model, 1, true);
  setSelected(model, 3, true);
  HostLog<ColorMultiSelectionModel> log(model);

  model.removeItem(0);
  CHECK((log.log) == ((QStringList{"rowsAboutToBeRemoved(0,0)", "rowsRemoved(0,0)"})));
  CHECK((selectedRowsOf(model)) == ((QList<int>{0, 2})));

  log.log.clear();
  model.removeItem(0);
  CHECK((log.count("selectedCountChanged")) == (1));
  CHECK((log.count("hasSelectionChanged")) == (0));

  log.log.clear();
  model.removeItem(1);
  CHECK((log.count("selectedCountChanged")) == (1));
  CHECK((log.count("hasSelectionChanged")) == (1));
  CHECK(!model.hasSelection());
}

void at_8_9_multiResetDropsTheSelectionWithOneNotificationEach() {
  ColorMultiSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(3));
  setSelected(model, 2, true);
  HostLog<ColorMultiSelectionModel> log(model);

  model.resetWith(colors(5));
  CHECK((model.rowCount()) == (5));
  CHECK((model.selectedCount()) == (0));
  CHECK((log.log) == ((QStringList{"modelAboutToBeReset", "modelReset", "selectedCountChanged", "hasSelectionChanged"})));
  CHECK(setSelected(model, 4, true));
  CHECK(isRowSelected(model, 4));
}

void at_8_9_singleResetDropsTheSelectionToo() {
  ColorSingleSelectionModel model;
  QAbstractItemModelTester tester(&model, kTesterMode);
  model.resetWith(colors(3));
  setSelected(model, 2, true);
  HostLog<ColorSingleSelectionModel> log(model);
  model.resetWith(colors(2));
  CHECK((log.log) == ((QStringList{"modelAboutToBeReset", "modelReset", "selectedRowChanged", "hasSelectionChanged"})));
  CHECK(!model.hasSelection());

  log.log.clear();
  model.resetWith(colors(4));  // nothing was selected: nothing to announce
  CHECK((log.log) == ((QStringList{"modelAboutToBeReset", "modelReset"})));
}

void at_8_10_bothModelsPassTheModelTester() {
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

void capability_theHostConceptsAcceptTheDataMemberSignals() {
  static_assert(observable::SingleHost<ColorSingleSelectionModel>);
  static_assert(observable::MultiHost<ColorMultiSelectionModel>);
  static_assert(!observable::SingleHost<ColorMultiSelectionModel>);
  static_assert(!observable::MultiHost<ColorSingleSelectionModel>);
  static_assert(!observable::SingleHost<ColorListModel>);
  CHECK(true);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  at_8_1_bothModelsNameTheSelectedRole();
  at_8_2_singleSelectThenSequenceT8();
  at_8_2_theRolesOfDataChangedAreExactlyTheSelectedRole();
  at_8_3_sequenceT9DeselectingAnotherRowChangesNothing();
  at_8_4_sequenceT14InsertionThenSequenceT13Removal();
  at_8_4_theSelectionAlreadyPointsRightWhenTheViewHearsTheInsertion();
  at_8_5_sequenceT10ClearSelection();
  at_8_5_clearSelectionIsInvokableThroughQtsMetaObject();
  at_8_6_multiSequenceT11ClearSelection();
  at_8_7_multiTicksAndSequenceT12();
  at_8_8_multiRemovalNotifiesFromTheKeptDelta();
  at_8_9_multiResetDropsTheSelectionWithOneNotificationEach();
  at_8_9_singleResetDropsTheSelectionToo();
  at_8_10_bothModelsPassTheModelTester();
  capability_theHostConceptsAcceptTheDataMemberSignals();
  return rqt_test::finish("capability_observable_selection_models");
}
