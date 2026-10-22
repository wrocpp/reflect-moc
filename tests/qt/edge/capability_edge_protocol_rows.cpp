// Model protocol, part 3: the life of the rows. Insert into an empty container, remove the last row, clear and reset
// (with the same content, with none, on an empty container), what persistent indexes and a selection do through each,
// and whether rowCount() and index() agree at every moment of a change, a batch included. QAbstractItemModelTester
// runs in fatal mode in every fixture. Catalogue rows: docs/qt-edge-cases/protocol.md (P-14 to P-26).
#include "protocol_support.hpp"

namespace {
using namespace edge;

// The row count a receiver sees at each signal named, in the order the signals came.
class CountAt : public QObject {
 public:
  explicit CountAt(QAbstractItemModel* model) : QObject(model) {
    using M = QAbstractItemModel;
    const auto watch = [this, model](auto signal) { connect(model, signal, this, [this, model] { m_counts.push_back(model->rowCount()); }); };
    watch(&M::rowsAboutToBeInserted);
    watch(&M::rowsInserted);
    watch(&M::rowsAboutToBeRemoved);
    watch(&M::rowsRemoved);
    watch(&M::modelAboutToBeReset);
    watch(&M::modelReset);
  }
  const std::vector<int>& counts() const { return m_counts; }

 private:
  std::vector<int> m_counts;
};

int position_of(const std::vector<int>& rows, int value) {
  return static_cast<int>(std::find(rows.begin(), rows.end(), value) - rows.begin());
}

// P-14
void insert_into_an_empty_container() {
  for_each_model([](auto& fx, const std::string& kind) {
    SignalLog log(fx.model.get());
    CountAt counts(fx.model.get());
    KCHECK(!fx.model->hasChildren() && fx.rows() == 0);
    fx.add();
    KCHECK(log.take() == QStringList({"rowsAboutToBeInserted 0 0", "rowsInserted 0 0"}));
    KCHECK(counts.counts() == std::vector<int>({0, 1}));  // the count moves between the two signals, not before
    KCHECK(fx.rows() == 1 && fx.shown_at(0) == 1);
    KCHECK(fx.model->hasChildren() && fx.index(0).isValid() && !fx.index(1).isValid());
  });
}

// P-15
void remove_the_last_row() {
  for_each_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    SignalLog log(fx.model.get());
    CountAt counts(fx.model.get());
    KCHECK(fx.remove(kSmallFill));  // the entry at the last row, in every kind
    KCHECK(log.take() == QStringList({"rowsAboutToBeRemoved 4 4", "rowsRemoved 4 4"}));
    KCHECK(counts.counts() == std::vector<int>({kSmallFill, kSmallFill - 1}));
    KCHECK(fx.shows({1, 2, 3, 4}) && !fx.index(4).isValid());
    for (int n = 1; n < kSmallFill; ++n) KCHECK(fx.remove(n));
    KCHECK(fx.rows() == 0 && !fx.index(0).isValid() && !fx.model->hasChildren());
    KCHECK(!fx.remove(1));  // and the container is empty
  });
}

// P-16
void clear_of_an_empty_container_is_silent() {
  for_each_model([](auto& fx, const std::string& kind) {
    SignalLog log(fx.model.get());
    KCHECK(!fx.source.clear());
    KCHECK(log.silent());
    fx.add();
    fx.remove(1);
    log.take();
    KCHECK(!fx.source.clear());  // empty again, after the container has held something
    KCHECK(log.silent());
  });
}

// P-17
void clear_resets_the_model_and_invalidates_what_pointed_into_it() {
  for_each_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    std::vector<QPersistentModelIndex> tracked;
    for (int row = 0; row < kSmallFill; ++row) tracked.emplace_back(fx.index(row));
    QItemSelectionModel selection(fx.model.get());
    selection.select(fx.index(1), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    selection.setCurrentIndex(fx.index(2), QItemSelectionModel::NoUpdate);
    SignalLog log(fx.model.get());
    CountAt counts(fx.model.get());
    KCHECK(fx.source.clear());
    KCHECK(log.take() == QStringList({"modelAboutToBeReset", "modelReset"}));
    KCHECK(counts.counts() == std::vector<int>({kSmallFill, 0}));
    KCHECK(fx.rows() == 0 && !fx.index(0).isValid());
    for (const QPersistentModelIndex& p : tracked) KCHECK(!p.isValid());
    KCHECK(!selection.hasSelection() && !selection.currentIndex().isValid() && selection.selectedIndexes().isEmpty());
  });
}

// P-18
void reset_with_the_same_content_is_still_a_reset() {
  for_each_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    QPersistentModelIndex tracked(fx.index(2));
    QItemSelectionModel selection(fx.model.get());
    selection.select(fx.index(2), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    SignalLog log(fx.model.get());
    const typename std::remove_cvref_t<decltype(fx)>::Container same = fx.source.items();
    fx.source.resetWith(same);
    KCHECK(log.take() == QStringList({"modelAboutToBeReset", "modelReset"}));
    KCHECK(fx.shows_as_set({1, 2, 3, 4, 5}));
    KCHECK(!tracked.isValid() && !selection.hasSelection());  // a reset drops every identity, equal content or not
  });
}

