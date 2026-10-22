// Support for the matrix tests: what a view of a matrix believes (built from the model's signals alone), a plain reference
// implementation of the matrix, and a seeded random driver that compares them after every change.
//
// A view only learns about changes from signals, so the MatrixMirror is built from them and compared with the live model
// after every step: any change that was not announced, announced for the wrong rows or columns, announced in the wrong
// order or never confirmed shows up as a difference. A dataChanged is also required to be EXACT: every cell it names did
// change (the matrix announces full rectangles, never a bounding box), and a header that changed must come with a
// headerDataChanged, never with a removal and an insert (the driver tracks a persistent index and a selection by the
// identity of a row, which a rename must keep).
#pragma once

#include "check.hpp"

#include <matrix_table_model.hpp>
#include <observable_matrix.hpp>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <utility>
#include <vector>

namespace observable_test {

using IntMatrix = observable::ObservableMatrix<QString, QString, int>;
constexpr int kUnset = -1;  // the uninitialised value of the driver's matrix

constexpr int kRowKeyDomain = 12;     // keys are drawn from a small domain, so they collide
constexpr int kColumnKeyDomain = 8;
constexpr int kCellValues = 4;        // values are drawn from [0, kCellValues): a write often changes nothing
constexpr int kMatrixKinds = 100;     // the weights below add up to at most this
constexpr int kWeightAddRow = 9;
constexpr int kWeightAddColumn = 8;
constexpr int kWeightAddRows = 6;
constexpr int kWeightAddColumns = 5;
constexpr int kWeightRemoveRow = 6;
constexpr int kWeightRemoveColumn = 5;
constexpr int kWeightRemoveRows = 5;
constexpr int kWeightRemoveColumns = 4;
constexpr int kWeightRenameRow = 8;
constexpr int kWeightRenameColumn = 6;
constexpr int kWeightSetValue = 14;
constexpr int kWeightSetValues = 12;
constexpr int kWeightSelect = 6;
constexpr int kWeightTrack = 3;
constexpr int kLongestMatrixBatch = 6;
constexpr int kDetails = 3;           // failed checks that a report describes

inline QString row_key(int n) { return QStringLiteral("r%1").arg(n); }
inline QString column_key(int n) { return QStringLiteral("c%1").arg(n); }

// What a view believes: headers and cells, kept from signals only.
class MatrixMirror : public QObject {
 public:
  explicit MatrixMirror(QAbstractItemModel* model) : QObject(model), m_model(model) {
    readAll();
    m_announced = {m_model->rowCount(), m_model->columnCount()};
    using M = QAbstractItemModel;
    connect(model, &M::rowsAboutToBeInserted, this, [this](const QModelIndex&, int first, int last) { begin(Open::insertRows, first, last); });
    connect(model, &M::rowsInserted, this, [this](const QModelIndex&, int first, int last) {
      if (!end(Open::insertRows, first, last, m_before.first + (last - first + 1), m_before.second)) return;
      if (first > static_cast<int>(m_cells.size())) return violation("rows were inserted past the end of the view");
      for (int row = first; row <= last; ++row) {
        m_cells.insert(m_cells.begin() + row, std::vector<QVariant>(static_cast<std::size_t>(m_model->columnCount())));
        m_rowHeaders.insert(m_rowHeaders.begin() + row, QString());
      }
      for (int row = first; row <= last; ++row) readRow(row);
    });
    connect(model, &M::rowsAboutToBeRemoved, this, [this](const QModelIndex&, int first, int last) { begin(Open::removeRows, first, last); });
    connect(model, &M::rowsRemoved, this, [this](const QModelIndex&, int first, int last) {
      if (!end(Open::removeRows, first, last, m_before.first - (last - first + 1), m_before.second)) return;
      if (last < static_cast<int>(m_cells.size())) {
        m_cells.erase(m_cells.begin() + first, m_cells.begin() + last + 1);
        m_rowHeaders.erase(m_rowHeaders.begin() + first, m_rowHeaders.begin() + last + 1);
      }
    });
    connect(model, &M::columnsAboutToBeInserted, this, [this](const QModelIndex&, int first, int last) { begin(Open::insertColumns, first, last); });
    connect(model, &M::columnsInserted, this, [this](const QModelIndex&, int first, int last) {
      if (!end(Open::insertColumns, first, last, m_before.first, m_before.second + (last - first + 1))) return;
      if (first > static_cast<int>(m_columnHeaders.size())) return violation("columns were inserted past the end of the view");
      for (auto& row : m_cells) row.insert(row.begin() + first, static_cast<std::size_t>(last - first + 1), QVariant());
      m_columnHeaders.insert(m_columnHeaders.begin() + first, static_cast<std::size_t>(last - first + 1), QString());
      for (int column = first; column <= last; ++column) readColumn(column);
    });
    connect(model, &M::columnsAboutToBeRemoved, this, [this](const QModelIndex&, int first, int last) { begin(Open::removeColumns, first, last); });
    connect(model, &M::columnsRemoved, this, [this](const QModelIndex&, int first, int last) {
      if (!end(Open::removeColumns, first, last, m_before.first, m_before.second - (last - first + 1))) return;
      if (last >= static_cast<int>(m_columnHeaders.size())) return violation("columns were removed past the end of the view");
      for (auto& row : m_cells) row.erase(row.begin() + first, row.begin() + last + 1);
      m_columnHeaders.erase(m_columnHeaders.begin() + first, m_columnHeaders.begin() + last + 1);
    });
    connect(model, &M::rowsAboutToBeMoved, this, [this](const QModelIndex&, int, int, const QModelIndex&, int) { violation("a move was announced"); });
    connect(model, &M::modelAboutToBeReset, this, [this] { begin(Open::reset, 0, 0); });
    connect(model, &M::modelReset, this, [this] {
      if (m_open != Open::reset) violation("modelReset without modelAboutToBeReset");
      m_open = Open::none;
      readAll();
      m_announced = {m_model->rowCount(), m_model->columnCount()};
    });
    connect(model, &M::layoutChanged, this, [this] { violation("a layout change was announced"); });
    connect(model, &M::dataChanged, this, [this](const QModelIndex& top, const QModelIndex& bottom) {
      if (!top.isValid() || !bottom.isValid()) return violation("dataChanged with an invalid index");
      for (int row = top.row(); row <= bottom.row(); ++row) {
        for (int column = top.column(); column <= bottom.column(); ++column) {
          if (row >= static_cast<int>(m_cells.size()) || column >= static_cast<int>(m_columnHeaders.size())) {
            violation("dataChanged names a cell the view does not have");
            continue;
          }
          const QVariant now = m_model->data(m_model->index(row, column));
          if (m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] == now) violation("dataChanged names a cell that did not change");
          m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = now;
        }
      }
    });
    connect(model, &M::headerDataChanged, this, [this](Qt::Orientation orientation, int first, int last) {
      for (int section = first; section <= last; ++section) {
        if (orientation == Qt::Vertical && section < static_cast<int>(m_rowHeaders.size())) m_rowHeaders[static_cast<std::size_t>(section)] = header(orientation, section);
        if (orientation == Qt::Horizontal && section < static_cast<int>(m_columnHeaders.size())) m_columnHeaders[static_cast<std::size_t>(section)] = header(orientation, section);
      }
    });
  }

