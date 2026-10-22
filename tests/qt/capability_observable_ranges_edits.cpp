// Range changes of a list (one begin and one end however long the run), editing through setData, a table model,
// and the rule that a container and its models live in one thread. QAbstractItemModelTester runs in fatal mode.
#include "check.hpp"
#include "observable_models_support.hpp"

#include <container_model.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtTest/QAbstractItemModelTester>

#include <sys/wait.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#include <unistd.h>

#include <csignal>
#include <random>

namespace {
using observable_test::Sample;
using Listed = observable::ObservableContainer<QList<Sample>>;
using ListModel = observable::ContainerModel<QList<Sample>>;
using Hashed = observable::ObservableContainer<QHash<unsigned, Sample>>;

constexpr unsigned kSeed = 11;
constexpr int kSteps = 3000;
constexpr int kLongestRun = 6;
constexpr int kEdited = 77;

QVariant show(const qsizetype&, const Sample& s) { return s.value; }
QVariant show_hashed(const unsigned&, const Sample& s) { return s.value; }

std::optional<Sample> parse(const Sample&, const QVariant& edited) {
  bool ok = false;
  const int value = edited.toInt(&ok);
  if (!ok) return std::nullopt;
  return Sample{value};
}

void ranges_follow_a_vector() {
  Listed list;
  ListModel model(&list, show);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  observable_test::ModelWatcher watcher(&model, false);
  std::vector<int> shadow;
  std::mt19937 rng(kSeed);
  int value = 1;
  int acceptedRuns = 0;
  int announced = 0;
  QObject::connect(&model, &QAbstractItemModel::rowsInserted, &model, [&](const QModelIndex&, int, int) { ++announced; });
  QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &model, [&](const QModelIndex&, int, int) { ++announced; });

  for (int step = 0; step < kSteps; ++step) {
    const int size = static_cast<int>(shadow.size());
    const int first = static_cast<int>(rng() % static_cast<unsigned>(size + 3)) - 1;  // sometimes out of range
    const int length = 1 + static_cast<int>(rng() % kLongestRun);
    if (rng() % 2) {
      QList<Sample> run;
      for (int i = 0; i < length; ++i) run.append(Sample{value + i});
      const bool valid = first >= 0 && first <= size;
      const bool accepted = list.insertRange(first, run);
      CHECK(accepted == valid);
      if (valid) {
        for (int i = 0; i < length; ++i) shadow.insert(shadow.begin() + first + i, value + i);
        value += length;
        ++acceptedRuns;
      }
    } else {
      const bool valid = first >= 0 && first + length <= size;
      const bool accepted = list.removeRange(first, length);
      CHECK(accepted == valid);
      if (valid) {
        shadow.erase(shadow.begin() + first, shadow.begin() + first + length);
        ++acceptedRuns;
      }
    }
    watcher.check();
    CHECK(model.rowCount() == static_cast<int>(shadow.size()));
    for (int row = 0; row < model.rowCount(); ++row) CHECK(model.index(row).data().toInt() == shadow[row]);
  }
  CHECK(watcher.clean());
  CHECK(announced == acceptedRuns);  // one end call per run, however long
  CHECK(acceptedRuns > kSteps / 4);
}

void ranges_need_a_list() {
  Hashed hashed;
  observable::ContainerModel<QHash<unsigned, Sample>> model(&hashed, show_hashed);
  int announcements = 0;
  QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &model, [&](const QModelIndex&, int, int) { ++announcements; });
  CHECK(!hashed.insertRange(0, {Sample{1}, Sample{2}}));
  CHECK(!hashed.removeRange(0, 1));
  CHECK(announcements == 0 && model.rowCount() == 0);
}

void a_range_is_one_change() {
  Listed list;
  ListModel model(&list, show);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  int begins = 0, ends = 0, firstRow = -1, lastRow = -1;
  QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &model, [&](const QModelIndex&, int first, int last) {
    ++begins;
    firstRow = first;
    lastRow = last;
  });
  QObject::connect(&model, &QAbstractItemModel::rowsInserted, &model, [&](const QModelIndex&, int, int) { ++ends; });
  CHECK(list.insertRange(0, {Sample{1}, Sample{2}, Sample{3}, Sample{4}}));
  CHECK(begins == 1 && ends == 1 && firstRow == 0 && lastRow == 3);
  CHECK(!list.insertRange(0, {}));        // an empty run announces nothing
  CHECK(!list.removeRange(0, 0));
  CHECK(!list.removeRange(3, 2));         // past the end
  CHECK(begins == 1 && ends == 1);
}

