// The matrix and its table model, driven by thousands of random changes (adds, removes, renames, writes, batches of writes,
// clears). After every change the matrix, the model, a view mirror built from signals alone, a selection of rows and a
// persistent index must agree with a plain reference. QAbstractItemModelTester runs in fatal mode on the real model.
// The driver is then run against adapters with one mistake each (a rename announced as a removal and an insertion, a
// bounding box instead of rectangles, a column with no announcement): it must report every one of them and the right
// model not at all.
#include "observable_matrix_flawed.hpp"

#include <QtCore/QCoreApplication>
#include <QtTest/QAbstractItemModelTester>

#include <cstdio>

namespace {
constexpr int kSteps = 3000;
constexpr unsigned kSeeds[] = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr int kControlSteps = 4000;

void the_matrix_model_follows_random_changes() {
  int applied = 0;
  int refused = 0;
  for (const unsigned seed : kSeeds) {
    std::unique_ptr<QAbstractItemModelTester> tester;
    const auto report = observable_test::run_random_matrix(seed, kSteps, [&](observable_test::IntMatrix* matrix) {
      auto model = std::make_unique<observable::MatrixTableModel<observable_test::IntMatrix>>(*matrix);
      tester = std::make_unique<QAbstractItemModelTester>(model.get(), QAbstractItemModelTester::FailureReportingMode::Fatal);
      return std::unique_ptr<QAbstractItemModel>(std::move(model));
    });
    applied += report.applied;
    refused += report.refused;
    if (!report.clean()) {
      std::fprintf(stderr, "seed %u: %d mismatches\n", seed, report.mismatches);
      for (const QString& v : report.violations.mid(0, 3) + report.details) std::fprintf(stderr, "  %s\n", qPrintable(v));
    }
    CHECK(report.clean());
    tester.reset();
  }
  CHECK(applied > 0 && refused > 0);  // the driver both changes the matrix and asks for what it must refuse
  std::printf("matrix: %d changes applied, %d requests refused\n", applied, refused);
}

// Each flaw is reported, the control (no flaw) is not.
void the_driver_reports_every_flawed_adapter() {
  using observable_test::MatrixFlaw;
  for (const MatrixFlaw flaw : {MatrixFlaw::none, MatrixFlaw::rename_as_remove_insert, MatrixFlaw::bounding_box, MatrixFlaw::column_without_signal}) {
    int caught = 0;
    for (const unsigned seed : kSeeds) {
      const auto report = observable_test::run_random_matrix(seed, kControlSteps, [flaw](observable_test::IntMatrix* matrix) {
        return std::unique_ptr<QAbstractItemModel>(new observable_test::FlawedMatrixModel(matrix, flaw));
      });
      caught += report.clean() ? 0 : 1;
    }
    if (flaw == MatrixFlaw::none) {
      CHECK(caught == 0);
    } else {
      if (caught == 0) std::fprintf(stderr, "flaw %s was not caught\n", observable_test::name_of(flaw));
      CHECK(caught > 0);
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  the_matrix_model_follows_random_changes();
  the_driver_reports_every_flawed_adapter();
  return rqt_test::finish("capability_observable_matrix_random");
}