// P-19
void reset_with_nothing_on_an_empty_container_announces_once() {
  for_each_model([](auto& fx, const std::string& kind) {
    SignalLog log(fx.model.get());
    fx.source.resetWith(fx.content({}));
    KCHECK(log.take() == QStringList({"modelAboutToBeReset", "modelReset"}));  // documented: it always announces
    KCHECK(fx.rows() == 0);
    fx.fill(2);
    log.take();
    fx.source.resetWith(fx.content({}));
    KCHECK(log.take() == QStringList({"modelAboutToBeReset", "modelReset"}));
    KCHECK(fx.rows() == 0 && !fx.index(0).isValid());
  });
}

// P-20
void row_count_is_the_old_one_when_a_reset_begins_and_the_new_one_when_it_ends() {
  for_each_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    CountAt counts(fx.model.get());
    fx.source.resetWith(fx.content({7, 8}));
    KCHECK(counts.counts() == std::vector<int>({kSmallFill, 2}));
    fx.source.resetWith(fx.content({7, 8, 9}));
    KCHECK(fx.rows() == 3 && fx.shows_as_set({7, 8, 9}));
  });
}

// P-21
void a_removed_row_stays_invalid_when_the_same_entry_returns() {
  for_each_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    QPersistentModelIndex tracked(fx.index(position_of(fx.shown(), 3)));
    KCHECK(fx.remove(3));
    KCHECK(!tracked.isValid());
    fx.add_named(3);
    KCHECK(!tracked.isValid());  // a persistent index does not find its way back
    KCHECK(fx.rows() == kSmallFill && position_of(fx.shown(), 3) < kSmallFill);
  });
}

// P-22
void persistent_index_and_selection_shift_with_an_insert_and_a_remove() {
  for_each_model([](auto& fx, const std::string& kind) {
    using Fx = std::remove_cvref_t<decltype(fx)>;
    fx.fill(kSmallFill);
    QPersistentModelIndex tracked(fx.index(position_of(fx.shown(), 3)));
    QItemSelectionModel selection(fx.model.get());
    selection.select(tracked, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    const auto follows = [&] {
      const int row = position_of(fx.shown(), 3);
      return tracked.isValid() && tracked.row() == row && tracked.data().toInt() == 3 && selection.isRowSelected(row) &&
             selection.selectedRows().size() == 1;
    };
    KCHECK(follows());
    if constexpr (Fx::indexed) {
      KCHECK(fx.source.insert(0, Sample{0}));  // above it
    } else {
      fx.add_named(0);                         // above it in a map, and after it in a hash
    }
    KCHECK(follows());
    KCHECK(fx.remove(1));                      // above it
    KCHECK(follows());
    KCHECK(fx.remove(5));                      // below it
    KCHECK(follows());
    KCHECK(fx.remove(3));
    KCHECK(!tracked.isValid() && !selection.hasSelection());
  });
}

template <class Fx>
void changes_of_every_kind(Fx& fx) {
  fx.fill(kSmallFill);
  fx.remove(2);
  if constexpr (Fx::indexed) {
    fx.source.insertRange(1, {Sample{40}, Sample{41}, Sample{42}});
    fx.source.removeRange(0, 2);
    fx.source.move(0, fx.source.size() - 1);
    fx.source.move(2, 1);
  }
  if constexpr (Fx::keyed) {
    const int first = fx.next;
    fx.source.insertMany(Fx::content({first + 7, first + 1, first + 2, first + 5, first + 3}));
    fx.next = first + 8;
    fx.source.removeMany({Fx::named(first + 1), Fx::named(first + 2), Fx::named(first + 5), Fx::named(1), Fx::named(5)});
  }
  fx.source.resetWith(fx.content({7, 8, 9}));
  fx.add();
  fx.source.clear();
}

// P-23
void row_count_matches_the_rows_index_reaches_at_every_signal() {
  for_each_model([](auto& fx, const std::string& kind) {
    RowProbe probe(fx.model.get());
    observable_test::ModelWatcher watcher(fx.model.get(), false);
    changes_of_every_kind(fx);
    watcher.check();
    KCHECK(probe.looks() > 0);
    KCHECK(probe.disagreements() == 0);
    KCHECK(watcher.clean());
  });
}

template <class Fx>
void insert_batches(Fx& fx) {
  fx.fill(kSmallFill);
  if constexpr (Fx::indexed) {
    fx.source.insertRange(2, {Sample{40}, Sample{41}});
    fx.source.insertRange(0, {Sample{42}});
    fx.source.insertRange(fx.source.size(), {Sample{43}, Sample{44}});
  }
  if constexpr (Fx::keyed) {
    const int first = fx.next;
    fx.source.insertMany(Fx::content({first + 7, first + 1, first + 2, first + 5, first + 4}));  // scattered, in runs
    fx.next = first + 8;
  }
}

// P-24
void rows_stay_readable_while_a_batch_inserts() {
  for_each_model([](auto& fx, const std::string& kind) {
    RowProbe probe(fx.model.get());
    insert_batches(fx);
    KCHECK(probe.looks() > 0);
    KCHECK(probe.disagreements() == 0 && probe.blank_reads() == 0);
  });
}

// P-26
void a_hash_index_that_compacts_its_slots_keeps_the_rows() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        constexpr int kRemoved = 70;  // the index compacts once it has more than 64 slots and over half of them are dead
        fx.fill(kSlotCompaction);
        QPersistentModelIndex last(fx.index(kSlotCompaction - 1));
        for (int n = 1; n <= kRemoved; ++n) KCHECK(fx.remove(n));
        KCHECK(fx.rows() == kSlotCompaction - kRemoved && last.isValid() && last.row() == fx.rows() - 1 && last.data().toInt() == kSlotCompaction);
        fx.fill(kSmallFill);
        std::vector<int> want;
        for (int n = kRemoved + 1; n <= kSlotCompaction + kSmallFill; ++n) want.push_back(n);
        KCHECK(fx.shown() == want);
        for (int row = 0; row < fx.rows(); ++row) KCHECK(fx.shown_at(row) == want[static_cast<std::size_t>(row)]);
        QAbstractItemModelTester tester(fx.model.get(), QAbstractItemModelTester::FailureReportingMode::Fatal);
      },
      false, 0, /*tested=*/false);
}

