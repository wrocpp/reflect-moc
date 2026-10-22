// Matrix adapters with one deliberate mistake each, the kind that a hand-written table model makes when it follows a matrix:
// a rename announced as a removal and an insertion, a setValues announced as the bounding box of the changed cells, a column
// added with no columnsInserted. They exist so that the matrix driver is shown to fail on a wrong model, and not only to
// pass on a right one.
#pragma once

#include "observable_matrix_support.hpp"

#include <algorithm>

namespace observable_test {

enum class MatrixFlaw {
  none,
  rename_as_remove_insert,  // a renamed row is removed and inserted again: its selection and persistent indexes are lost
  bounding_box,             // setValues announces one dataChanged over the bounding box of its rectangles
  column_without_signal,    // a column is added and the model announces nothing
};

inline const char* name_of(MatrixFlaw flaw) {
  switch (flaw) {
    case MatrixFlaw::none: return "none";
    case MatrixFlaw::rename_as_remove_insert: return "rename_as_remove_insert";
    case MatrixFlaw::bounding_box: return "bounding_box";
    case MatrixFlaw::column_without_signal: return "column_without_signal";
  }
  return "?";
}

class FlawedMatrixModel : public QAbstractTableModel {
 public:
  FlawedMatrixModel(IntMatrix* source, MatrixFlaw flaw) : m_source(source), m_flaw(flaw) {
    using S = IntMatrix;
    connect(source, &S::rowsAboutToBeAdded, this, [this](qsizetype f, qsizetype l, const QList<QString>&) { beginInsertRows({}, int(f), int(l)); });
    connect(source, &S::rowsAdded, this, [this](qsizetype f, qsizetype l, const QList<QString>&) { m_rows += int(l - f + 1); endInsertRows(); });
    connect(source, &S::rowAboutToBeAdded, this, [this](const QString&, qsizetype i) { beginInsertRows({}, int(i), int(i)); });
    connect(source, &S::rowAdded, this, [this](const QString&, qsizetype) { ++m_rows; endInsertRows(); });
    connect(source, &S::columnsAboutToBeAdded, this, [this](qsizetype f, qsizetype l, const QList<QString>&) { if (m_flaw != MatrixFlaw::column_without_signal) beginInsertColumns({}, int(f), int(l)); });
    connect(source, &S::columnsAdded, this, [this](qsizetype f, qsizetype l, const QList<QString>&) {
      m_columns += int(l - f + 1);
      if (m_flaw != MatrixFlaw::column_without_signal) endInsertColumns();
    });
    connect(source, &S::columnAboutToBeAdded, this, [this](const QString&, qsizetype i) { if (m_flaw != MatrixFlaw::column_without_signal) beginInsertColumns({}, int(i), int(i)); });
    connect(source, &S::columnAdded, this, [this](const QString&, qsizetype) {
      ++m_columns;
      if (m_flaw != MatrixFlaw::column_without_signal) endInsertColumns();
    });
    connect(source, &S::rowsAboutToBeRemoved, this, [this](qsizetype f, qsizetype l, const QList<QString>&) { beginRemoveRows({}, int(f), int(l)); });
    connect(source, &S::rowsRemoved, this, [this](qsizetype f, qsizetype l, const QList<QString>&) { m_rows -= int(l - f + 1); endRemoveRows(); });
    connect(source, &S::rowAboutToBeRemoved, this, [this](const QString&, qsizetype i) { beginRemoveRows({}, int(i), int(i)); });
    connect(source, &S::rowRemoved, this, [this](const QString&, qsizetype) { --m_rows; endRemoveRows(); });
    connect(source, &S::columnsAboutToBeRemoved, this, [this](qsizetype f, qsizetype l, const QList<QString>&) { beginRemoveColumns({}, int(f), int(l)); });
    connect(source, &S::columnsRemoved, this, [this](qsizetype f, qsizetype l, const QList<QString>&) { m_columns -= int(l - f + 1); endRemoveColumns(); });
    connect(source, &S::columnAboutToBeRemoved, this, [this](const QString&, qsizetype i) { beginRemoveColumns({}, int(i), int(i)); });
    connect(source, &S::columnRemoved, this, [this](const QString&, qsizetype) { --m_columns; endRemoveColumns(); });

    connect(source, &S::rowAboutToBeRenamed, this, [this](const QString&, const QString&, qsizetype i) {
      if (m_flaw == MatrixFlaw::rename_as_remove_insert) beginRemoveRows({}, int(i), int(i));
    });
    connect(source, &S::rowRenamed, this, [this](const QString&, const QString&, qsizetype i) {
      if (m_flaw != MatrixFlaw::rename_as_remove_insert) return headerDataChanged(Qt::Vertical, int(i), int(i));
      --m_rows;
      endRemoveRows();
      beginInsertRows({}, int(i), int(i));
      ++m_rows;
      endInsertRows();
    });
    connect(source, &S::columnRenamed, this, [this](const QString&, const QString&, qsizetype i) { headerDataChanged(Qt::Horizontal, int(i), int(i)); });

    connect(source, &S::cellChanged, this, [this](const QString&, const QString&, qsizetype i, qsizetype j) { dataChanged(index(int(i), int(j)), index(int(i), int(j))); });
    connect(source, &S::cellsAboutToChange, this, [this](qsizetype t, qsizetype l, qsizetype b, qsizetype r) {
      if (m_pending.empty()) m_box = {int(t), int(l), int(b), int(r)};
      m_box = {std::min(m_box[0], int(t)), std::min(m_box[1], int(l)), std::max(m_box[2], int(b)), std::max(m_box[3], int(r))};
      m_pending.push_back({int(t), int(l), int(b), int(r)});
    });
    connect(source, &S::cellsChanged, this, [this](qsizetype t, qsizetype l, qsizetype b, qsizetype r) {
      if (m_flaw != MatrixFlaw::bounding_box) return dataChanged(index(int(t), int(l)), index(int(b), int(r)));
      if (m_pending.empty()) return;  // the bounding box was announced with the first twin
      dataChanged(index(m_box[0], m_box[1]), index(m_box[2], m_box[3]));
      m_pending.clear();
    });
    connect(source, &S::aboutToBeCleared, this, [this] { beginResetModel(); });
    connect(source, &S::cleared, this, [this] {
      m_rows = 0;
      m_columns = 0;
      endResetModel();
    });
  }

  int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : m_rows; }
  int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : m_columns; }
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || role != Qt::DisplayRole || index.row() >= m_source->rowCount() || index.column() >= m_source->columnCount()) return {};
    const int v = m_source->valueAt(index.row(), index.column());
    return m_source->isUninitialized(v) ? QVariant() : QVariant(v);
  }
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
    if (role != Qt::DisplayRole || section < 0) return {};
    if (orientation == Qt::Vertical) return section < m_source->rowCount() ? QVariant(m_source->rowKeys()[section]) : QVariant();
    return section < m_source->columnCount() ? QVariant(m_source->columnKeys()[section]) : QVariant();
  }

 private:
  IntMatrix* m_source;
  MatrixFlaw m_flaw;
  int m_rows = 0;
  int m_columns = 0;
  std::vector<std::array<int, 4>> m_pending;
  std::array<int, 4> m_box{};
};

}  // namespace observable_test
