// A receiver of a signal of a matrix mutates the same matrix while the announcement is still going out. Every mutator is
// tried from a receiver of the "about" and the "done" signal of every kind of change.
//
// The rule is the container's: the nested call is refused. It returns false (0 for a batch), changes nothing, emits
// nothing, and a debug build asserts. The same file is built twice. In a debug build every case runs in a child process
// and must end in SIGABRT. In the build that defines QT_NO_DEBUG (capability_edge_matrix_reentrancy_release) the calls
// return, each must say it took nothing, and the matrix and its model must equal those of the same outer change made without
// a receiver.
// Catalogue: docs/qt-edge-cases/matrix-selection.md (M-12, M-13).
#include "data_support.hpp"
#include "matrix_support.hpp"

#include <QtCore/QCoreApplication>

#include <functional>
#include <string>
#include <vector>

#ifndef EDGE_MATRIX_TEST_NAME
#define EDGE_MATRIX_TEST_NAME "capability_edge_matrix_reentrancy"
#endif

namespace {
using namespace matrix_test;
using observable::MatrixTableModel;

struct Fixture {
  Fixture() : matrix(kNaN), model(matrix), tester(&model, kTesterMode) {
    matrix.addRows({"r0", "r1", "r2", "r3"});
    matrix.addColumns({"c0", "c1", "c2"});
    matrix.setValue("r1", "c1", 5.0);
  }
  Mat matrix;
  MatrixTableModel<Mat> model;
  QAbstractItemModelTester tester;