  // Call after every change: nothing may be left open, and the mirror must equal the live model.
  void check() {
    if (m_open != Open::none) violation("a change was announced and never confirmed");
    if (m_model->rowCount() != m_announced.first || m_model->columnCount() != m_announced.second) violation("a count changed without an announcement");
    if (static_cast<int>(m_cells.size()) != m_model->rowCount() || static_cast<int>(m_columnHeaders.size()) != m_model->columnCount()) {
      violation(QStringLiteral("a view would show %1 by %2, the model has %3 by %4").arg(m_cells.size()).arg(m_columnHeaders.size()).arg(m_model->rowCount()).arg(m_model->columnCount()));
      return;
    }
    for (int row = 0; row < m_model->rowCount(); ++row) {
      if (m_rowHeaders[static_cast<std::size_t>(row)] != header(Qt::Vertical, row)) violation(QStringLiteral("row header %1 is stale").arg(row));
      for (int column = 0; column < m_model->columnCount(); ++column)
        if (m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] != m_model->data(m_model->index(row, column)))
          violation(QStringLiteral("cell %1,%2 is stale").arg(row).arg(column));
    }
    for (int column = 0; column < m_model->columnCount(); ++column)
      if (m_columnHeaders[static_cast<std::size_t>(column)] != header(Qt::Horizontal, column)) violation(QStringLiteral("column header %1 is stale").arg(column));
  }

  const QStringList& violations() const { return m_violations; }
  bool clean() const { return m_violations.isEmpty(); }

 private:
  enum class Open { none, insertRows, removeRows, insertColumns, removeColumns, reset };

  QString header(Qt::Orientation orientation, int section) const { return m_model->headerData(section, orientation).toString(); }
  void violation(const QString& text) { m_violations.append(text); }

  void readRow(int row) {
    m_rowHeaders[static_cast<std::size_t>(row)] = header(Qt::Vertical, row);
    for (int column = 0; column < m_model->columnCount(); ++column) m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = m_model->data(m_model->index(row, column));
  }
  void readColumn(int column) {
    m_columnHeaders[static_cast<std::size_t>(column)] = header(Qt::Horizontal, column);
    for (int row = 0; row < m_model->rowCount(); ++row) m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = m_model->data(m_model->index(row, column));
  }
  void readAll() {
    m_cells.assign(static_cast<std::size_t>(m_model->rowCount()), std::vector<QVariant>(static_cast<std::size_t>(m_model->columnCount())));
    m_rowHeaders.assign(static_cast<std::size_t>(m_model->rowCount()), QString());
    m_columnHeaders.assign(static_cast<std::size_t>(m_model->columnCount()), QString());
    for (int row = 0; row < m_model->rowCount(); ++row) readRow(row);
    for (int column = 0; column < m_model->columnCount(); ++column) readColumn(column);
  }

  void begin(Open kind, int first, int last) {
    if (m_open != Open::none) violation("a change was announced while another was still open");
    m_open = kind;
    m_first = first;
    m_last = last;
    m_before = {m_model->rowCount(), m_model->columnCount()};
  }
  // True when the confirmation matches the announcement and the counts are what the change implies.
  bool end(Open kind, int first, int last, int expectedRows, int expectedColumns) {
    bool ok = true;
    if (m_open != kind) {
      violation("a change was confirmed that was not announced");
      ok = false;
    }
    if (first != m_first || last != m_last) {
      violation("the confirmation names other indices than the announcement");
      ok = false;
    }
    if (m_model->rowCount() != expectedRows || m_model->columnCount() != expectedColumns) {
      violation("the counts after the change are not what the change implies");
      ok = false;
    }
    m_open = Open::none;
    m_announced = {m_model->rowCount(), m_model->columnCount()};
    return ok;
  }

  QAbstractItemModel* m_model;
  std::vector<std::vector<QVariant>> m_cells;
  std::vector<QString> m_rowHeaders;
  std::vector<QString> m_columnHeaders;
  QStringList m_violations;
  Open m_open = Open::none;
  int m_first = 0;
  int m_last = 0;
  std::pair<int, int> m_before{0, 0};
  std::pair<int, int> m_announced{0, 0};
};

