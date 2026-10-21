// A gesture on many entries is one event. insertMany, removeMany and takeMany announce the list of keys once;
// the model turns the list into one begin and end pair per run of consecutive rows. resetWith replaces the
// whole content between one pair of signals, and the data pair carries the roles that changed.
//
// The shapes follow the contract Leonardo Rossi described: https://gist.github.com/96Ven96/100c0ec4917dced4d6c8ac9a4cc06b7e
// QAbstractItemModelTester runs in fatal mode in every model test.
#include "check.hpp"
#include "observable_models_support.hpp"

#include <container_model.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPersistentModelIndex>
#include <QtTest/QAbstractItemModelTester>

#include <algorithm>
#include <map>
#include <random>
#include <set>

namespace {
using observable_test::Sample;
using Map = observable::ObservableContainer<QMap<int, Sample>>;
using MapModel = observable::ContainerModel<QMap<int, Sample>>;
using Hash = observable::ObservableContainer<QHash<unsigned, Sample>>;
using HashModel = observable::ContainerModel<QHash<unsigned, Sample>>;
using Set = observable::ObservableContainer<QSet<unsigned>>;
using List = observable::ObservableContainer<QList<Sample>>;
using Multi = observable::ObservableContainer<QMultiMap<int, Sample>>;

constexpr unsigned kSeed = 5;
constexpr int kSteps = 4000;
constexpr int kKeyDomain = 40;
constexpr int kLongestBatch = 8;

QVariant show_map(const int&, const Sample& s) { return s.value; }
QVariant show_hash(const unsigned&, const Sample& s) { return s.value; }

Sample sample(int n) { return Sample{n}; }

QMap<int, Sample> entries(std::initializer_list<int> keys) {
  QMap<int, Sample> out;
  for (const int key : keys) out.insert(key, sample(key * 10));
  return out;
}

// Every signal of a container, with what the container looked like at that moment.
class ContainerLog {
 public:
  explicit ContainerLog(Map& source) : m_source(source) {
    const auto batch = [this](const char* name) {
      return [this, name](const QList<int>& keys) { note(name, join(keys)); };
    };
    connect(&Map::entriesAboutToBeAdded, batch("entriesAboutToBeAdded"));
    connect(&Map::entriesAdded, batch("entriesAdded"));
    connect(&Map::entriesAboutToBeRemoved, batch("entriesAboutToBeRemoved"));
    connect(&Map::entriesRemoved, batch("entriesRemoved"));
    const auto single = [this](const char* name) { return [this, name](const int& key) { note(name, QString::number(key)); }; };
    connect(&Map::entryAboutToBeAdded, single("entryAboutToBeAdded"));
    connect(&Map::entryAdded, single("entryAdded"));
    connect(&Map::entryAboutToBeRemoved, single("entryAboutToBeRemoved"));
    connect(&Map::entryRemoved, single("entryRemoved"));
    const auto changed = [this](const char* name) {
      return [this, name](const int& key, const QList<int>& roles) { note(name, QString::number(key) + " roles=" + join(roles)); };
    };
    connect(&Map::entryAboutToBeChanged, changed("entryAboutToBeChanged"));
    connect(&Map::entryChanged, changed("entryChanged"));
    connect(&Map::aboutToBeCleared, [this] { note("aboutToBeCleared", ""); });
    connect(&Map::cleared, [this] { note("cleared", ""); });
    connect(&Map::aboutToBeReset, [this] { note("aboutToBeReset", ""); });
    connect(&Map::resetCompleted, [this] { note("resetCompleted", ""); });
  }

  QStringList take() { return std::exchange(m_lines, {}); }

 private:
  void connect(auto signal, auto&& handler) { QObject::connect(&m_source, signal, &m_source, handler); }
  static QString join(const QList<int>& values) {
    QStringList parts;
    for (const int v : values) parts.append(QString::number(v));
    return '[' + parts.join(',') + ']';
  }
  // "<signal> <payload> keys=<keys in the container at that moment>"
  void note(const char* name, const QString& payload) {
    m_lines.append(QStringLiteral("%1 %2 now=%3").arg(QLatin1String(name), payload, join(m_source.items().keys())));
  }

