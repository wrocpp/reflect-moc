// Drives the container, its model and the store, and prints what a view would see. The same file is built
// against the moc version (original/) and the reflect-moc version (ported/); their output must be identical.
#include "container_model.h"
#include "sample_store.h"

#include <QtCore/QCoreApplication>
#include <QtTest/QAbstractItemModelTester>

#include <cstdio>
#include <vector>

namespace {
constexpr int kFirst = 10;
constexpr int kSecond = 30;
constexpr int kBetween = 20;
constexpr quint32 kKey = 42;
constexpr int kStoredValue = 7;
constexpr int kUnit = 2;
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  SampleList list;
  ContainerModel model(&list);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  std::vector<int> aboutToInsert;
  int inserted = 0;
  QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &model,
                   [&aboutToInsert](const QModelIndex&, int first, int) { aboutToInsert.push_back(first); });
  QObject::connect(&model, &QAbstractItemModel::rowsInserted, &model,
                   [&inserted](const QModelIndex&, int, int) { ++inserted; });

  list.insert(0, Sample{kFirst});
  list.insert(1, Sample{kSecond});
  list.insert(1, Sample{kBetween});

  std::printf("rows %d\n", model.rowCount());
  for (int row = 0; row < model.rowCount(); ++row)
    std::printf("row %d = %d\n", row, model.index(row).data().toInt());
  std::printf("rowsAboutToBeInserted %zu, rowsInserted %d\n", aboutToInsert.size(), inserted);
  for (std::size_t i = 0; i < aboutToInsert.size(); ++i) std::printf("insert %zu at row %d\n", i, aboutToInsert[i]);

  SampleStore store;
  int unitChanges = 0;
  QObject::connect(&store, &SampleStore::unitChanged, &store, [&unitChanges] { ++unitChanges; });
  store.insert(kKey, Sample{kStoredValue});
  store.setUnit(kUnit);
  store.setUnit(kUnit);
  std::printf("store size %lld, value %d, unit %d, unitChanged %d\n", static_cast<long long>(store.size()),
              store.value(kKey).value, store.unit(), unitChanges);
  return 0;
}
