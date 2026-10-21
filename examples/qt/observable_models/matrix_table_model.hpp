// A QAbstractTableModel for an ObservableMatrix, driven by the matrix's typed signals: the begin and end calls are paired
// here, so a view keeps its selection and scroll position through every change of structure.
//
// A rename is a header change (headerDataChanged), never a move, a remove plus an insert, or a reset. A change of
// cells is one dataChanged over exactly the rectangle the matrix announced. Rows and columns come and go one pair per
// run, and the rows and columns that a begin names are still readable at that moment.
//
// The model counts its own rows and columns, because between the matrix's own signal and the model's the two differ
// (a receiver connected before the model sees the matrix changed and the model not yet), and every read checks both:
// a cell outside either reads as empty.
//
// The model holds its matrix by QPointer. When the matrix is destroyed first the model resets to empty from the
// matrix's destroyed signal, so every call stays safe. A model made while the matrix is announcing a change did not
// hear the "about" signal: at the "done" signal it resets to the matrix's content instead of ending a change it never
// began. The display role and the edit role give the cell's value (an invalid QVariant for an uninitialised cell), the
// header gives the key of the row or column; the model is read only.
//
// A model and its matrix live in one thread; a debug build asserts it.
#pragma once

#include "observable_matrix.hpp"

#include <QtCore/QAbstractTableModel>
#include <QtCore/QModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QVariant>

#include <algorithm>

namespace observable {

template <class M>
class MatrixTableModel : public QAbstractTableModel {
 public:
  using RowKey = typename M::RowKeyType;
  using ColKey = typename M::ColKeyType;
  using Value = typename M::ValueType;

  explicit MatrixTableModel(M& source, QObject* parent = nullptr) : QAbstractTableModel(parent), m_source(&source) {
    Q_ASSERT_X(source.thread() == this->thread(), "observable::MatrixTableModel",
               "a model must live in the thread of its matrix (move data between threads, not the model)");
    takeContent();
    connectSource();
  }