  Map& m_source;
  QStringList m_lines;
};

void batches_are_announced_once() {
  Map map(entries({20, 40, 60}));
  ContainerLog log(map);

  // Two new keys and one that is there: one announcement with the two, in the order of the container.
  QMap<int, Sample> batch = entries({10, 40, 30});
  batch.insert(40, sample(-1));
  CHECK(map.insertMany(batch) == 2);
  CHECK(log.take() == QStringList({"entriesAboutToBeAdded [10,30] now=[20,40,60]", "entriesAdded [10,30] now=[10,20,30,40,60]"}));
  CHECK(map.value(10).value == 100);
  CHECK(map.value(40).value == 400);  // a key that was there is skipped, not replaced
}

void removal_ignores_what_is_absent() {
  Map map(entries({1, 2, 3, 4}));
  ContainerLog log(map);

  // A repeated key and an absent key are ignored, and the keys that are left keep the caller's order.
  CHECK(map.removeMany({3, 3, 99, 1}) == 2);
  CHECK(log.take() == QStringList({"entriesAboutToBeRemoved [3,1] now=[1,2,3,4]", "entriesRemoved [3,1] now=[2,4]"}));

  // Nothing left to remove: nothing announced.
  CHECK(map.removeMany({1, 99}) == 0);
  CHECK(map.removeMany({}) == 0);
  CHECK(map.insertMany(entries({2, 4})) == 0);
  CHECK(map.insertMany({}) == 0);
  CHECK(log.take().isEmpty());
}

void a_batch_never_emits_the_single_entry_signals() {
  Map map(entries({1, 2}));
  ContainerLog log(map);
  CHECK(map.insertMany(entries({3, 4})) == 2);
  CHECK(map.removeMany({1, 2}) == 2);
  for (const QString& line : log.take()) {
    CHECK(!line.startsWith("entryAdded") && !line.startsWith("entryRemoved"));
    CHECK(!line.startsWith("entryAboutToBeAdded") && !line.startsWith("entryAboutToBeRemoved"));
  }
}

void takeMany_returns_the_entries_and_moves_between_containers() {
  Map from(entries({1, 2, 3}));
  Map to;
  ContainerLog fromLog(from);
  ContainerLog toLog(to);

  const QMap<int, Sample> none = from.takeMany({77, 78});
  CHECK(none.isEmpty());
  CHECK(fromLog.take().isEmpty());

  CHECK(to.insertMany(from.takeMany({2, 2, 1, 5})) == 2);
  CHECK(from.items().keys() == QList<int>({3}));
  CHECK(to.items().keys() == QList<int>({1, 2}));
  CHECK(to.value(2).value == 20);
  // One pair on each side.
  CHECK(fromLog.take() == QStringList({"entriesAboutToBeRemoved [2,1] now=[1,2,3]", "entriesRemoved [2,1] now=[3]"}));
  CHECK(toLog.take() == QStringList({"entriesAboutToBeAdded [1,2] now=[]", "entriesAdded [1,2] now=[1,2]"}));
}

void a_list_and_a_multi_container_have_no_batches() {
  List list;
  Multi multi;
  CHECK(list.insertMany({}) == 0);
  CHECK(list.removeMany({0}) == 0);
  CHECK(list.takeMany({0}).isEmpty());
  CHECK(multi.removeMany({}) == 0);
  QMultiMap<int, Sample> one;
  one.insert(1, sample(1));
  CHECK(multi.insertMany(one) == 0);
  CHECK(multi.size() == 0);
  Set set;
  CHECK(set.insertMany(QSet<unsigned>{1, 2, 3}) == 3);  // a set is a set of keys, so it can
  CHECK(set.removeMany({2, 2, 9}) == 1);
}

void reset_replaces_everything_in_one_step() {
  Map map(entries({1, 2}));
  ContainerLog log(map);
  map.resetWith(entries({7, 8, 9}));
  CHECK(log.take() == QStringList({"aboutToBeReset  now=[1,2]", "resetCompleted  now=[7,8,9]"}));
  // It always announces, even when both sides are empty, and it is not a clear.
  map.resetWith({});
  map.resetWith({});
  CHECK(log.take() == QStringList({"aboutToBeReset  now=[7,8,9]", "resetCompleted  now=[]",
                                   "aboutToBeReset  now=[]", "resetCompleted  now=[]"}));
}

void the_data_pair_carries_roles() {
  Map map(entries({5}));
  ContainerLog log(map);
  CHECK(map.replace(5, sample(1)));
  CHECK(map.editValue(5, [](Sample& s) { s.value += 1; }, {Qt::DisplayRole, Qt::UserRole + 3}));
  CHECK(map.editValue(5, [](Sample& s) { s.value += 1; }));
  CHECK(log.take() == QStringList({"entryAboutToBeChanged 5 roles=[] now=[5]", "entryChanged 5 roles=[] now=[5]",
                                   "entryAboutToBeChanged 5 roles=[0,259] now=[5]", "entryChanged 5 roles=[0,259] now=[5]",
                                   "entryAboutToBeChanged 5 roles=[] now=[5]", "entryChanged 5 roles=[] now=[5]"}));
  CHECK(map.value(5).value == 3);
  // The about signal sees the old value and the twin the new one.
  int atAbout = -1;
  int atTwin = -1;
  QObject::connect(&map, &Map::entryAboutToBeChanged, &map, [&](const int&, const QList<int>&) { atAbout = map.value(5).value; });
  QObject::connect(&map, &Map::entryChanged, &map, [&](const int&, const QList<int>&) { atTwin = map.value(5).value; });
  CHECK(map.editValue(5, [](Sample& s) { s.value = 40; }));
  CHECK(atAbout == 3 && atTwin == 40);
  // An absent key changes nothing, announces nothing, and the edit does not run.
  bool ran = false;
  CHECK(!map.editValue(6, [&](Sample&) { ran = true; }));
  CHECK(!ran);
  CHECK(log.take().size() == 2);
}

// Listens to a model and writes down every structural signal.
class RowLog {
 public:
  explicit RowLog(QAbstractItemModel& model) : m_model(model) {
    using M = QAbstractItemModel;
    QObject::connect(&model, &M::rowsAboutToBeInserted, &model, [this](const QModelIndex&, int f, int l) { note("aboutToInsert", f, l); });
    QObject::connect(&model, &M::rowsInserted, &model, [this](const QModelIndex&, int f, int l) { note("inserted", f, l); });
    QObject::connect(&model, &M::rowsAboutToBeRemoved, &model, [this](const QModelIndex&, int f, int l) { note("aboutToRemove", f, l); });
    QObject::connect(&model, &M::rowsRemoved, &model, [this](const QModelIndex&, int f, int l) { note("removed", f, l); });
    QObject::connect(&model, &M::modelAboutToBeReset, &model, [this] { note("aboutToReset", m_model.rowCount(), 0); });
    QObject::connect(&model, &M::modelReset, &model, [this] { note("reset", m_model.rowCount(), 0); });
    QObject::connect(&model, &M::dataChanged, &model, [this](const QModelIndex& a, const QModelIndex& b, const QList<int>& roles) {
      QStringList parts;
      for (const int role : roles) parts.append(QString::number(role));
      m_lines.append(QStringLiteral("dataChanged %1-%2 roles=[%3]").arg(a.row()).arg(b.row()).arg(parts.join(',')));
    });
  }
  QStringList take() { return std::exchange(m_lines, {}); }