void editing_goes_through_the_container() {
  Listed list;
  ListModel editable(&list, show, parse);
  QAbstractItemModelTester tester(&editable, QAbstractItemModelTester::FailureReportingMode::Fatal);
  list.insert(0, Sample{1});
  list.insert(1, Sample{2});
  int changes = 0;
  QObject::connect(&editable, &QAbstractItemModel::dataChanged, &editable, [&](const QModelIndex&, const QModelIndex&) { ++changes; });

  const QModelIndex second = editable.index(1);
  CHECK(editable.flags(second) & Qt::ItemIsEditable);
  CHECK(editable.setData(second, kEdited));
  CHECK(editable.data(second).toInt() == kEdited && list.value(1).value == kEdited);
  CHECK(changes == 1);
  CHECK(!editable.setData(second, "not a number"));           // the editor refuses it
  CHECK(!editable.setData(second, kEdited, Qt::DisplayRole));  // only the edit role is writable
  CHECK(!editable.setData(editable.index(5), kEdited));        // no such row
  CHECK(editable.data(second).toInt() == kEdited && changes == 1);

  ListModel readOnly(&list, show);
  CHECK(!(readOnly.flags(readOnly.index(0)) & Qt::ItemIsEditable));
  CHECK(!readOnly.setData(readOnly.index(0), kEdited));
  CHECK(list.value(0).value == 1);
}

void a_hash_edit_reaches_the_right_key() {
  Hashed hashed;
  observable::ContainerModel<QHash<unsigned, Sample>> model(&hashed, show_hashed, parse);
  hashed.insert(30, Sample{3});
  hashed.insert(10, Sample{1});
  hashed.insert(20, Sample{2});
  hashed.remove(30);
  // rows are now 10, 20 in the order they were added; row 1 is key 20
  CHECK(model.setData(model.index(1), kEdited));
  CHECK(hashed.value(20).value == kEdited && hashed.value(10).value == 1);
}

void a_table_model_has_columns() {
  Listed list;
  std::vector<observable::Column<QList<Sample>>> columns;
  columns.push_back({"value", show, parse});
  columns.push_back({"double", [](const qsizetype&, const Sample& s) { return QVariant(2 * s.value); }});
  observable::ContainerTableModel<QList<Sample>> table(&list, columns);
  QAbstractItemModelTester tester(&table, QAbstractItemModelTester::FailureReportingMode::Fatal);
  list.insertRange(0, {Sample{1}, Sample{2}});
  CHECK(table.columnCount() == 2 && table.rowCount() == 2);
  CHECK(table.headerData(0, Qt::Horizontal).toString() == "value" && table.headerData(1, Qt::Horizontal).toString() == "double");
  CHECK(table.index(1, 0).data().toInt() == 2 && table.index(1, 1).data().toInt() == 4);
  CHECK(table.flags(table.index(0, 0)) & Qt::ItemIsEditable);
  CHECK(!(table.flags(table.index(0, 1)) & Qt::ItemIsEditable));  // that column has no editor
  int changes = 0;
  int lastColumn = -1;
  QObject::connect(&table, &QAbstractItemModel::dataChanged, &table, [&](const QModelIndex&, const QModelIndex& bottom) {
    ++changes;
    lastColumn = bottom.column();
  });
  CHECK(table.setData(table.index(0, 0), kEdited));
  CHECK(table.index(0, 1).data().toInt() == 2 * kEdited);
  CHECK(changes == 1 && lastColumn == 1);  // one signal covers the whole row
  CHECK(!table.setData(table.index(0, 1), kEdited));
}

#ifndef QT_NO_DEBUG
// Runs `scenario` in a child process and reports whether it died of an assertion (SIGABRT).
template <class F>
bool aborts(F&& scenario) {
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
#if defined(__linux__)
    prctl(PR_SET_DUMPABLE, 0);  // a core dump to a piped handler (systemd-coredump on a runner) takes seconds per abort
#endif
    if (!std::freopen("/dev/null", "w", stderr)) _exit(2);
    scenario();
    _exit(0);
  }
  int status = 0;
  waitpid(child, &status, 0);
  return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

void one_thread_rule() {
  CHECK(!aborts([] {  // the control: same thread, no assertion
    Listed list;
    ListModel model(&list, show);
    list.insert(0, Sample{1});
  }));
  CHECK(aborts([] {  // a model made on another thread than its container
    Listed list;
    QThread other;
    list.moveToThread(&other);
    ListModel model(&list, show);
  }));
  CHECK(aborts([] {  // a container changed from a thread it does not live in
    Listed list;
    QThread worker;
    QObject::connect(&worker, &QThread::started, &worker, [&] { list.insert(0, Sample{1}); }, Qt::DirectConnection);
    worker.start();
    worker.wait();
  }));
}
#endif
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  ranges_follow_a_vector();
  ranges_need_a_list();
  a_range_is_one_change();
  editing_goes_through_the_container();
  a_hash_edit_reaches_the_right_key();
  a_table_model_has_columns();
#ifndef QT_NO_DEBUG
  one_thread_rule();
#endif
  return rqt_test::finish("capability_observable_ranges_edits");
}