// The expected matrix: keys in order, an id per row and column that never changes while it lives, and the cells.
class MatrixShadow {
 public:
  struct Line {
    QString key;
    int id;
  };

  const std::vector<Line>& rows() const { return m_rows; }
  const std::vector<Line>& columns() const { return m_columns; }
  int value(const QString& row, const QString& column) const {
    const auto found = m_cells.find({idOf(m_rows, row), idOf(m_columns, column)});
    return found == m_cells.end() ? kUnset : found->second;
  }
  int value(const Line& row, const Line& column) const { return value(row.key, column.key); }
  std::optional<int> rowIndex(const QString& key) const { return indexOf(m_rows, key); }
  std::optional<int> columnIndex(const QString& key) const { return indexOf(m_columns, key); }

  bool addRow(const QString& key) { return add(m_rows, key, m_nextRow); }
  bool addColumn(const QString& key) { return add(m_columns, key, m_nextColumn); }
  bool removeRow(const QString& key) { return remove(m_rows, key); }
  bool removeColumn(const QString& key) { return remove(m_columns, key); }
  bool renameRow(const QString& from, const QString& to) { return rename(m_rows, from, to); }
  bool renameColumn(const QString& from, const QString& to) { return rename(m_columns, from, to); }

  // Returns whether the cell changed (and writes it).
  std::optional<bool> set(const QString& row, const QString& column, int v) {
    const auto r = rowIndex(row);
    const auto c = columnIndex(column);
    if (!r || !c) return std::nullopt;
    const std::pair<int, int> at{m_rows[static_cast<std::size_t>(*r)].id, m_columns[static_cast<std::size_t>(*c)].id};
    if (value(row, column) == v) return false;
    m_cells[at] = v;
    return true;
  }

