// A dense matrix labelled by keys that announces every change before it happens and confirms it after, with typed
// signals: the rows and the columns are named by keys (a string, a number, a struct with qHash), the cells hold a
// Value, and a cell that was never written holds the value the matrix was made with (the "uninitialised" value).
//
// The announcing is written once, in the mutators below, as for ObservableContainer, and the guards are the same:
//  - a mutator called from a receiver of the matrix's own signals (a change inside a change) is refused like an
//    invalid request: it returns false (or 0), changes nothing and emits nothing, and a debug build asserts;
//  - a matrix and its models live in one thread, and a debug build asserts it on every change;
//  - a key that cannot be found again (a NaN) is refused, a row or column past the limit of an int is refused.
// A mutator that is asked for something invalid (a key that is there already, a key that is not, a rename onto a key in
// use) returns false (0 for a batch), changes nothing and emits nothing.
//
// The signals say what changed, in indices as well as keys, so a model needs no lookup:
//  - rows and columns are added at the end, one or many (one pair of signals over the new range), and removed one or
//    many (one pair per run of consecutive indices, from the last run to the first, so the runs still to do keep
//    their indices);
//  - a rename changes a key and nothing else: the row keeps its index and its cells (a model emits headerDataChanged);
//  - setValue announces one cell, setValues announces the changed cells as FULL rectangles (never the bounding box of
//    cells some of which did not change): every "about" first, then every write, then every twin, so a receiver at an
//    "about" signal sees the old values of all of them;
//  - clear removes every row and column between one pair of signals.
// A cell that is set to the value it holds changes nothing and says nothing.
//
// The index of a key is found with a Fenwick tree over insertion slots (row_index.hpp), so a lookup and a removal
// cost O(log n) however many rows there are, where rebuilding a hash from the key list after every change costs O(n).
#pragma once

#include "container_traits.hpp"
#include "row_index.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QThread>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace observable {

// A key of a row or a column: copied, compared, hashed.
template <class K>
concept MatrixKey = std::copyable<K> && std::equality_comparable<K> && requires(const K& k) {
  { qHash(k) } -> std::convertible_to<std::size_t>;
};

// Inclusive runs of an ascending, duplicate-free range of indices: {1, 2, 5, 7, 8} gives [1,2] [5,5] [7,8].
inline std::vector<std::pair<qsizetype, qsizetype>> contiguous_runs(const std::vector<qsizetype>& ascending) {
  std::vector<std::pair<qsizetype, qsizetype>> runs;
  for (const qsizetype index : ascending) {
    if (!runs.empty() && runs.back().second + 1 == index)
      runs.back().second = index;
    else
      runs.emplace_back(index, index);
  }
  return runs;
}

template <class RowKey, class ColKey, class Value>
  requires MatrixKey<RowKey> && MatrixKey<ColKey> && std::equality_comparable<Value>
class ObservableMatrix : public QObject {
  RQT_OBJECT

 public:
  using RowKeyType = RowKey;
  using ColKeyType = ColKey;
  using ValueType = Value;
  using Cell = std::tuple<RowKey, ColKey, Value>;

  // A Value that can throw when it moves would leave every model between "about" and "done".
  static_assert(std::is_nothrow_move_constructible_v<Value> && std::is_nothrow_move_assignable_v<Value>,
                "observable::ObservableMatrix: the value type must be nothrow move constructible and assignable, "
                "because a throw between the 'about' and the 'done' signal leaves every model mid-change");

  // `uninitialized` is the value of a cell nobody wrote, and of a cell that is asked for in a row or column that is not there.
  explicit ObservableMatrix(Value uninitialized, QObject* parent = nullptr) : QObject(parent), m_sentinel(std::move(uninitialized)) {}