// P-27: two entries of one key, and the order the newest one takes
template <class Fx>
void duplicate_keys(Fx& fx, const std::string& kind) {
  using Key = typename Fx::Key;
  SignalLog log(fx.model.get());
  KCHECK(fx.source.insert(Key{5, 0}, Sample{50}));
  KCHECK(fx.source.insert(Key{5, 0}, Sample{51}));
  KCHECK(fx.source.insert(Key{3, 0}, Sample{30}));
  KCHECK(fx.source.insert(Key{5, 0}, Sample{52}));
  if constexpr (Fx::sorted) {  // by key, the newest of a key first
    KCHECK(fx.shows({30, 52, 51, 50}));
    KCHECK(log.has("rowsInserted 0 0"));
    log.take();
    KCHECK(fx.source.remove(Key{5, 1}));  // the second newest
    KCHECK(log.take() == QStringList({"rowsAboutToBeRemoved 2 2", "rowsRemoved 2 2"}));
    KCHECK(fx.shows({30, 52, 50}));
  } else {                     // in the order added
    KCHECK(fx.shows({50, 51, 30, 52}));
    log.take();
    KCHECK(fx.source.remove(Key{5, 1}));
    KCHECK(log.take() == QStringList({"rowsAboutToBeRemoved 1 1", "rowsRemoved 1 1"}));
    KCHECK(fx.shows({50, 30, 52}));
  }
  KCHECK(fx.source.replace(Key{5, 0}, Sample{kEdited}));  // the newest
  KCHECK(fx.shows_as_set({30, kEdited, 50}));
  KCHECK(fx.shown_at(Fx::sorted ? 1 : 2) == kEdited);
  KCHECK(!fx.source.remove(Key{5, 2}) && !fx.source.remove(Key{4, 0}));  // not there
}

void entries_of_one_key_in_a_multi_container() {
  Fixture<QMultiMap<int, Sample>, false> map_list;
  duplicate_keys(map_list, "QMultiMap list");
  Fixture<QMultiMap<int, Sample>, true> map_table;
  duplicate_keys(map_table, "QMultiMap table");
  Fixture<QMultiHash<unsigned, Sample>, false> hash_list;
  duplicate_keys(hash_list, "QMultiHash list");
  Fixture<QMultiHash<unsigned, Sample>, true> hash_table;
  duplicate_keys(hash_table, "QMultiHash table");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  insert_into_an_empty_container();
  remove_the_last_row();
  clear_of_an_empty_container_is_silent();
  clear_resets_the_model_and_invalidates_what_pointed_into_it();
  reset_with_the_same_content_is_still_a_reset();
  reset_with_nothing_on_an_empty_container_announces_once();
  row_count_is_the_old_one_when_a_reset_begins_and_the_new_one_when_it_ends();
  a_removed_row_stays_invalid_when_the_same_entry_returns();
  persistent_index_and_selection_shift_with_an_insert_and_a_remove();
  row_count_matches_the_rows_index_reaches_at_every_signal();
  rows_stay_readable_while_a_batch_inserts();
  a_hash_index_that_compacts_its_slots_keeps_the_rows();
  entries_of_one_key_in_a_multi_container();
  return rqt_test::finish("capability_edge_protocol_rows");
}