 private:
  void note(const char* name, int first, int last) {
    m_lines.append(QStringLiteral("%1 %2 %3 count=%4").arg(QLatin1String(name)).arg(first).arg(last).arg(m_model.rowCount()));
  }
  QAbstractItemModel& m_model;
  QStringList m_lines;
};

QList<int> shown(const QAbstractItemModel& model) {
  QList<int> out;
  for (int row = 0; row < model.rowCount(); ++row) out.append(model.index(row, 0).data().toInt());
  return out;
}

// Rows 8, keys 1..8 stored as 10..80. Removing keys 2, 3, 6 and 8 leaves runs [1,2], [5,5] and [7,7]; they are
// announced from the bottom, so each begin names rows that have not moved yet.
void removal_is_one_cycle_per_run_from_the_bottom() {
  Map map(entries({1, 2, 3, 4, 5, 6, 7, 8}));
  MapModel model(&map, show_map);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  RowLog log(model);
  // While a run is announced the container has already dropped the entries, so a view that reads a doomed row
  // gets nothing instead of a value that is no longer there.
  bool doomedRowsReadEmpty = true;
  QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, &model, [&](const QModelIndex&, int first, int last) {
    for (int row = first; row <= last; ++row) doomedRowsReadEmpty = doomedRowsReadEmpty && !model.index(row, 0).data().isValid();
  });
  CHECK(map.removeMany({2, 3, 6, 8}) == 4);
  CHECK(log.take() == QStringList({"aboutToRemove 7 7 count=8", "removed 7 7 count=7", "aboutToRemove 5 5 count=7",
                                   "removed 5 5 count=6", "aboutToRemove 1 2 count=6", "removed 1 2 count=4"}));
  CHECK(shown(model) == QList<int>({10, 40, 50, 70}));
  CHECK(doomedRowsReadEmpty);
}

