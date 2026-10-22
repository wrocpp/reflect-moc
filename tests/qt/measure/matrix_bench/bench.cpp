// What one change of a keyed matrix costs, at 10,000 and 100,000 rows: observable::ObservableMatrix, which finds a key's index
// with a Fenwick tree over insertion slots, against the matrix of the spec (examples/qt/spec_regen/binding_b), which marks its
// key-to-index map stale after every change of structure or of a key and rebuilds it at the next lookup. Each operation is
// timed on a matrix with a table model attached (the models of both) and on a bare matrix, and printed as nanoseconds per
// change.
//
//   bench <rows>   one line per implementation and operation: "<impl> <model|bare> <operation> <rows> <ns per change>"
#include <matrix_table_model.hpp>
#include <observable_matrix.hpp>

#include <obs/matrix.h>
#include <obs/matrix_table_model.h>

#include <QtCore/QCoreApplication>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

namespace {
constexpr int kColumns = 8;
constexpr unsigned kSeed = 7;

using Clock = std::chrono::steady_clock;

// How many changes of each kind are timed: enough to average out, few enough that a linear rebuild at 100,000 rows finishes.
int samples(int rows) { return rows >= 100000 ? 60 : 400; }

double per_change_ns(Clock::time_point start, int changes) {
  return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count()) / changes;
}

template <class Matrix>
void fill(Matrix& m, int rows) {
  QList<int> keys;
  keys.reserve(rows);
  for (int i = 0; i < rows; ++i) keys.append(i);
  m.addRows(keys);
  QList<int> columns;
  for (int j = 0; j < kColumns; ++j) columns.append(j);
  m.addColumns(columns);
}

template <class Matrix, class Model>
void measure(const char* impl, int rows, bool withModel) {
  Matrix m(-1.0);
  std::unique_ptr<Model> model;
  fill(m, rows);
  if (withModel) model = std::make_unique<Model>(m);
  std::mt19937 rng(kSeed);
  const int n = samples(rows);
  const char* label = withModel ? "model" : "bare";
  const auto report = [&](const char* operation, Clock::time_point start) { std::printf("%s %s %s %d %.0f\n", impl, label, operation, rows, per_change_ns(start, n)); };
  const auto random_row = [&] { return static_cast<int>(rng() % static_cast<unsigned>(rows)); };

  {  // set a cell: no change of structure
    const auto start = Clock::now();
    for (int i = 0; i < n; ++i) m.setValue(random_row(), static_cast<int>(rng() % kColumns), static_cast<double>(i));
    report("setValue", start);
  }
  {  // remove a row in the middle, then add one back at the end (timed together as two changes of structure)
    std::vector<int> removed;
    const auto start = Clock::now();
    for (int i = 0; i < n; ++i) {
      const int key = random_row();
      if (m.removeRow(key)) removed.push_back(key);
    }
    const double removal = per_change_ns(start, n);
    std::printf("%s %s removeRow %d %.0f\n", impl, label, rows, removal);
    const auto addStart = Clock::now();
    for (const int key : removed) m.addRow(key);
    std::printf("%s %s addRow %d %.0f\n", impl, label, rows, per_change_ns(addStart, static_cast<int>(removed.size())));
  }
  {  // give a row another key
    const auto start = Clock::now();
    for (int i = 0; i < n; ++i) m.renameRow(random_row(), rows + 1 + i);
    report("renameRow", start);
  }
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  const int rows = argc > 1 ? std::atoi(argv[1]) : 10000;
  for (const bool withModel : {false, true}) {
    measure<obs::Matrix<int, int, double>, obs::MatrixTableModel<obs::Matrix<int, int, double>>>("spec", rows, withModel);
    measure<observable::ObservableMatrix<int, int, double>, observable::MatrixTableModel<observable::ObservableMatrix<int, int, double>>>("observable", rows, withModel);
  }
  return 0;
}