  void clear() {
    m_rows.clear();
    m_columns.clear();
    m_cells.clear();
  }

 private:
  static std::optional<int> indexOf(const std::vector<Line>& lines, const QString& key) {
    for (std::size_t i = 0; i < lines.size(); ++i)
      if (lines[i].key == key) return static_cast<int>(i);
    return std::nullopt;
  }
  static int idOf(const std::vector<Line>& lines, const QString& key) {
    const auto at = indexOf(lines, key);
    return at ? lines[static_cast<std::size_t>(*at)].id : -1;
  }
  static bool add(std::vector<Line>& lines, const QString& key, int& next) {
    if (indexOf(lines, key)) return false;
    lines.push_back({key, next++});
    return true;
  }
  static bool remove(std::vector<Line>& lines, const QString& key) {
    const auto at = indexOf(lines, key);
    if (!at) return false;
    lines.erase(lines.begin() + *at);
    return true;
  }
  static bool rename(std::vector<Line>& lines, const QString& from, const QString& to) {
    const auto at = indexOf(lines, from);
    if (!at) return false;
    if (from == to) return true;
    if (indexOf(lines, to)) return false;
    lines[static_cast<std::size_t>(*at)].key = to;
    return true;
  }

  std::vector<Line> m_rows;
  std::vector<Line> m_columns;
  std::map<std::pair<int, int>, int> m_cells;  // by (row id, column id): cells follow their row and column through any change
  int m_nextRow = 0;
  int m_nextColumn = 0;
};

struct MatrixReport {
  int steps = 0;
  int applied = 0;     // changes the matrix accepted
  int refused = 0;     // requests it refused (and answered false or 0 to)
  int mismatches = 0;  // the matrix, the model, the selection or a persistent index disagreed with the reference
  QStringList violations;
  QStringList details;  // the first few failed checks, with the step
  bool clean() const { return mismatches == 0 && violations.isEmpty(); }
};

// Applies `steps` random changes to a matrix and, after each, compares the matrix, its model, a mirror built from the
// model's signals, a selection of cells and a persistent index with the reference.
// The place of a failed check is reported with the step, so a mismatch can be found without a debugger.
#define EXPECT(cond) check_at((cond), __LINE__)