 signals:
  rqt::signal<void(const RowKey&, qsizetype)> rowAboutToBeAdded;
  rqt::signal<void(const RowKey&, qsizetype)> rowAdded;
  rqt::signal<void(const ColKey&, qsizetype)> columnAboutToBeAdded;
  rqt::signal<void(const ColKey&, qsizetype)> columnAdded;
  rqt::signal<void(qsizetype, qsizetype, const QList<RowKey>&)> rowsAboutToBeAdded;  // first, last, keys
  rqt::signal<void(qsizetype, qsizetype, const QList<RowKey>&)> rowsAdded;
  rqt::signal<void(qsizetype, qsizetype, const QList<ColKey>&)> columnsAboutToBeAdded;
  rqt::signal<void(qsizetype, qsizetype, const QList<ColKey>&)> columnsAdded;
  rqt::signal<void(const RowKey&, qsizetype)> rowAboutToBeRemoved;
  rqt::signal<void(const RowKey&, qsizetype)> rowRemoved;
  rqt::signal<void(const ColKey&, qsizetype)> columnAboutToBeRemoved;
  rqt::signal<void(const ColKey&, qsizetype)> columnRemoved;
  rqt::signal<void(qsizetype, qsizetype, const QList<RowKey>&)> rowsAboutToBeRemoved;  // one run: first, last, keys
  rqt::signal<void(qsizetype, qsizetype, const QList<RowKey>&)> rowsRemoved;
  rqt::signal<void(qsizetype, qsizetype, const QList<ColKey>&)> columnsAboutToBeRemoved;
  rqt::signal<void(qsizetype, qsizetype, const QList<ColKey>&)> columnsRemoved;
  rqt::signal<void(const RowKey&, const RowKey&, qsizetype)> rowAboutToBeRenamed;  // from, to, index
  rqt::signal<void(const RowKey&, const RowKey&, qsizetype)> rowRenamed;
  rqt::signal<void(const ColKey&, const ColKey&, qsizetype)> columnAboutToBeRenamed;
  rqt::signal<void(const ColKey&, const ColKey&, qsizetype)> columnRenamed;
  rqt::signal<void(const RowKey&, const ColKey&, qsizetype, qsizetype)> cellAboutToChange;  // keys, row, column
  rqt::signal<void(const RowKey&, const ColKey&, qsizetype, qsizetype)> cellChanged;
  rqt::signal<void(qsizetype, qsizetype, qsizetype, qsizetype)> cellsAboutToChange;  // top, left, bottom, right
  rqt::signal<void(qsizetype, qsizetype, qsizetype, qsizetype)> cellsChanged;
  rqt::signal<void()> aboutToBeCleared;
  rqt::signal<void()> cleared;

 public:
  qsizetype rowCount() const { return m_rows.size(); }
  qsizetype columnCount() const { return m_columns.size(); }
  const QList<RowKey>& rowKeys() const { return m_rows; }
  const QList<ColKey>& columnKeys() const { return m_columns; }

  // The index of a key, or -1. A key that cannot be found again (a NaN) is never there.
  qsizetype rowIndexOf(const RowKey& key) const { return indexIn(m_rowIndex, key); }
  qsizetype columnIndexOf(const ColKey& key) const { return indexIn(m_columnIndex, key); }
  bool hasRow(const RowKey& key) const { return rowIndexOf(key) >= 0; }
  bool hasColumn(const ColKey& key) const { return columnIndexOf(key) >= 0; }

  // "Absent" is a value: a NaN never equals itself, so a NaN sentinel is tested with isnan.
  bool isUninitialized(const Value& v) const { return sameValue(v, m_sentinel); }

  // Total: an absent row or column gives the uninitialised value, and reading never creates structure.
  Value value(const RowKey& row, const ColKey& column) const {
    const qsizetype i = rowIndexOf(row);
    const qsizetype j = columnIndexOf(column);
    return i < 0 || j < 0 ? m_sentinel : m_cells[cellOffset(i, j)];
  }

  Value valueAt(qsizetype row, qsizetype column) const {
    Q_ASSERT(row >= 0 && row < rowCount() && column >= 0 && column < columnCount());
    return m_cells[cellOffset(row, column)];
  }

  void assertOwnThread() const {
    Q_ASSERT_X(QThread::currentThread() == thread(), "observable::ObservableMatrix",
               "a matrix must be changed from the thread it lives in (move data between threads, not the model)");
  }

  // True while a mutator is announcing, between its first signal and its last.
  bool announcing() const { return m_announcing; }

  bool addRow(const RowKey& key) {
    assertOwnThread();
    if (refusedAsNested() || !canAdd(m_rowIndex, key, rowCount())) return false;
    const Announcing announcing(*this);
    const qsizetype index = rowCount();
    emit rowAboutToBeAdded(key, index);
    appendRows({key});
    emit rowAdded(key, index);
    return true;
  }

  bool addColumn(const ColKey& key) {
    assertOwnThread();
    if (refusedAsNested() || !canAdd(m_columnIndex, key, columnCount())) return false;
    const Announcing announcing(*this);
    const qsizetype index = columnCount();
    emit columnAboutToBeAdded(key, index);
    appendColumns({key});
    emit columnAdded(key, index);
    return true;
  }

