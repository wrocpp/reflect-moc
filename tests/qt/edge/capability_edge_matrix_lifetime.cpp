// Matrix lifetime: a model deleted before its matrix, a matrix deleted before its model (the model resets to empty and every call
// stays safe), a model made while the matrix is announcing a change, deleteLater of the matrix during an announcement, the order
// of the destroyed signals, and the one-thread rule (a debug build asserts, in a child process).
// Catalogue: docs/qt-edge-cases/matrix-selection.md (M-14 to M-19).
#include "data_support.hpp"
#include "matrix_support.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QThread>

namespace {
using namespace matrix_test;
using observable::MatrixTableModel;

void fill(Mat& m) {
  m.addRows({"r0", "r1", "r2"});
  m.addColumns({"c0", "c1"});
  m.setValue("r1", "c1", 3.0);
}

// A model that is deleted first leaves nothing behind: the matrix goes on changing and says nothing to anyone.
void a_model_deleted_first() {
  Mat m(kNaN);
  fill(m);
  delete new MatrixTableModel<Mat>(m);
  CHECK(m.addRow("x") && m.removeRow("r0") && m.setValue("x", "c0", 1.0) && m.clear());
}

// A matrix deleted first: the model resets to empty, once, and every call stays safe.
void a_matrix_deleted_first() {
  auto* m = new Mat(kNaN);
  fill(*m);
  MatrixTableModel<Mat> model(*m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  int resets = 0;
  QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&] { ++resets; });
  CHECK(model.rowCount() == 3 && model.columnCount() == 2);
  delete m;
  CHECK(resets == 1 && model.rowCount() == 0 && model.columnCount() == 0);
  CHECK(!model.data(model.index(0, 0)).isValid() && !model.headerData(0, Qt::Vertical).isValid() && !model.headerData(0, Qt::Horizontal).isValid());
  CHECK(!model.flags(model.index(0, 0)).testFlag(Qt::ItemIsEditable));
}

// A model made while the matrix is announcing a change did not hear the "about" signal: at the "done" it resets to the matrix's
// content instead of ending a change it never began, for a pair of rows, of columns, and of a clear.
void a_model_made_in_the_middle_of_a_change() {
  for (int kind = 0; kind < 3; ++kind) {
    Mat m(kNaN);
    fill(m);
    std::unique_ptr<MatrixTableModel<Mat>> model;
    std::unique_ptr<QAbstractItemModelTester> tester;
    int resets = 0;
    const auto make = [&] {
      model = std::make_unique<MatrixTableModel<Mat>>(m);
      QObject::connect(model.get(), &QAbstractItemModel::modelReset, model.get(), [&] { ++resets; });
      tester = std::make_unique<QAbstractItemModelTester>(model.get(), kTesterMode);
    };
    QObject context;
    if (kind == 0) QObject::connect(&m, &Mat::rowsAboutToBeRemoved, &context, make);
    if (kind == 1) QObject::connect(&m, &Mat::columnAboutToBeAdded, &context, make);
    if (kind == 2) QObject::connect(&m, &Mat::aboutToBeCleared, &context, make);
    if (kind == 0) m.removeRows({"r0", "r1"});
    if (kind == 1) m.addColumn("x");
    if (kind == 2) m.clear();
    CHECK(resets == 1);  // it followed the matrix with one reset
    CHECK(model->rowCount() == m.rowCount() && model->columnCount() == m.columnCount() && mirrors(*model, m));
    // And it follows from there on.
    m.addRow("later");
    CHECK(mirrors(*model, m));
  }
}

// deleteLater of the matrix from a receiver of a change: the change is announced in full, the matrix goes at the next turn
// of the event loop, and the model resets to empty then.
void a_matrix_deleted_later_during_a_change() {
  auto* m = new Mat(kNaN);
  QPointer<Mat> guard(m);
  fill(*m);
  MatrixTableModel<Mat> model(*m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  QObject::connect(m, &Mat::rowsRemoved, m, [&] { m->deleteLater(); });
  CHECK(m->removeRows({"r0", "r2"}) == 2);
  CHECK(guard && model.rowCount() == 1);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  CHECK(!guard && model.rowCount() == 0 && model.columnCount() == 0);
}

// The destroyed signals: a model that is a child of its matrix hears of its destruction first (and resets), and goes after it.
void destroyed_in_order() {
  QStringList order;
  auto* m = new Mat(kNaN);
  auto* model = new MatrixTableModel<Mat>(*m, m);  // a child
  QObject::connect(m, &QObject::destroyed, [&] { order.append("matrix"); });
  QObject::connect(model, &QObject::destroyed, [&] { order.append("model"); });
  delete m;
  CHECK(order == (QStringList{"matrix", "model"}));
}

#ifndef QT_NO_DEBUG
// A matrix and its models live in one thread: a change from another thread, a matrix moved away, and a model made on a thread
// that is not the matrix's all assert.
void one_thread_rule() {
  using edge_data::aborts;
  CHECK(!aborts([] {
    Mat m(kNaN);
    MatrixTableModel<Mat> model(m);
    m.addRow("r");
  }));
  CHECK(aborts([] {
    Mat m(kNaN);
    QThread other;
    m.moveToThread(&other);
    m.addRow("r");
  }));
  CHECK(aborts([] {
    Mat m(kNaN);
    QThread other;
    m.moveToThread(&other);
    MatrixTableModel<Mat> model(m);
  }));
  CHECK(aborts([] {
    Mat m(kNaN);
    QThread worker;
    QObject::connect(&worker, &QThread::started, &worker, [&] { m.addRow("r"); }, Qt::DirectConnection);
    worker.start();
    worker.wait(3000);
  }));
}
#endif
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
#ifndef QT_NO_DEBUG
  one_thread_rule();  // forks first: the process has no other thread yet
#endif
  a_model_deleted_first();
  a_matrix_deleted_first();
  a_model_made_in_the_middle_of_a_change();
  a_matrix_deleted_later_during_a_change();
  destroyed_in_order();
  return rqt_test::finish("capability_edge_matrix_lifetime");
}
