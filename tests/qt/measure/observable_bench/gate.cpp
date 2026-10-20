// The correctness gate for the QVariant twin (examples/qt/observable_models_qvariant). A baseline that is
// timed must first be shown to keep the model contract, so this runs the same seeded random driver, view
// mirror, selection and persistent-index checks that examples/qt/observable_models passes, against the twin.
#include "observable_models_support.hpp"

#include <container_model.h>

#include <QtCore/QCoreApplication>
#include <QtTest/QAbstractItemModelTester>

#include <cstdio>

namespace {
constexpr int kSteps = 4000;
constexpr unsigned kSeeds[] = {1, 2, 3, 4, 5, 6, 7, 8};
}  // namespace

template <class C>
bool gate(const char* name) {
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  using Source = observable_qv::ObservableContainer<C>;
  bool ok = true;
  int applied = 0;
  for (const unsigned seed : kSeeds) {
    std::unique_ptr<QAbstractItemModelTester> tester;
    const auto report = observable_test::run_random<C, Source>(seed, kSteps, [&](Source* source) {
      auto model = std::make_unique<observable_qv::ContainerModel<C>>(
          source, [](const Key& key, const Value& value) { return QVariant(observable_test::Make<C>::shown(key, value)); });
      tester = std::make_unique<QAbstractItemModelTester>(model.get(), QAbstractItemModelTester::FailureReportingMode::Fatal);
      return model;
    });
    applied += report.applied;
    if (!report.clean()) {
      ok = false;
      std::fprintf(stderr, "%s seed %u: %d mismatches\n", name, seed, report.mismatches);
      for (const QString& v : report.violations.mid(0, 3)) std::fprintf(stderr, "  %s\n", qPrintable(v));
    }
    tester.reset();
  }
  std::printf("%-22s %d changes applied\n", name, applied);
  return ok;
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  using observable_test::Sample;
  CHECK(gate<QList<Sample>>("QList"));
  CHECK(gate<QQueue<Sample>>("QQueue"));
  CHECK(gate<QStack<Sample>>("QStack"));
  CHECK(gate<QStringList>("QStringList"));
  CHECK((gate<QMap<int, Sample>>("QMap")));
  CHECK((gate<QHash<unsigned, Sample>>("QHash")));
  CHECK(gate<QSet<unsigned>>("QSet"));
  return rqt_test::finish("observable_bench_gate");
}