// The keys may come in any order: the runs are those of the rows, not of the list the caller wrote.
void removal_follows_the_rows_not_the_order_of_the_list() {
  Map map(entries({1, 2, 3, 4, 5, 6, 7, 8}));
  MapModel model(&map, show_map);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  RowLog log(model);
  CHECK(map.removeMany({8, 3, 6, 2}) == 4);
  CHECK(log.take() == QStringList({"aboutToRemove 7 7 count=8", "removed 7 7 count=7", "aboutToRemove 5 5 count=7",
                                   "removed 5 5 count=6", "aboutToRemove 1 2 count=6", "removed 1 2 count=4"}));
  CHECK(shown(model) == QList<int>({10, 40, 50, 70}));
}

// Keys 20, 40, 60, 80 hold the rows 0..3. Adding 10, 30 and 90 gives the final rows 0, 2 and 6 for the new keys
// and the rows 1, 3, 4, 5 for the old ones, so three cycles from the top.
void insertion_is_one_cycle_per_run_from_the_top() {
  Map map(entries({20, 40, 60, 80}));
  MapModel model(&map, show_map);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  RowLog log(model);
  CHECK(map.insertMany(entries({90, 10, 30})) == 3);
  CHECK(log.take() == QStringList({"aboutToInsert 0 0 count=4", "inserted 0 0 count=5", "aboutToInsert 2 2 count=5",
                                   "inserted 2 2 count=6", "aboutToInsert 6 6 count=6", "inserted 6 6 count=7"}));
  CHECK(shown(model) == QList<int>({100, 200, 300, 400, 600, 800, 900}));

  // Consecutive rows are one cycle however many there are: three keys between two old ones.
  Map gap(entries({10, 50}));
  MapModel gapModel(&gap, show_map);
  QAbstractItemModelTester gapTester(&gapModel, QAbstractItemModelTester::FailureReportingMode::Fatal);
  RowLog gapLog(gapModel);
  CHECK(gap.insertMany(entries({20, 30, 40})) == 3);
  CHECK(gapLog.take() == QStringList({"aboutToInsert 1 3 count=2", "inserted 1 3 count=5"}));
}

void a_hash_batch_is_one_run_at_the_end() {
  Hash hash;
  HashModel model(&hash, show_hash);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  QHash<unsigned, Sample> first;
  for (unsigned key = 1; key <= 4; ++key) first.insert(key, Sample{static_cast<int>(key)});
  CHECK(hash.insertMany(first) == 4);
  RowLog log(model);
  QHash<unsigned, Sample> more;
  for (unsigned key = 10; key <= 12; ++key) more.insert(key, Sample{static_cast<int>(key)});
  CHECK(hash.insertMany(more) == 3);
  CHECK(log.take() == QStringList({"aboutToInsert 4 6 count=4", "inserted 4 6 count=7"}));
  const QList<int> before = shown(model);
  CHECK(hash.removeMany({2, 3}) == 2);
  int removedRows = 0;
  for (const QString& line : log.take())
    if (line.startsWith("removed ")) removedRows += line.split(' ')[2].toInt() - line.split(' ')[1].toInt() + 1;
  CHECK(removedRows == 2);  // however the two rows lie, they are announced as the rows they are
  CHECK(model.rowCount() == 5);
  CHECK(before.size() == 7);
}

// A batch keeps the selection and a persistent index of the rows that stay; a reset drops them.
void a_batch_keeps_the_selection_a_reset_does_not() {
  Map map(entries({1, 2, 3, 4, 5, 6}));
  MapModel model(&map, show_map);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  QItemSelectionModel selection(&model);
  selection.select(model.index(4, 0), QItemSelectionModel::Select);  // key 5
  const QPersistentModelIndex tracked = model.index(4, 0);
  CHECK(map.removeMany({1, 2, 3}) == 3);
  CHECK(tracked.isValid() && tracked.row() == 1 && tracked.data().toInt() == 50);
  CHECK(selection.isRowSelected(1));
  CHECK(selection.selectedRows().size() == 1);
  CHECK(map.insertMany(entries({0, 1})) == 2);
  CHECK(tracked.isValid() && tracked.row() == 3 && tracked.data().toInt() == 50);
  CHECK(selection.isRowSelected(3));

  map.resetWith(entries({5}));
  CHECK(!tracked.isValid());
  CHECK(selection.selectedRows().isEmpty());
}