  // The matrix and what its model shows, as text.
  QString state() const {
    QStringList lines;
    lines << matrix.rowKeys().join(',') << matrix.columnKeys().join(',');
    for (qsizetype i = 0; i < matrix.rowCount(); ++i)
      for (qsizetype j = 0; j < matrix.columnCount(); ++j) lines << QString::number(matrix.valueAt(i, j));
    lines << QString::number(model.rowCount()) << QString::number(model.columnCount()) << (mirrors(model, matrix) ? "mirrored" : "DIVERGED");
    return lines.join('|');
  }
};

struct Nested {
  std::string name;
  std::function<bool(Mat&)> call;  // true: the matrix took the change
};

std::vector<Nested> nested_changes() {
  return {
      {"addRow", [](Mat& m) { return m.addRow("n1"); }},
      {"addColumn", [](Mat& m) { return m.addColumn("n1"); }},
      {"addRows", [](Mat& m) { return m.addRows({"n2", "n3"}) != 0; }},
      {"addColumns", [](Mat& m) { return m.addColumns({"n2", "n3"}) != 0; }},
      {"removeRow", [](Mat& m) { return m.removeRow("r0"); }},
      {"removeColumn", [](Mat& m) { return m.removeColumn("c0"); }},
      {"removeRows", [](Mat& m) { return m.removeRows({"r0", "r2"}) != 0; }},
      {"removeColumns", [](Mat& m) { return m.removeColumns({"c0", "c2"}) != 0; }},
      {"renameRow", [](Mat& m) { return m.renameRow("r0", "renamed"); }},
      {"renameColumn", [](Mat& m) { return m.renameColumn("c0", "renamed"); }},
      {"setValue", [](Mat& m) { return m.setValue("r0", "c0", 9.0); }},
      {"setValues", [](Mat& m) { return m.setValues({{"r0", "c0", 9.0}, {"r2", "c2", 9.0}}) != 0; }},
      {"clear", [](Mat& m) { return m.clear(); }},
  };
}

// An outer change, with the signal that the receiver is connected to.
struct Outer {
  std::string name;
  std::function<void(Mat&, std::function<void()>, QObject&)> connect;
  std::function<void(Mat&)> change;
};

template <class Signal>
std::function<void(Mat&, std::function<void()>, QObject&)> on(Signal signal) {
  return [signal](Mat& m, std::function<void()> receiver, QObject& context) {
    QObject::connect(&m, signal, &context, [receiver](auto&&...) { receiver(); });
  };
}

std::vector<Outer> outer_changes() {
  return {
      {"rowAboutToBeAdded", on(&Mat::rowAboutToBeAdded), [](Mat& m) { m.addRow("x"); }},
      {"rowAdded", on(&Mat::rowAdded), [](Mat& m) { m.addRow("x"); }},
      {"columnAboutToBeAdded", on(&Mat::columnAboutToBeAdded), [](Mat& m) { m.addColumn("x"); }},
      {"columnAdded", on(&Mat::columnAdded), [](Mat& m) { m.addColumn("x"); }},
      {"rowsAboutToBeAdded", on(&Mat::rowsAboutToBeAdded), [](Mat& m) { m.addRows({"x", "y"}); }},
      {"rowsAdded", on(&Mat::rowsAdded), [](Mat& m) { m.addRows({"x", "y"}); }},
      {"columnsAdded", on(&Mat::columnsAdded), [](Mat& m) { m.addColumns({"x", "y"}); }},
      {"rowAboutToBeRemoved", on(&Mat::rowAboutToBeRemoved), [](Mat& m) { m.removeRow("r1"); }},
      {"rowRemoved", on(&Mat::rowRemoved), [](Mat& m) { m.removeRow("r1"); }},
      {"columnRemoved", on(&Mat::columnRemoved), [](Mat& m) { m.removeColumn("c1"); }},
      {"rowsAboutToBeRemoved", on(&Mat::rowsAboutToBeRemoved), [](Mat& m) { m.removeRows({"r0", "r2"}); }},
      {"rowsRemoved", on(&Mat::rowsRemoved), [](Mat& m) { m.removeRows({"r0", "r2"}); }},
      {"columnsRemoved", on(&Mat::columnsRemoved), [](Mat& m) { m.removeColumns({"c0", "c2"}); }},
      {"rowAboutToBeRenamed", on(&Mat::rowAboutToBeRenamed), [](Mat& m) { m.renameRow("r1", "q"); }},
      {"rowRenamed", on(&Mat::rowRenamed), [](Mat& m) { m.renameRow("r1", "q"); }},
      {"columnRenamed", on(&Mat::columnRenamed), [](Mat& m) { m.renameColumn("c1", "q"); }},
      {"cellAboutToChange", on(&Mat::cellAboutToChange), [](Mat& m) { m.setValue("r2", "c2", 4.0); }},
      {"cellChanged", on(&Mat::cellChanged), [](Mat& m) { m.setValue("r2", "c2", 4.0); }},
      {"cellsAboutToChange", on(&Mat::cellsAboutToChange), [](Mat& m) { m.setValues({{"r2", "c2", 4.0}, {"r3", "c0", 1.0}}); }},
      {"cellsChanged", on(&Mat::cellsChanged), [](Mat& m) { m.setValues({{"r2", "c2", 4.0}, {"r3", "c0", 1.0}}); }},
      {"aboutToBeCleared", on(&Mat::aboutToBeCleared), [](Mat& m) { m.clear(); }},
      {"cleared", on(&Mat::cleared), [](Mat& m) { m.clear(); }},
  };
}

// The state after the outer change alone.
[[maybe_unused]] QString control(const Outer& outer) {
  Fixture f;
  outer.change(f.matrix);
  return f.state();
}

// Runs the outer change with `nested` called from a receiver of its signal; returns whether the nested call took and the
// state afterwards.
std::pair<bool, QString> run(const Outer& outer, const Nested& nested) {
  Fixture f;
  QObject context;
  bool took = false;
  int calls = 0;
  outer.connect(f.matrix, [&] {
    if (calls++ == 0) took = nested.call(f.matrix);
  }, context);
  outer.change(f.matrix);
  return {took, f.state()};
}

#ifndef QT_NO_DEBUG
// Every nested call asserts, in a child process.
void every_nested_change_asserts() {
  for (const Outer& outer : outer_changes())
    for (const Nested& nested : nested_changes()) {
      const bool aborted = edge_data::aborts([&] { run(outer, nested); });
      if (!aborted) std::fprintf(stderr, "no assertion: %s from %s\n", nested.name.c_str(), outer.name.c_str());
      CHECK(aborted);
    }
}
#else
// Every nested call is refused and the matrix is as if it had not been made.
void every_nested_change_is_refused() {
  for (const Outer& outer : outer_changes()) {
    const QString expected = control(outer);
    for (const Nested& nested : nested_changes()) {
      const auto [took, state] = run(outer, nested);
      if (took || state != expected) std::fprintf(stderr, "%s from %s: took %d, state %s\n", nested.name.c_str(), outer.name.c_str(), took, qPrintable(state));
      CHECK(!took && state == expected);
    }
  }
}
#endif
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
#ifndef QT_NO_DEBUG
  every_nested_change_asserts();
#else
  every_nested_change_is_refused();
#endif
  return rqt_test::finish(EDGE_MATRIX_TEST_NAME);
}
