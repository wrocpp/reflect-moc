// The matrix's table model (the acceptance tests of the spec, SPEC 6): the model mirrors the matrix after every structural step, a
// rename is one header change, dataChanged covers exactly the changed cells, an uninitialised cell is an invalid variant.
#include "matrix_support.hpp"

#include <QtCore/QCoreApplication>

namespace {
using namespace matrix_test;

void at_6_1_everyStructuralStepKeepsTheModelInStep() {
  Mat m(kNaN);
  observable::MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  CHECK(mirrors(model, m));

  m.addRow("r0");
  CHECK(mirrors(model, m));
  m.addColumn("c0");
  CHECK(mirrors(model, m));
  m.addRows({"r1", "r2", "r3"});
  CHECK(mirrors(model, m));
  m.addColumns({"c1", "c2"});
  CHECK(mirrors(model, m));
  m.setValue("r1", "c1", 4.0);
  CHECK(mirrors(model, m));
  m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r3", "c2", 3.0}});
  CHECK(mirrors(model, m));
  m.removeRow("r0");
  CHECK(mirrors(model, m));
  m.removeColumn("c1");
  CHECK(mirrors(model, m));
  m.addRows(keys("n", 6));
  m.removeRows({"n0", "n1", "n3", "n5", "r2"});
  CHECK(mirrors(model, m));
  m.addColumns(keys("m", 5));
  m.removeColumns({"m0", "m1", "m4", "c0"});
  CHECK(mirrors(model, m));
  m.renameRow("n2", "renamed");
  m.renameColumn("m2", "renamedColumn");
  CHECK(mirrors(model, m));
  CHECK((model.headerData(m.rowIndexOf("renamed"), Qt::Vertical).toString()) == (QString("renamed")));
  m.clear();
  CHECK(mirrors(model, m));
  CHECK((model.rowCount()) == (0));
  CHECK((model.columnCount()) == (0));
}

void at_6_2_sequenceT7ARenameIsOneHeaderChange() {
  Mat m(kNaN);
  m.addRows({"s1", "s2", "s3"});
  m.addColumn("c");
  observable::MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  Recorder source(m);
  QSignalSpy header(&model, &QAbstractItemModel::headerDataChanged);
  QSignalSpy structure1(&model, &QAbstractItemModel::rowsInserted);
  QSignalSpy structure2(&model, &QAbstractItemModel::rowsRemoved);
  QSignalSpy structure3(&model, &QAbstractItemModel::rowsMoved);
  QSignalSpy structure4(&model, &QAbstractItemModel::modelReset);

  m.renameRow("s2", "s2b");

  CHECK((source.log) == ((QStringList{"rowAboutToBeRenamed(s2,s2b,1)", "rowRenamed(s2,s2b,1)"})));
  CHECK((header.count()) == (1));
  CHECK((header.at(0).at(0).value<Qt::Orientation>()) == (Qt::Vertical));
  CHECK((header.at(0).at(1).toInt()) == (1));
  CHECK((header.at(0).at(2).toInt()) == (1));
  CHECK((structure1.count() + structure2.count() + structure3.count() + structure4.count()) == (0));
  CHECK((model.headerData(1, Qt::Vertical).toString()) == (QString("s2b")));
}

void at_6_3_dataChangedCoversExactlyTheChangedCells() {
  Mat m(kNaN);
  m.addRows({"r0", "r1"});
  m.addColumns({"c0", "c1"});
  observable::MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

  m.setValue("r1", "c0", 1.0);
  CHECK((spy.count()) == (1));
  CHECK((spy.at(0).at(0).toModelIndex()) == (model.index(1, 0)));
  CHECK((spy.at(0).at(1).toModelIndex()) == (model.index(1, 0)));

  spy.clear();
  m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r1", "c1", 9.0}, {"r1", "c0", 5.0}});
  // r0: c0..c1 and r1: c0..c1 are the same extent, so the block is ONE rectangle
  CHECK((spy.count()) == (1));
  CHECK((spy.at(0).at(0).toModelIndex()) == (model.index(0, 0)));
  CHECK((spy.at(0).at(1).toModelIndex()) == (model.index(1, 1)));

  spy.clear();
  Mat l(kNaN);
  l.addRows({"r0", "r1"});
  l.addColumns({"c0", "c1"});
  observable::MatrixTableModel<Mat> lModel(l);
  QAbstractItemModelTester lTester(&lModel, kTesterMode);
  QSignalSpy lSpy(&lModel, &QAbstractItemModel::dataChanged);
  l.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 2.0}, {"r1", "c0", 3.0}});
  CHECK((lSpy.count()) == (2));
  CHECK((lSpy.at(0).at(0).toModelIndex()) == (lModel.index(0, 0)));
  CHECK((lSpy.at(0).at(1).toModelIndex()) == (lModel.index(0, 1)));
  CHECK((lSpy.at(1).at(0).toModelIndex()) == (lModel.index(1, 0)));
  CHECK((lSpy.at(1).at(1).toModelIndex()) == (lModel.index(1, 0)));
}

void at_6_4_anUninitialisedCellIsAnInvalidVariant() {
  Mat m(kNaN);
  m.addRow("r");
  m.addColumn("c");
  observable::MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  CHECK(!model.data(model.index(0, 0), Qt::DisplayRole).isValid());
  m.setValue("r", "c", 2.0);
  CHECK((model.data(model.index(0, 0), Qt::DisplayRole).toDouble()) == (2.0));
  // The edit role answers what the display role does (the spec left it empty, and an editor would open blank),
  // and an uninitialised cell is empty for it too; another role is empty.
  CHECK(model.data(model.index(0, 0), Qt::EditRole).toDouble() == 2.0);
  CHECK(!model.data(model.index(0, 0), Qt::ToolTipRole).isValid());
}

void capability_aBatchRowRemovalReachesTheModelAsOneCyclePerRun() {
  Mat m(kNaN);
  m.addRows(keys("k", 8));
  m.addColumn("c");
  observable::MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  QStringList log;
  QObject ctx;
  QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &ctx, [&](const QModelIndex &, int f, int l) { log << QStringLiteral("%1-%2").arg(f).arg(l); });
  m.removeRows({"k1", "k2", "k5", "k7"});
  CHECK((log) == ((QStringList{"7-7", "5-5", "1-2"})));
  CHECK(mirrors(model, m));
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  at_6_1_everyStructuralStepKeepsTheModelInStep();
  at_6_2_sequenceT7ARenameIsOneHeaderChange();
  at_6_3_dataChangedCoversExactlyTheChangedCells();
  at_6_4_anUninitialisedCellIsAnInvalidVariant();
  capability_aBatchRowRemovalReachesTheModelAsOneCyclePerRun();
  return rqt_test::finish("capability_observable_matrix_model");
}