  // Adds the keys that are not there, once each, in the order given: one pair of signals over the new range. Returns how
  // many were added, and says nothing when that is none.
  qsizetype addRows(const QList<RowKey>& keys) {
    assertOwnThread();
    if (refusedAsNested()) return 0;
    const QList<RowKey> fresh = freshKeys(m_rowIndex, keys);
    if (fresh.isEmpty() || !fits_rows(rowCount(), fresh.size())) return 0;
    const Announcing announcing(*this);
    const qsizetype first = rowCount();
    const qsizetype last = first + fresh.size() - 1;
    emit rowsAboutToBeAdded(first, last, fresh);
    appendRows(fresh);
    emit rowsAdded(first, last, fresh);
    return fresh.size();
  }

  qsizetype addColumns(const QList<ColKey>& keys) {
    assertOwnThread();
    if (refusedAsNested()) return 0;
    const QList<ColKey> fresh = freshKeys(m_columnIndex, keys);
    if (fresh.isEmpty() || !fits_rows(columnCount(), fresh.size())) return 0;
    const Announcing announcing(*this);
    const qsizetype first = columnCount();
    const qsizetype last = first + fresh.size() - 1;
    emit columnsAboutToBeAdded(first, last, fresh);
    appendColumns(fresh);
    emit columnsAdded(first, last, fresh);
    return fresh.size();
  }

  bool removeRow(const RowKey& key) {
    assertOwnThread();
    if (refusedAsNested()) return false;
    const qsizetype index = rowIndexOf(key);
    if (index < 0) return false;
    const Announcing announcing(*this);
    const RowKey removed = m_rows[index];  // a copy: `key` may refer to the element that goes
    emit rowAboutToBeRemoved(removed, index);
    eraseRows(index, index);
    emit rowRemoved(removed, index);
    return true;
  }

  bool removeColumn(const ColKey& key) {
    assertOwnThread();
    if (refusedAsNested()) return false;
    const qsizetype index = columnIndexOf(key);
    if (index < 0) return false;
    const Announcing announcing(*this);
    const ColKey removed = m_columns[index];
    emit columnAboutToBeRemoved(removed, index);
    eraseColumns(index, index);
    emit columnRemoved(removed, index);
    return true;
  }