void a_reset_is_one_reset_and_the_counts_follow() {
  Map map(entries({1, 2}));
  MapModel model(&map, show_map);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  RowLog log(model);
  map.resetWith(entries({7, 8, 9}));
  // At the begin the model still has its old two rows, at the end its three new ones, and nothing else is announced.
  CHECK(log.take() == QStringList({"aboutToReset 2 0 count=2", "reset 3 0 count=3"}));
  CHECK(shown(model) == QList<int>({70, 80, 90}));
  CHECK(model.index(0, 0).data(MapModel::KeyRole).toInt() == 7);
  map.resetWith({});
  CHECK(log.take() == QStringList({"aboutToReset 3 0 count=3", "reset 0 0 count=0"}));
  map.resetWith({});
  CHECK(log.take() == QStringList({"aboutToReset 0 0 count=0", "reset 0 0 count=0"}));
  CHECK(map.insert(1, sample(1)));  // the model still works afterwards
  CHECK(shown(model) == QList<int>({1}));
}

void roles_reach_the_view_unchanged() {
  Map map(entries({1, 2, 3}));
  MapModel model(&map, show_map);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  RowLog log(model);
  CHECK(map.editValue(2, [](Sample& s) { s.value = 1; }, {Qt::DisplayRole}));
  CHECK(map.replace(3, sample(2)));
  CHECK(map.editValue(1, [](Sample& s) { s.value = 3; }, {Qt::ForegroundRole, Qt::UserRole + 1}));
  CHECK(log.take() == QStringList({"dataChanged 1-1 roles=[0]", "dataChanged 2-2 roles=[]", "dataChanged 0-0 roles=[9,257]"}));
}

// Random batches against a plain reference, with the tester fatal and a mirror built from signals only.
void random_batches_follow_a_reference() {
  Map map;
  MapModel model(&map, show_map);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  observable_test::ModelWatcher watcher(&model, true);
  QItemSelectionModel selection(&model);
  std::map<int, int> shadow;
  std::mt19937 rng(kSeed);
  int value = 1;
  int batches = 0;
  int resets = 0;
  const auto draw = [&](int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); };

  for (int step = 0; step < kSteps; ++step) {
    const int kind = draw(10);
    const int length = 1 + draw(kLongestBatch);
    if (kind < 4) {
      QMap<int, Sample> batch;
      std::size_t fresh = 0;
      for (int i = 0; i < length; ++i) {
        const int key = draw(kKeyDomain);
        batch.insert(key, sample(value + i));
      }
      for (auto it = batch.cbegin(); it != batch.cend(); ++it)
        if (shadow.insert({it.key(), it.value().value}).second) ++fresh;
      CHECK(static_cast<std::size_t>(map.insertMany(batch)) == fresh);
      value += length;
      batches += fresh > 0;
    } else if (kind < 8) {
      QList<int> keys;
      std::size_t present = 0;
      std::set<int> seen;
      for (int i = 0; i < length; ++i) {
        const int key = draw(kKeyDomain);
        keys.append(key);
        if (shadow.count(key) && seen.insert(key).second) ++present;
      }
      for (const int key : seen) shadow.erase(key);
      CHECK(static_cast<std::size_t>(map.removeMany(keys)) == present);
      batches += present > 0;
    } else if (kind == 8) {
      const int row = draw(std::max<int>(1, model.rowCount()));
      if (model.rowCount() > 0) selection.select(model.index(row, 0), QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
    } else {
      QMap<int, Sample> content;
      shadow.clear();
      for (int i = 0; i < length; ++i) {
        const int key = draw(kKeyDomain);
        content.insert(key, sample(value + i));
        shadow[key] = value + i;
      }
      value += length;
      map.resetWith(content);
      ++resets;
    }
    watcher.check();
    CHECK(model.rowCount() == static_cast<int>(shadow.size()));
    int row = 0;
    for (const auto& [key, expected] : shadow) {
      if (row >= model.rowCount()) break;
      CHECK(model.index(row, 0).data(MapModel::KeyRole).toInt() == key);
      CHECK(model.index(row, 0).data().toInt() == expected);
      ++row;
    }
  }
  CHECK(watcher.clean());
  CHECK(batches > kSteps / 4);
  CHECK(resets > kSteps / 20);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  batches_are_announced_once();
  removal_ignores_what_is_absent();
  a_batch_never_emits_the_single_entry_signals();
  takeMany_returns_the_entries_and_moves_between_containers();
  a_list_and_a_multi_container_have_no_batches();
  reset_replaces_everything_in_one_step();
  the_data_pair_carries_roles();
  removal_is_one_cycle_per_run_from_the_bottom();
  removal_follows_the_rows_not_the_order_of_the_list();
  insertion_is_one_cycle_per_run_from_the_top();
  a_hash_batch_is_one_run_at_the_end();
  a_batch_keeps_the_selection_a_reset_does_not();
  a_reset_is_one_reset_and_the_counts_follow();
  roles_reach_the_view_unchanged();
  random_batches_follow_a_reference();
  return rqt_test::finish("capability_observable_batches");
}
