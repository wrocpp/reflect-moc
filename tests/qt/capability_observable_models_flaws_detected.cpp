// The checks of the stress test must catch a wrong adapter. Each adapter in observable_models_flawed.hpp has
// one mistake of the kind that makes ghost rows, and the same random driver must report it. The same driver
// reports nothing for the right adapter, so a detection is not noise.
#include "observable_models_flawed.hpp"

#include <QtCore/QCoreApplication>

#include <cstdio>

namespace {
constexpr int kSteps = 600;
constexpr unsigned kSeed = 11;
constexpr observable_test::Flaw kFlaws[] = {
    observable_test::Flaw::forgot_end_insert, observable_test::Flaw::late_announce_insert,
    observable_test::Flaw::wrong_row_remove,  observable_test::Flaw::silent_replace,
    observable_test::Flaw::forgot_end_move,   observable_test::Flaw::forgot_end_reset,
};
constexpr observable_test::BatchFlaw kBatchFlaws[] = {
    observable_test::BatchFlaw::removal_ascending, observable_test::BatchFlaw::insertion_one_span,
    observable_test::BatchFlaw::reset_keeps_rows,  observable_test::BatchFlaw::count_from_container,
};
}  // namespace

static observable_test::Report run(observable_test::BatchFlaw flaw) {
  using Map = QMap<int, observable_test::Sample>;
  return observable_test::run_random<Map, observable::ObservableContainer<Map>>(
      kSeed, kSteps, [flaw](observable::ObservableContainer<Map>* source) {
        return std::make_unique<observable_test::FlawedMapModel>(source, flaw);
      },
      true);
}

static observable_test::Report run(observable_test::Flaw flaw) {
  return observable_test::run_random<QList<observable_test::Sample>, observable::ObservableContainer<QList<observable_test::Sample>>>(
      kSeed, kSteps, [flaw](observable::ObservableContainer<QList<observable_test::Sample>>* source) {
        return std::make_unique<observable_test::FlawedListModel>(source, flaw);
      },
      true);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  const auto right = run(observable_test::Flaw::none);
  std::printf("%-22s mismatches %d, violations %lld\n", "none (the right model)", right.mismatches,
              static_cast<long long>(right.violations.size()));
  std::fflush(stdout);
  CHECK(right.clean());

  for (const auto flaw : kFlaws) {
    std::printf("running %s\n", observable_test::name_of(flaw));
    std::fflush(stdout);
    const auto report = run(flaw);
    std::printf("%-22s mismatches %d, violations %lld  first: %s\n", observable_test::name_of(flaw), report.mismatches,
                static_cast<long long>(report.violations.size()),
                report.violations.isEmpty() ? "-" : qPrintable(report.violations.first()));
    std::fflush(stdout);
    CHECK(!report.clean());
  }

  // The same for a hand-written model of a map that follows batches and resets.
  const auto rightMap = run(observable_test::BatchFlaw::none);
  std::printf("%-22s mismatches %d, violations %lld\n", "none (a map model)", rightMap.mismatches,
              static_cast<long long>(rightMap.violations.size()));
  CHECK(rightMap.clean());
  for (const auto flaw : kBatchFlaws) {
    const auto report = run(flaw);
    std::printf("%-22s mismatches %d, violations %lld  first: %s\n", observable_test::name_of(flaw), report.mismatches,
                static_cast<long long>(report.violations.size()),
                report.violations.isEmpty() ? "-" : qPrintable(report.violations.first()));
    std::fflush(stdout);
    CHECK(!report.clean());
  }
  return rqt_test::finish("capability_observable_models_flaws_detected");
}
