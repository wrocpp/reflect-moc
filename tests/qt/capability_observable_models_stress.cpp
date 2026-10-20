// One generic model for every Qt container that ContainerTraits describes, driven by thousands of
// random changes. After each change the container, the model, a view mirror built from signals alone,
// the selection and a persistent index must all agree with a plain reference. QAbstractItemModelTester
// runs in fatal mode throughout, so a violation of the model contract aborts the test.
#include "observable_models_support.hpp"

#include <container_model.hpp>

#include <QtCore/QCoreApplication>
#include <QtTest/QAbstractItemModelTester>

#include <cstdio>

namespace {
constexpr int kSteps = 4000;
constexpr unsigned kSeeds[] = {1, 2, 3, 4, 5, 6, 7, 8};
}  // namespace

// The same random changes drive the list model and a table model whose first column is the list model's one.
template <class C>
bool stress(const char* name, bool table = false) {
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  bool ok = true;
  int applied = 0;
  int refused = 0;
  for (const unsigned seed : kSeeds) {
    std::unique_ptr<QAbstractItemModelTester> tester;
    const auto report = observable_test::run_random<C, observable::ObservableContainer<C>>(seed, kSteps, [&](observable::ObservableContainer<C>* source) {
      const auto shown = [](const Key& key, const Value& value) { return QVariant(observable_test::Make<C>::shown(key, value)); };
      std::unique_ptr<QAbstractItemModel> model;
      if (table)
        model = std::make_unique<observable::ContainerTableModel<C>>(
            source, std::vector<observable::Column<C>>{{"value", shown}, {"again", shown}});
      else
        model = std::make_unique<observable::ContainerModel<C>>(source, shown);
      tester = std::make_unique<QAbstractItemModelTester>(model.get(), QAbstractItemModelTester::FailureReportingMode::Fatal);
      return model;
    }, /*stop_at_first_violation=*/false, /*with_proxy=*/true);
    applied += report.applied;
    refused += report.refused;
    if (!report.clean()) {
      ok = false;
      std::fprintf(stderr, "%s seed %u: %d mismatches\n", name, seed, report.mismatches);
      for (const QString& v : report.violations.mid(0, 3)) std::fprintf(stderr, "  %s\n", qPrintable(v));
    }
    tester.reset();
  }
  std::printf("%-22s %-5s %d changes applied, %d invalid requests refused\n", name, table ? "table" : "list", applied, refused);
  return ok;
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  using observable_test::Sample;
  CHECK(stress<QList<Sample>>("QList"));
  CHECK(stress<QQueue<Sample>>("QQueue"));
  CHECK(stress<QStack<Sample>>("QStack"));
  CHECK(stress<QStringList>("QStringList"));
  CHECK((stress<QMap<int, Sample>>("QMap")));
  CHECK((stress<QHash<unsigned, Sample>>("QHash")));
  CHECK(stress<QSet<unsigned>>("QSet"));
  CHECK(stress<QList<Sample>>("QList", true));
  CHECK((stress<QMap<int, Sample>>("QMap", true)));
  CHECK((stress<QHash<unsigned, Sample>>("QHash", true)));
  CHECK(stress<QSet<unsigned>>("QSet", true));
  return rqt_test::finish("capability_observable_models_stress");
}
