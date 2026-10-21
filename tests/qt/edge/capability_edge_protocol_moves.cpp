// Model protocol, part 2: moves. A QList is the one container with an order to move in; the others refuse a move.
// beginMoveRows takes the row the block is put BEFORE, so a move down counts one further. QAbstractItemModelTester
// runs in fatal mode in every fixture. Catalogue rows: docs/qt-edge-cases/protocol.md (P-05 to P-13).
#include "protocol_support.hpp"

#include <QtCore/QItemSelectionModel>

namespace {
using namespace edge;

QString moved_lines(int source, int destination) {
  return QStringLiteral("rowsAboutToBeMoved %1 %1 %2").arg(source).arg(destination);
}
QString done_lines(int source, int destination) {
  return QStringLiteral("rowsMoved %1 %1 %2").arg(source).arg(destination);
}
int destination_of(int from, int to) { return to > from ? to + 1 : to; }

std::vector<int> after_move(std::vector<int> rows, int from, int to) {
  const int value = rows[static_cast<std::size_t>(from)];
  rows.erase(rows.begin() + from);
  rows.insert(rows.begin() + to, value);
  return rows;
}

// P-05
void move_to_the_same_row_is_refused() {
  for_each_list_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    SignalLog log(fx.model.get());
    for (int row = 0; row < kSmallFill; ++row) KCHECK(!fx.source.move(row, row));
    KCHECK(log.silent());
    KCHECK(fx.shows({1, 2, 3, 4, 5}));
  });
}

// P-06
void move_one_row_down_to_the_next_row() {
  for_each_list_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    SignalLog log(fx.model.get());
    KCHECK(fx.source.move(0, 1));
    KCHECK(log.take() == QStringList({moved_lines(0, 2), done_lines(0, 2)}));  // put before row 2, which is row 1 once it left
    KCHECK(fx.shows({2, 1, 3, 4, 5}));
    KCHECK(fx.source.move(2, 3));
    KCHECK(log.take() == QStringList({moved_lines(2, 4), done_lines(2, 4)}));
    KCHECK(fx.shows({2, 1, 4, 3, 5}));
  });
}

// P-07
void move_one_row_up_to_the_previous_row() {
  for_each_list_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    SignalLog log(fx.model.get());
    KCHECK(fx.source.move(1, 0));
    KCHECK(log.take() == QStringList({moved_lines(1, 0), done_lines(1, 0)}));
    KCHECK(fx.shows({2, 1, 3, 4, 5}));
    KCHECK(fx.source.move(4, 3));
    KCHECK(log.take() == QStringList({moved_lines(4, 3), done_lines(4, 3)}));
    KCHECK(fx.shows({2, 1, 3, 5, 4}));
  });
}

// P-08
void move_from_the_first_row_to_the_last_and_back() {
  for_each_list_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    SignalLog log(fx.model.get());
    KCHECK(fx.source.move(0, kSmallFill - 1));
    KCHECK(log.take() == QStringList({moved_lines(0, kSmallFill), done_lines(0, kSmallFill)}));  // after the last row
    KCHECK(fx.shows({2, 3, 4, 5, 1}));
    KCHECK(fx.source.move(kSmallFill - 1, 0));
    KCHECK(log.take() == QStringList({moved_lines(kSmallFill - 1, 0), done_lines(kSmallFill - 1, 0)}));
    KCHECK(fx.shows({1, 2, 3, 4, 5}));
  });
}

// P-09: every ordered pair of rows, against a plain vector, with the signal contract watched
void move_destination_arithmetic_for_every_pair_of_rows() {
  for_each_list_model([](auto& fx, const std::string& kind) {
    fx.fill(kSmallFill);
    observable_test::ModelWatcher watcher(fx.model.get(), false);
    SignalLog log(fx.model.get());
    std::vector<int> expected = fx.shown();
    for (int from = 0; from < kSmallFill; ++from) {
      for (int to = 0; to < kSmallFill; ++to) {
        if (from == to) continue;
        KCHECK(fx.source.move(from, to));
        expected = after_move(expected, from, to);
        KCHECK(log.take() == QStringList({moved_lines(from, destination_of(from, to)), done_lines(from, destination_of(from, to))}));
        KCHECK(fx.shown() == expected);
        watcher.check();
      }
    }
    KCHECK(watcher.clean());
  });
}

// P-10
void move_out_of_range_is_refused() {
  for_each_list_model([](auto& fx, const std::string& kind) {
    KCHECK(!fx.source.move(0, 0));  // an empty list
    fx.fill(1);
    SignalLog log(fx.model.get());
    KCHECK(!fx.source.move(0, 1) && !fx.source.move(1, 0) && !fx.source.move(0, 0));  // one row has nowhere to go
    fx.fill(kSmallFill - 1);
    KCHECK(!fx.source.move(-1, 0) && !fx.source.move(0, -1));
    KCHECK(!fx.source.move(kSmallFill, 0) && !fx.source.move(0, kSmallFill));
    KCHECK(!fx.source.move(-1, kSmallFill));
    KCHECK(log.count("rowsAboutToBeMoved") == 0 && log.count("rowsMoved") == 0 && log.count("rowsAboutToBeRemoved") == 0);
    KCHECK(fx.shows({1, 2, 3, 4, 5}));
  });
}