  // Removes the keys that are there, once each: one pair of signals per run of consecutive rows, from the last run to the
  // first. Returns how many were removed.
  qsizetype removeRows(const QList<RowKey>& keys) {
    assertOwnThread();
    if (refusedAsNested()) return 0;
    const std::vector<qsizetype> indices = sortedIndices(m_rowIndex, keys);
    const Announcing announcing(*this);
    const auto runs = contiguous_runs(indices);
    for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
      const QList<RowKey> keysOfRun = m_rows.mid(run->first, run->second - run->first + 1);
      emit rowsAboutToBeRemoved(run->first, run->second, keysOfRun);
      eraseRows(run->first, run->second);
      emit rowsRemoved(run->first, run->second, keysOfRun);
    }
    return static_cast<qsizetype>(indices.size());
  }

  qsizetype removeColumns(const QList<ColKey>& keys) {
    assertOwnThread();
    if (refusedAsNested()) return 0;
    const std::vector<qsizetype> indices = sortedIndices(m_columnIndex, keys);
    const Announcing announcing(*this);
    const auto runs = contiguous_runs(indices);
    for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
      const QList<ColKey> keysOfRun = m_columns.mid(run->first, run->second - run->first + 1);
      emit columnsAboutToBeRemoved(run->first, run->second, keysOfRun);
      eraseColumns(run->first, run->second);
      emit columnsRemoved(run->first, run->second, keysOfRun);
    }
    return static_cast<qsizetype>(indices.size());
  }

  // Gives a row another key. Never a move, a remove plus an add, or a merge: the row keeps its index and its cells. A
  // rename to the key it has is true and says nothing; a rename onto another row's key is refused.
  bool renameRow(const RowKey& from, const RowKey& to) {
    assertOwnThread();
    if (refusedAsNested()) return false;
    const qsizetype index = rowIndexOf(from);
    if (index < 0 || !usable_key(to)) return false;
    if (from == to) return true;
    if (hasRow(to)) return false;
    const Announcing announcing(*this);
    const RowKey oldKey = m_rows[index];  // a copy: `from` may refer to the element that is overwritten
    emit rowAboutToBeRenamed(oldKey, to, index);
    m_rows[index] = to;
    m_rowIndex.rename(oldKey, to);
    emit rowRenamed(oldKey, to, index);
    return true;
  }

  bool renameColumn(const ColKey& from, const ColKey& to) {
    assertOwnThread();
    if (refusedAsNested()) return false;
    const qsizetype index = columnIndexOf(from);
    if (index < 0 || !usable_key(to)) return false;
    if (from == to) return true;
    if (hasColumn(to)) return false;
    const Announcing announcing(*this);
    const ColKey oldKey = m_columns[index];
    emit columnAboutToBeRenamed(oldKey, to, index);
    m_columns[index] = to;
    m_columnIndex.rename(oldKey, to);
    emit columnRenamed(oldKey, to, index);
    return true;
  }

  // Sets a cell. False for a row or column that is not there; true, and silent, when the cell holds the value already.
  bool setValue(const RowKey& row, const ColKey& column, const Value& v) {
    assertOwnThread();
    if (refusedAsNested()) return false;
    const qsizetype i = rowIndexOf(row);
    const qsizetype j = columnIndexOf(column);
    if (i < 0 || j < 0) return false;
    if (sameValue(m_cells[cellOffset(i, j)], v)) return true;
    const Announcing announcing(*this);
    emit cellAboutToChange(row, column, i, j);
    m_cells[cellOffset(i, j)] = v;
    emit cellChanged(row, column, i, j);
    return true;
  }

  // Sets many cells. A cell in a row or column that is not there is skipped, the last of a repeated cell wins, a cell that
  // would not change is dropped; what is left is announced as full rectangles. Returns how many cells changed.
  qsizetype setValues(const QList<Cell>& cells) {
    assertOwnThread();
    if (refusedAsNested()) return 0;
    const Changes changed = changedCells(cells);
    if (changed.empty()) return 0;
    std::vector<Rect> rects = rectanglesOf(changed);
    std::ranges::sort(rects, [](const Rect& a, const Rect& b) { return std::tie(a.top, a.left) < std::tie(b.top, b.left); });
    const Announcing announcing(*this);
    for (const Rect& r : rects) emit cellsAboutToChange(r.top, r.left, r.bottom, r.right);
    for (const auto& [position, v] : changed) m_cells[cellOffset(position.first, position.second)] = v;
    for (const Rect& r : rects) emit cellsChanged(r.top, r.left, r.bottom, r.right);
    return static_cast<qsizetype>(changed.size());
  }

  // Removes every row and column between one pair of signals. An empty matrix says nothing.
  bool clear() {
    assertOwnThread();
    if (refusedAsNested() || (m_rows.isEmpty() && m_columns.isEmpty())) return false;
    const Announcing announcing(*this);
    emit aboutToBeCleared();
    m_rows.clear();
    m_columns.clear();
    m_cells.clear();
    m_rowIndex.clear();
    m_columnIndex.clear();
    emit cleared();
    return true;
  }

 private:
  struct Rect {
    qsizetype top, left, bottom, right;
  };
  using Position = std::pair<qsizetype, qsizetype>;  // row, column
  using Changes = std::map<Position, Value>;         // ordered by row, then column

  // Marks the matrix as announcing for the length of one mutator, also when a receiver throws.
  class Announcing {
   public:
    explicit Announcing(ObservableMatrix& matrix) : m_matrix(matrix) { m_matrix.m_announcing = true; }
    ~Announcing() { m_matrix.m_announcing = false; }
    Announcing(const Announcing&) = delete;
    Announcing& operator=(const Announcing&) = delete;

   private:
    ObservableMatrix& m_matrix;
  };

  bool refusedAsNested() const {
    Q_ASSERT_X(!m_announcing, "observable::ObservableMatrix",
               "a matrix must not be changed from a receiver of its own signals (queue the change, or make it after the "
               "announcing call has returned)");
    return m_announcing;
  }

  qsizetype cellOffset(qsizetype row, qsizetype column) const { return row * columnCount() + column; }

  template <class Key>
  static qsizetype indexIn(const InsertionOrderIndex<Key>& index, const Key& key) {
    return usable_key(key) && index.contains(key) ? index.rowOf(key) : -1;
  }

  template <class Key>
  static bool canAdd(const InsertionOrderIndex<Key>& index, const Key& key, qsizetype count) {
    return usable_key(key) && !index.contains(key) && fits_rows(count, 1);
  }

  // Equal for "nothing changed": a NaN over a NaN is equal when the uninitialised value is a NaN.
  bool sameValue(const Value& a, const Value& b) const {
    if constexpr (std::is_floating_point_v<Value>) {
      if (std::isnan(m_sentinel) && std::isnan(a) && std::isnan(b)) return true;
    }
    return a == b;
  }

  // The keys that can be added, once each, in the order given.
  template <class Key>
  static QList<Key> freshKeys(const InsertionOrderIndex<Key>& index, const QList<Key>& keys) {
    QList<Key> fresh;
    QSet<Key> seen;
    for (const Key& key : keys) {
      if (!usable_key(key) || index.contains(key) || seen.contains(key)) continue;
      seen.insert(key);
      fresh.append(key);
    }
    return fresh;
  }

  // The ascending, duplicate-free indices of the keys that are there.
  template <class Key>
  static std::vector<qsizetype> sortedIndices(const InsertionOrderIndex<Key>& index, const QList<Key>& keys) {
    std::vector<qsizetype> indices;
    for (const Key& key : keys)
      if (usable_key(key) && index.contains(key)) indices.push_back(index.rowOf(key));
    std::ranges::sort(indices);
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
  }

  void appendRows(const QList<RowKey>& keys) {
    for (const RowKey& key : keys) m_rowIndex.insert(key, rowCount());
    m_rows.append(keys);
    m_cells.insert(m_cells.size(), columnCount() * keys.size(), m_sentinel);
  }

  // `keys.size()` columns after the old ones: a run of sentinels at the end of every row, the last row first, so the
  // offsets still to use do not move.
  void appendColumns(const QList<ColKey>& keys) {
    const qsizetype oldWidth = columnCount();
    for (const ColKey& key : keys) m_columnIndex.insert(key, columnCount());
    m_columns.append(keys);
    for (qsizetype r = rowCount() - 1; r >= 0; --r) m_cells.insert((r + 1) * oldWidth, keys.size(), m_sentinel);
  }

  void eraseRows(qsizetype first, qsizetype last) {
    for (qsizetype r = first; r <= last; ++r) m_rowIndex.erase(m_rows[r], r);
    const qsizetype width = columnCount();
    m_cells.remove(first * width, (last - first + 1) * width);
    m_rows.remove(first, last - first + 1);
  }

  void eraseColumns(qsizetype first, qsizetype last) {
    for (qsizetype c = first; c <= last; ++c) m_columnIndex.erase(m_columns[c], c);
    const qsizetype oldWidth = columnCount();
    for (qsizetype r = rowCount() - 1; r >= 0; --r) m_cells.remove(r * oldWidth + first, last - first + 1);
    m_columns.remove(first, last - first + 1);
  }

  // The cells that would change: absent keys skipped, the last occurrence of a cell wins, unchanged cells dropped.
  Changes changedCells(const QList<Cell>& cells) const {
    Changes changed;
    for (const auto& [row, column, v] : cells) {
      const qsizetype i = rowIndexOf(row);
      const qsizetype j = columnIndexOf(column);
      if (i >= 0 && j >= 0) changed.insert_or_assign(Position{i, j}, v);
    }
    std::erase_if(changed, [this](const auto& entry) { return sameValue(m_cells[cellOffset(entry.first.first, entry.first.second)], entry.second); });
    return changed;
  }

  // Per row, the runs of consecutive changed columns.
  static std::map<qsizetype, std::vector<Position>> runsByRow(const Changes& changed) {
    std::map<qsizetype, std::vector<Position>> runs;  // row -> (left, right) runs
    for (const auto& entry : changed) {
      const auto [row, column] = entry.first;
      std::vector<Position>& rowRuns = runs[row];
      if (!rowRuns.empty() && rowRuns.back().second + 1 == column)
        rowRuns.back().second = column;
      else
        rowRuns.emplace_back(column, column);
    }
    return runs;
  }

  // A run joins the open rectangle of the previous row with the same extent, or starts a new one.
  static std::size_t joinOrStart(std::vector<Rect>& rects, const std::map<Position, std::size_t>& open, qsizetype row, const Position& run) {
    const auto joined = open.find(run);
    if (joined == open.end()) {
      rects.push_back({row, run.first, row, run.second});
      return rects.size() - 1;
    }
    rects[joined->second].bottom = row;
    return joined->second;
  }

  // The changed cells as full rectangles: rows that follow each other with runs of the same extent make one.
  static std::vector<Rect> rectanglesOf(const Changes& changed) {
    std::vector<Rect> rects;
    std::map<Position, std::size_t> open;  // extent -> the rectangle whose bottom is the previous row
    qsizetype previousRow = -2;
    for (const auto& [row, runs] : runsByRow(changed)) {
      if (row != previousRow + 1) open.clear();  // a gap ends every rectangle
      std::map<Position, std::size_t> next;
      for (const Position& run : runs) next.insert_or_assign(run, joinOrStart(rects, open, row, run));
      open = std::move(next);
      previousRow = row;
    }
    return rects;
  }

  Value m_sentinel;
  QList<RowKey> m_rows;
  QList<ColKey> m_columns;
  QList<Value> m_cells;  // row-major
  InsertionOrderIndex<RowKey> m_rowIndex;
  InsertionOrderIndex<ColKey> m_columnIndex;
  bool m_announcing = false;
};

}  // namespace observable