template <class MakeModel>
MatrixReport run_random_matrix(unsigned seed, int steps, MakeModel&& make_model) {
  IntMatrix matrix(kUnset);
  std::unique_ptr<QAbstractItemModel> model = make_model(&matrix);
  MatrixMirror mirror(model.get());
  QItemSelectionModel selection(model.get());
  QPersistentModelIndex tracked;
  std::optional<std::pair<int, int>> trackedIds;  // (row id, column id) of the cell the persistent index follows
  std::set<std::pair<int, int>> selectedCells;  // (row id, column id) of every selected cell
  MatrixShadow shadow;
  std::mt19937 rng(seed);
  MatrixReport report;

  const auto draw = [&rng](int low, int high) { return std::uniform_int_distribution<int>(low, high - 1)(rng); };
  int currentStep = 0;
  const auto check_at = [&report, &currentStep](bool ok, int line) {
    if (ok) return;
    ++report.mismatches;
    if (report.details.size() < kDetails) report.details.append(QStringLiteral("step %1: the check at line %2 failed").arg(currentStep).arg(line));
  };
  const auto random_row = [&] { return row_key(draw(0, kRowKeyDomain)); };
  const auto random_column = [&] { return column_key(draw(0, kColumnKeyDomain)); };
  const auto random_rows = [&] {
    QList<QString> keys;
    for (int i = draw(1, kLongestMatrixBatch + 1); i > 0; --i) keys.append(random_row());
    return keys;
  };
  const auto random_columns = [&] {
    QList<QString> keys;
    for (int i = draw(1, kLongestMatrixBatch + 1); i > 0; --i) keys.append(random_column());
    return keys;
  };
  const auto forget_row = [&](const QString& key) {
    const auto at = shadow.rowIndex(key);
    if (!at) return;
    const int id = shadow.rows()[static_cast<std::size_t>(*at)].id;
    std::erase_if(selectedCells, [id](const auto& cell) { return cell.first == id; });
    if (trackedIds && trackedIds->first == id) trackedIds.reset();
  };
  const auto forget_column = [&](const QString& key) {
    const auto at = shadow.columnIndex(key);
    if (!at) return;
    const int id = shadow.columns()[static_cast<std::size_t>(*at)].id;
    std::erase_if(selectedCells, [id](const auto& cell) { return cell.second == id; });
    if (trackedIds && trackedIds->second == id) trackedIds.reset();
  };

  for (int step = 0; step < steps; ++step) {
    ++report.steps;
    currentStep = step;
    const int kind = draw(0, kMatrixKinds);
    int edge = 0;
    const auto at = [&](int weight) { return kind < (edge += weight); };
    bool accepted = true;

    if (at(kWeightAddRow)) {
      const QString key = random_row();
      accepted = matrix.addRow(key);
      EXPECT(accepted == shadow.addRow(key));
    } else if (at(kWeightAddColumn)) {
      const QString key = random_column();
      accepted = matrix.addColumn(key);
      EXPECT(accepted == shadow.addColumn(key));
    } else if (at(kWeightAddRows)) {
      const QList<QString> keys = random_rows();
      qsizetype expected = 0;
      for (const QString& key : keys) expected += shadow.addRow(key);
      accepted = matrix.addRows(keys) == expected && expected > 0;
      EXPECT(matrix.rowCount() == static_cast<qsizetype>(shadow.rows().size()));
    } else if (at(kWeightAddColumns)) {
      const QList<QString> keys = random_columns();
      qsizetype expected = 0;
      for (const QString& key : keys) expected += shadow.addColumn(key);
      accepted = matrix.addColumns(keys) == expected && expected > 0;
      EXPECT(matrix.columnCount() == static_cast<qsizetype>(shadow.columns().size()));
    } else if (at(kWeightRemoveRow)) {
      const QString key = random_row();
      forget_row(key);
      accepted = matrix.removeRow(key);
      EXPECT(accepted == shadow.removeRow(key));
    } else if (at(kWeightRemoveColumn)) {
      const QString key = random_column();
      forget_column(key);
      accepted = matrix.removeColumn(key);
      EXPECT(accepted == shadow.removeColumn(key));
    } else if (at(kWeightRemoveRows)) {
      const QList<QString> keys = random_rows();
      qsizetype expected = 0;
      for (const QString& key : keys) {
        forget_row(key);
        expected += shadow.removeRow(key);
      }
      accepted = matrix.removeRows(keys) == expected && expected > 0;
    } else if (at(kWeightRemoveColumns)) {
      const QList<QString> keys = random_columns();
      qsizetype expected = 0;
      for (const QString& key : keys) {
        forget_column(key);
        expected += shadow.removeColumn(key);
      }
      accepted = matrix.removeColumns(keys) == expected && expected > 0;
    } else if (at(kWeightRenameRow)) {
      const QString from = random_row();
      const QString to = random_row();
      accepted = matrix.renameRow(from, to);
      EXPECT(accepted == shadow.renameRow(from, to));
    } else if (at(kWeightRenameColumn)) {
      const QString from = random_column();
      const QString to = random_column();
      accepted = matrix.renameColumn(from, to);
      EXPECT(accepted == shadow.renameColumn(from, to));
    } else if (at(kWeightSetValue)) {
      const QString row = random_row();
      const QString column = random_column();
      const int v = draw(0, kCellValues);
      accepted = matrix.setValue(row, column, v);
      EXPECT(accepted == shadow.set(row, column, v).has_value());
    } else if (at(kWeightSetValues)) {
      QList<std::tuple<QString, QString, int>> cells;
      for (int i = draw(1, kLongestMatrixBatch * 3); i > 0; --i) cells.append({random_row(), random_column(), draw(0, kCellValues)});
      // The last of a repeated cell wins, a cell that would not change is not counted.
      std::map<std::pair<QString, QString>, int> last;
      for (const auto& [row, column, v] : cells)
        if (shadow.rowIndex(row) && shadow.columnIndex(column)) last[{row, column}] = v;
      qsizetype expected = 0;
      for (const auto& [position, v] : last) expected += shadow.set(position.first, position.second, v).value_or(false);
      EXPECT(matrix.setValues(cells) == expected);
      accepted = expected > 0;
    } else if (at(kWeightSelect)) {
      if (!shadow.rows().empty() && !shadow.columns().empty()) {
        const int row = draw(0, static_cast<int>(shadow.rows().size()));
        const int column = draw(0, static_cast<int>(shadow.columns().size()));
        const std::pair<int, int> ids{shadow.rows()[static_cast<std::size_t>(row)].id, shadow.columns()[static_cast<std::size_t>(column)].id};
        selection.select(model->index(row, column), QItemSelectionModel::Toggle);
        if (!selectedCells.erase(ids)) selectedCells.insert(ids);
      }
    } else if (at(kWeightTrack)) {
      if (!shadow.rows().empty() && !shadow.columns().empty()) {
        const int row = draw(0, static_cast<int>(shadow.rows().size()));
        const int column = draw(0, static_cast<int>(shadow.columns().size()));
        tracked = model->index(row, column);
        trackedIds = std::pair{shadow.rows()[static_cast<std::size_t>(row)].id, shadow.columns()[static_cast<std::size_t>(column)].id};
      }
    } else {
      selectedCells.clear();
      trackedIds.reset();
      accepted = matrix.clear();
      EXPECT(accepted == (!shadow.rows().empty() || !shadow.columns().empty()));
      shadow.clear();
    }
    ++(accepted ? report.applied : report.refused);
    mirror.check();

    // The matrix holds what the reference holds, cell by cell.
    EXPECT(matrix.rowCount() == static_cast<qsizetype>(shadow.rows().size()) && matrix.columnCount() == static_cast<qsizetype>(shadow.columns().size()));
    for (std::size_t i = 0; i < shadow.rows().size() && static_cast<qsizetype>(i) < matrix.rowCount(); ++i) {
      EXPECT(matrix.rowKeys()[static_cast<qsizetype>(i)] == shadow.rows()[i].key && matrix.rowIndexOf(shadow.rows()[i].key) == static_cast<qsizetype>(i));
      for (std::size_t j = 0; j < shadow.columns().size() && static_cast<qsizetype>(j) < matrix.columnCount(); ++j)
        EXPECT(matrix.valueAt(static_cast<qsizetype>(i), static_cast<qsizetype>(j)) == shadow.value(shadow.rows()[i], shadow.columns()[j]));
    }
    for (std::size_t j = 0; j < shadow.columns().size() && static_cast<qsizetype>(j) < matrix.columnCount(); ++j)
      EXPECT(matrix.columnKeys()[static_cast<qsizetype>(j)] == shadow.columns()[j].key && matrix.columnIndexOf(shadow.columns()[j].key) == static_cast<qsizetype>(j));
    // The model shows it, keys and cells.
    EXPECT(model->rowCount() == static_cast<int>(shadow.rows().size()) && model->columnCount() == static_cast<int>(shadow.columns().size()));
    for (int row = 0; row < model->rowCount() && row < static_cast<int>(shadow.rows().size()); ++row) {
      EXPECT(model->headerData(row, Qt::Vertical).toString() == shadow.rows()[static_cast<std::size_t>(row)].key);
      for (int column = 0; column < model->columnCount() && column < static_cast<int>(shadow.columns().size()); ++column) {
        const int expected = shadow.value(shadow.rows()[static_cast<std::size_t>(row)], shadow.columns()[static_cast<std::size_t>(column)]);
        const QVariant shown = model->data(model->index(row, column));
        EXPECT(expected == kUnset ? !shown.isValid() : shown.toInt() == expected);
      }
    }
    // The selection follows its cells through every change, a rename included.
    std::set<std::pair<int, int>> selectedNow;
    for (const QModelIndex& index : selection.selectedIndexes())
      if (index.row() < static_cast<int>(shadow.rows().size()) && index.column() < static_cast<int>(shadow.columns().size()))
        selectedNow.insert({shadow.rows()[static_cast<std::size_t>(index.row())].id, shadow.columns()[static_cast<std::size_t>(index.column())].id});
    EXPECT(selectedNow == selectedCells);
    // A persistent index follows its cell, and is invalid once the row or column is gone.
    if (trackedIds) {
      const auto row = std::find_if(shadow.rows().begin(), shadow.rows().end(), [&](const MatrixShadow::Line& l) { return l.id == trackedIds->first; });
      const auto column = std::find_if(shadow.columns().begin(), shadow.columns().end(), [&](const MatrixShadow::Line& l) { return l.id == trackedIds->second; });
      EXPECT(row != shadow.rows().end() && column != shadow.columns().end() && tracked.isValid() &&
             tracked.row() == row - shadow.rows().begin() && tracked.column() == column - shadow.columns().begin());
    } else {
      EXPECT(!tracked.isValid());
    }
  }
  report.violations = mirror.violations();
  return report;
}

#undef EXPECT

}  // namespace observable_test