// P-11
void persistent_indexes_follow_a_move() {
  for_each_list_model([](auto& fx, const std::string& kind) {
    using Fx = std::remove_cvref_t<decltype(fx)>;
    fx.fill(kSmallFill);
    std::vector<QPersistentModelIndex> tracked;
    std::vector<QPersistentModelIndex> twins;
    for (int row = 0; row < kSmallFill; ++row) {
      tracked.emplace_back(fx.index(row, 0));
      if constexpr (Fx::table) twins.emplace_back(fx.index(row, 1));
    }
    std::vector<int> rows = fx.shown();  // entry values by row, entry v is tracked[v - 1]
    const auto agrees = [&] {
      for (int entry = 1; entry <= kSmallFill; ++entry) {
        const int row = static_cast<int>(std::find(rows.begin(), rows.end(), entry) - rows.begin());
        const QPersistentModelIndex& p = tracked[static_cast<std::size_t>(entry - 1)];
        if (!(p.isValid() && p.row() == row && p.column() == 0 && p.data().toInt() == entry)) return false;
        if constexpr (Fx::table) {
          const QPersistentModelIndex& twin = twins[static_cast<std::size_t>(entry - 1)];
          if (!(twin.isValid() && twin.row() == row && twin.column() == 1 && twin.data().toInt() == kTwinFactor * entry)) return false;
        }
      }
      return true;
    };
    KCHECK(agrees());
    for (const auto& [from, to] : {std::pair{1, 3}, {3, 1}, {0, 4}, {4, 0}, {2, 3}, {3, 2}}) {
      KCHECK(fx.source.move(from, to));
      rows = after_move(rows, from, to);
      KCHECK(agrees());
    }
  });
}

// P-12
void selection_and_current_index_follow_a_move() {
  for_each_list_model([](auto& fx, const std::string& kind) {
    using Fx = std::remove_cvref_t<decltype(fx)>;
    fx.fill(kSmallFill);
    QItemSelectionModel selection(fx.model.get());
    selection.select(fx.index(1), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);   // entry 2
    selection.select(fx.index(3), QItemSelectionModel::Select | QItemSelectionModel::Rows);           // entry 4
    selection.setCurrentIndex(fx.index(2), QItemSelectionModel::NoUpdate);                            // entry 3
    const auto selected_entries = [&] {
      std::vector<int> out;
      for (const QModelIndex& index : selection.selectedRows()) out.push_back(index.data().toInt());
      std::sort(out.begin(), out.end());
      return out;
    };
    const std::vector<int> want{2, 4};
    KCHECK(selected_entries() == want);
    KCHECK(selection.selectedIndexes().size() == (Fx::table ? 2 * kTableColumns : 2));
    KCHECK(fx.source.move(1, 4));  // entry 2 to the end, across entry 4
    KCHECK(fx.shows({1, 3, 4, 5, 2}));
    KCHECK(selected_entries() == want);
    KCHECK(selection.currentIndex().data().toInt() == 3 && selection.currentIndex().row() == 1);
    KCHECK(fx.source.move(2, 0));  // entry 4 to the top
    KCHECK(fx.shows({4, 1, 3, 5, 2}));
    KCHECK(selected_entries() == want && selection.isRowSelected(0) && selection.isRowSelected(4));
    KCHECK(!selection.isRowSelected(1) && !selection.isRowSelected(2) && !selection.isRowSelected(3));
    KCHECK(selection.currentIndex().data().toInt() == 3 && selection.currentIndex().row() == 2);
    KCHECK(fx.source.move(4, 0) && fx.source.move(0, 4));  // there and back again
    KCHECK(selected_entries() == want);
  });
}

// P-13: every other container refuses a move, by design
void move_in_a_container_without_an_order_is_refused() {
  for_each_model([](auto& fx, const std::string& kind) {
    using Fx = std::remove_cvref_t<decltype(fx)>;
    if constexpr (!Fx::movable) {
      fx.fill(kSmallFill);
      SignalLog log(fx.model.get());
      KCHECK(!fx.source.move(Fx::named(1), Fx::named(2)));
      KCHECK(!fx.source.move(Fx::named(2), Fx::named(1)));
      KCHECK(!fx.source.move(Fx::named(1), Fx::named(1)));
      KCHECK(log.silent());
      KCHECK(fx.shows({1, 2, 3, 4, 5}));
    }
  });
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  move_to_the_same_row_is_refused();
  move_one_row_down_to_the_next_row();
  move_one_row_up_to_the_previous_row();
  move_from_the_first_row_to_the_last_and_back();
  move_destination_arithmetic_for_every_pair_of_rows();
  move_out_of_range_is_refused();
  persistent_indexes_follow_a_move();
  selection_and_current_index_follow_a_move();
  move_in_a_container_without_an_order_is_refused();
  return rqt_test::finish("capability_edge_protocol_moves");
}