  int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : m_rows; }
  int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : m_columns; }

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    // An invalid index is a question QAbstractItemModelTester asks on purpose: answer it before checkIndex logs it.
    if (!m_source || !index.isValid() || !checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) return {};
    if (index.row() >= m_source->rowCount() || index.column() >= m_source->columnCount()) return {};  // the matrix is ahead
    return cellData(m_source->valueAt(index.row(), index.column()), role);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
    if (!m_source || role != Qt::DisplayRole || section < 0) return {};
    if (orientation == Qt::Vertical)
      return section < std::min<qsizetype>(m_rows, m_source->rowCount()) ? QVariant::fromValue(m_source->rowKeys()[section]) : QVariant{};
    return section < std::min<qsizetype>(m_columns, m_source->columnCount()) ? QVariant::fromValue(m_source->columnKeys()[section]) : QVariant{};
  }

 protected:
  // What a cell shows for a role; override it to format. The default shows the value for the display and the edit role.
  virtual QVariant cellData(const Value& value, int role) const {
    if ((role != Qt::DisplayRole && role != Qt::EditRole) || m_source->isUninitialized(value)) return {};
    return QVariant::fromValue(value);
  }

 private:
  // The matrix's content in one reset: for a matrix that is gone, the empty content.
  void resync() {
    beginResetModel();
    takeContent();
    endResetModel();
  }

  void takeContent() {
    m_open = false;
    m_rows = m_source ? to_row(m_source->rowCount()) : 0;
    m_columns = m_source ? to_row(m_source->columnCount()) : 0;
  }

  // The end of a change whose beginning this model heard. A model made between the two signals did not: it follows the
  // matrix with a reset, and the caller has nothing left to do.
  bool heardTheBeginning() {
    if (m_open) {
      m_open = false;
      return true;
    }
    resync();
    return false;
  }

  void begin() { m_open = true; }

  void finishRowsAdded(qsizetype first, qsizetype last) {
    if (!heardTheBeginning()) return;
    m_rows += to_row(last - first + 1);
    endInsertRows();
  }
  void finishRowsRemoved(qsizetype first, qsizetype last) {
    if (!heardTheBeginning()) return;
    m_rows -= to_row(last - first + 1);
    endRemoveRows();
  }
  void finishColumnsAdded(qsizetype first, qsizetype last) {
    if (!heardTheBeginning()) return;
    m_columns += to_row(last - first + 1);
    endInsertColumns();
  }
  void finishColumnsRemoved(qsizetype first, qsizetype last) {
    if (!heardTheBeginning()) return;
    m_columns -= to_row(last - first + 1);
    endRemoveColumns();
  }

  void connectStructure() {
    using S = M;
    QObject::connect(m_source, &S::rowAboutToBeAdded, this, [this](const RowKey&, qsizetype i) {
      begin();
      beginInsertRows({}, to_row(i), to_row(i));
    });
    QObject::connect(m_source, &S::rowAdded, this, [this](const RowKey&, qsizetype i) { finishRowsAdded(i, i); });
    QObject::connect(m_source, &S::rowsAboutToBeAdded, this, [this](qsizetype first, qsizetype last, const QList<RowKey>&) {
      begin();
      beginInsertRows({}, to_row(first), to_row(last));
    });
    QObject::connect(m_source, &S::rowsAdded, this, [this](qsizetype first, qsizetype last, const QList<RowKey>&) { finishRowsAdded(first, last); });
    QObject::connect(m_source, &S::rowAboutToBeRemoved, this, [this](const RowKey&, qsizetype i) {
      begin();
      beginRemoveRows({}, to_row(i), to_row(i));
    });
    QObject::connect(m_source, &S::rowRemoved, this, [this](const RowKey&, qsizetype i) { finishRowsRemoved(i, i); });
    QObject::connect(m_source, &S::rowsAboutToBeRemoved, this, [this](qsizetype first, qsizetype last, const QList<RowKey>&) {
      begin();
      beginRemoveRows({}, to_row(first), to_row(last));
    });
    QObject::connect(m_source, &S::rowsRemoved, this, [this](qsizetype first, qsizetype last, const QList<RowKey>&) { finishRowsRemoved(first, last); });

    QObject::connect(m_source, &S::columnAboutToBeAdded, this, [this](const ColKey&, qsizetype j) {
      begin();
      beginInsertColumns({}, to_row(j), to_row(j));
    });
    QObject::connect(m_source, &S::columnAdded, this, [this](const ColKey&, qsizetype j) { finishColumnsAdded(j, j); });
    QObject::connect(m_source, &S::columnsAboutToBeAdded, this, [this](qsizetype first, qsizetype last, const QList<ColKey>&) {
      begin();
      beginInsertColumns({}, to_row(first), to_row(last));
    });
    QObject::connect(m_source, &S::columnsAdded, this, [this](qsizetype first, qsizetype last, const QList<ColKey>&) { finishColumnsAdded(first, last); });
    QObject::connect(m_source, &S::columnAboutToBeRemoved, this, [this](const ColKey&, qsizetype j) {
      begin();
      beginRemoveColumns({}, to_row(j), to_row(j));
    });
    QObject::connect(m_source, &S::columnRemoved, this, [this](const ColKey&, qsizetype j) { finishColumnsRemoved(j, j); });
    QObject::connect(m_source, &S::columnsAboutToBeRemoved, this, [this](qsizetype first, qsizetype last, const QList<ColKey>&) {
      begin();
      beginRemoveColumns({}, to_row(first), to_row(last));
    });
    QObject::connect(m_source, &S::columnsRemoved, this, [this](qsizetype first, qsizetype last, const QList<ColKey>&) { finishColumnsRemoved(first, last); });
  }

  void connectContent() {
    using S = M;
    // A rename is a header change.
    QObject::connect(m_source, &S::rowRenamed, this, [this](const RowKey&, const RowKey&, qsizetype i) {
      if (i < m_rows) headerDataChanged(Qt::Vertical, to_row(i), to_row(i));
    });
    QObject::connect(m_source, &S::columnRenamed, this, [this](const ColKey&, const ColKey&, qsizetype j) {
      if (j < m_columns) headerDataChanged(Qt::Horizontal, to_row(j), to_row(j));
    });
    QObject::connect(m_source, &S::cellChanged, this, [this](const RowKey&, const ColKey&, qsizetype i, qsizetype j) { changed(i, j, i, j); });
    QObject::connect(m_source, &S::cellsChanged, this, [this](qsizetype top, qsizetype left, qsizetype bottom, qsizetype right) { changed(top, left, bottom, right); });

    QObject::connect(m_source, &S::aboutToBeCleared, this, [this] {
      begin();
      beginResetModel();
    });
    QObject::connect(m_source, &S::cleared, this, [this] {
      if (!heardTheBeginning()) return;
      m_rows = 0;
      m_columns = 0;
      endResetModel();
    });
  }

  void connectSource() {
    QObject::connect(m_source, &QObject::destroyed, this, [this] { resync(); });
    connectStructure();
    connectContent();
  }

  // A rectangle of cells, when the model has all of it: never an invalid index.
  void changed(qsizetype top, qsizetype left, qsizetype bottom, qsizetype right) {
    if (bottom >= m_rows || right >= m_columns) return;
    emit dataChanged(index(to_row(top), to_row(left)), index(to_row(bottom), to_row(right)));
  }

  QPointer<M> m_source;
  int m_rows = 0;
  int m_columns = 0;
  bool m_open = false;
};

}  // namespace observable
