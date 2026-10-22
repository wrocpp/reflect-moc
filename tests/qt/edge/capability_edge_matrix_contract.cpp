// Matrix edge cases: renames, NaN and other keys, the rectangles of setValues, a matrix with no rows or no columns, the last
// row and column, a column added to a matrix with rows, persistent indexes and selections through structure changes, a
// model that reads between the matrix's signal and its own. QAbstractItemModelTester runs in fatal mode on every model.
// Catalogue: docs/qt-edge-cases/matrix-selection.md (M-01 to M-11).
#include "matrix_support.hpp"
#include "observable_models_support.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPersistentModelIndex>

#include <limits>

namespace {
using namespace matrix_test;
using observable::MatrixTableModel;
using observable::ObservableMatrix;

QStringList rectangles(Recorder& rec) {
  QStringList out;
  for (const QString& line : rec.log)
    if (line.startsWith("cellsChanged")) out.append(line.mid(QStringLiteral("cellsChanged").size()));
  return out;
}

void fill_grid(Mat& m, int rows, int columns) {
  m.addRows(keys("r", rows));
  m.addColumns(keys("c", columns));
}

// M-01: a rename keeps the row's index, its cells, its persistent index and its selection; a rename onto a key in use, of an
// absent key or to a NaN is refused; a rename to the same key is true and silent; a swap goes through a spare key.
void a_rename_is_an_identity_that_keeps_everything() {
  Mat m(kNaN);
  m.addRows({"a", "b", "c"});
  m.addColumns({"x", "y"});
  m.setValue("b", "y", 5.0);
  MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  QItemSelectionModel selection(&model);
  QPersistentModelIndex cell = model.index(1, 1);
  selection.select(model.index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  Recorder rec(m);

  CHECK(m.renameRow("b", "b2"));
  CHECK(rec.log == (QStringList{"rowAboutToBeRenamed(b,b2,1)", "rowRenamed(b,b2,1)"}));
  CHECK(cell.isValid() && cell.row() == 1 && cell.column() == 1 && cell.data().toDouble() == 5.0);
  CHECK(selection.isRowSelected(1) && selection.selectedRows().size() == 1);
  CHECK(model.headerData(1, Qt::Vertical).toString() == "b2");
  CHECK(m.rowIndexOf("b2") == 1 && m.rowIndexOf("b") == -1 && m.value("b2", "y") == 5.0);

  rec.log.clear();
  CHECK(!m.renameRow("a", "c") && !m.renameRow("zz", "q") && !m.renameRow("a", "c"));
  CHECK(m.renameRow("a", "a") && !m.renameRow("zz", "zz"));
  CHECK(rec.log.isEmpty());
  // A swap: three renames, each one announced, and the cells go with their rows.
  m.setValue("a", "x", 1.0);
  m.setValue("c", "x", 3.0);
  CHECK(m.renameRow("a", "spare") && m.renameRow("c", "a") && m.renameRow("spare", "c"));
  CHECK(m.rowKeys() == (QList<QString>{"c", "b2", "a"}));
  CHECK(m.value("c", "x") == 1.0 && m.value("a", "x") == 3.0);
  CHECK(cell.isValid() && cell.row() == 1);

  // A column is the same.
  CHECK(m.renameColumn("y", "y2") && !m.renameColumn("x", "y2") && m.renameColumn("x", "x"));
  CHECK(model.headerData(1, Qt::Horizontal).toString() == "y2" && cell.column() == 1 && cell.data().toDouble() == 5.0);
}

// M-02: a NaN key is refused in every way in: addRow, addColumn, addRows, addColumns, a rename. -0.0 and 0.0 are one key, and
// an infinity is a key.
void nan_keys_in_a_matrix() {
  ObservableMatrix<double, double, int> m(-1);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  MatrixTableModel<ObservableMatrix<double, double, int>> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  CHECK(!m.addRow(nan) && !m.addColumn(nan));
  CHECK(m.addRows({nan, 1.0, nan, 2.0}) == 2 && m.addColumns({nan, nan}) == 0);
  CHECK(m.addColumn(1.0) && m.rowCount() == 2 && m.columnCount() == 1);
  CHECK(!m.renameRow(1.0, nan) && !m.renameColumn(1.0, nan));
  CHECK(!m.hasRow(nan) && m.rowIndexOf(nan) == -1 && m.value(nan, 1.0) == -1);
  CHECK(!m.setValue(nan, 1.0, 5) && m.setValues({{nan, 1.0, 5}, {1.0, nan, 5}}) == 0);
  CHECK(m.removeRows({nan}) == 0 && !m.removeRow(nan));
  CHECK(m.addRow(-0.0) && !m.addRow(0.0) && m.hasRow(0.0) && m.hasRow(-0.0));
  const double inf = std::numeric_limits<double>::infinity();
  CHECK(m.addRow(inf) && m.addRow(-inf) && m.hasRow(inf));
  CHECK(model.rowCount() == 5);
}

// M-03: the rectangles of setValues. Rows that follow each other with the same columns make one; a gap in rows or a run that
// differs makes another; a cell in a rectangle that would not change breaks it; the last of a repeated cell wins.
void setValues_makes_full_rectangles() {
  {  // a staircase: every row is another extent, so one rectangle per row
    Mat m(kNaN);
    fill_grid(m, 4, 4);
    Recorder rec(m);
    m.setValues({{"r0", "c0", 1.0}, {"r1", "c0", 1.0}, {"r1", "c1", 1.0}, {"r2", "c0", 1.0}, {"r2", "c1", 1.0}, {"r2", "c2", 1.0}});
    CHECK(rectangles(rec) == (QStringList{"(0,0,0,0)", "(1,0,1,1)", "(2,0,2,2)"}));
  }
  {  // a gap inside a row: two runs, each its own rectangle, then they join the same runs of the next row
    Mat m(kNaN);
    fill_grid(m, 2, 5);
    Recorder rec(m);
    m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 1.0}, {"r0", "c3", 1.0}, {"r1", "c0", 1.0}, {"r1", "c1", 1.0}, {"r1", "c3", 1.0}});
    CHECK(rectangles(rec) == (QStringList{"(0,0,1,1)", "(0,3,1,3)"}));
  }
  {  // a cell that does not change in the middle of a block splits it: never a rectangle that names it
    Mat m(kNaN);
    fill_grid(m, 3, 3);
    for (const char* r : {"r0", "r1", "r2"})
      for (const char* c : {"c0", "c1", "c2"}) m.setValue(r, c, 0.0);
    Recorder rec(m);
    QList<std::tuple<QString, QString, double>> cells;
    for (const char* r : {"r0", "r1", "r2"})
      for (const char* c : {"c0", "c1", "c2"}) cells.append({r, c, !(QString(r) == "r1" && QString(c) == "c1") ? 1.0 : 0.0});
    CHECK(m.setValues(cells) == 8);
    QStringList rects = rectangles(rec);
    for (const QString& rect : rects) CHECK(rect != "(0,0,2,2)");  // the whole block is not one change
    int covered = 0;
    for (const QString& rect : rects) {
      const QStringList p = rect.mid(1, rect.size() - 2).split(',');
      covered += (p[2].toInt() - p[0].toInt() + 1) * (p[3].toInt() - p[1].toInt() + 1);
    }
    CHECK(covered == 8);  // together they are exactly the eight cells that changed
  }
  {  // the same extent on rows with a gap between them is two rectangles; an overlapping extent is another
    Mat m(kNaN);
    fill_grid(m, 5, 3);
    Recorder rec(m);
    m.setValues({{"r0", "c0", 1.0}, {"r0", "c1", 1.0}, {"r2", "c0", 1.0}, {"r2", "c1", 1.0}, {"r3", "c0", 1.0}, {"r3", "c1", 1.0}, {"r3", "c2", 1.0}});
    CHECK(rectangles(rec) == (QStringList{"(0,0,0,1)", "(2,0,2,1)", "(3,0,3,2)"}));
  }
  {  // a repeated cell: the last wins, also when it puts the old value back
    Mat m(kNaN);
    fill_grid(m, 1, 1);
    m.setValue("r0", "c0", 5.0);
    Recorder rec(m);
    CHECK(m.setValues({{"r0", "c0", 9.0}, {"r0", "c0", 5.0}}) == 0 && rec.log.isEmpty() && m.value("r0", "c0") == 5.0);
    CHECK(m.setValues({{"r0", "c0", 1.0}, {"r0", "c0", 2.0}, {"r0", "c0", 3.0}}) == 1 && m.value("r0", "c0") == 3.0);
    CHECK(m.setValues({}) == 0);
  }
}

// M-04: a matrix with rows and no columns, or columns and no rows: the model has a row count and no cells, adding the
// missing dimension gives every cell the uninitialised value, and removing the last row or column leaves the other.
void a_matrix_with_no_rows_or_no_columns() {
  Mat m(kNaN);
  MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  CHECK(model.rowCount() == 0 && model.columnCount() == 0 && !m.clear() && !m.setValue("r", "c", 1.0));
  CHECK(m.addRows({"r0", "r1"}) == 2);  // rows, no columns
  CHECK(model.rowCount() == 2 && model.columnCount() == 0 && !model.index(0, 0).isValid());
  CHECK(!m.setValue("r0", "c", 1.0) && std::isnan(m.value("r0", "c")));
  CHECK(m.addColumn("c0"));
  CHECK(model.columnCount() == 1 && !model.data(model.index(1, 0)).isValid());  // a new column holds the uninitialised value
  CHECK(m.removeColumn("c0") && m.columnCount() == 0 && m.rowCount() == 2 && model.rowCount() == 2);
  CHECK(m.removeRows({"r0", "r1"}) == 2 && model.rowCount() == 0);
  CHECK(m.addColumns({"c0", "c1"}) == 2);  // columns, no rows
  CHECK(model.rowCount() == 0 && model.columnCount() == 2 && model.headerData(1, Qt::Horizontal).toString() == "c1");
  CHECK(m.addRow("r0") && m.valueAt(0, 1) != m.valueAt(0, 1));  // the new row is all uninitialised (NaN)
  CHECK(m.clear() && model.rowCount() == 0 && model.columnCount() == 0);
  CHECK(m.addRow("x") && m.clear() && m.rowCount() == 0);  // a matrix of one row and no columns is not empty
  CHECK(!m.clear());
}

// M-05: a column added to a matrix with rows is announced once over the new range; no cell changes, every old cell keeps its
// value, and a persistent index keeps pointing at its cell. The same for a column in the middle removed.
void a_column_added_to_a_matrix_with_rows() {
  Mat m(kNaN);
  fill_grid(m, 3, 2);
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j) m.setValue(QStringLiteral("r%1").arg(i), QStringLiteral("c%1").arg(j), 10.0 * i + j);
  MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  QPersistentModelIndex cell = model.index(2, 1);
  QStringList seen;
  QObject::connect(&model, &QAbstractItemModel::columnsInserted, &model, [&](const QModelIndex&, int f, int l) { seen << QStringLiteral("inserted %1-%2").arg(f).arg(l); });
  QObject::connect(&model, &QAbstractItemModel::dataChanged, &model, [&] { seen << "dataChanged"; });
  CHECK(m.addColumns({"x", "y", "z"}) == 3);
  CHECK(seen == (QStringList{"inserted 2-4"}));
  CHECK(cell.isValid() && cell.row() == 2 && cell.column() == 1 && cell.data().toDouble() == 21.0);
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j) CHECK(m.valueAt(i, j) == 10.0 * i + j);
  CHECK(m.valueAt(1, 3) != m.valueAt(1, 3));
  CHECK(m.removeColumns({"c0", "y"}) == 2);  // runs [0,0] and [3,3]: removed from the last
  CHECK(cell.column() == 0 && cell.data().toDouble() == 21.0);
  CHECK(m.columnKeys() == (QList<QString>{"c1", "x", "z"}));
}

// M-06: removing the last rows or columns one run at a time, and every row at once, with a persistent index on a cell that
// goes and one that stays.
void the_last_row_and_column() {
  Mat m(kNaN);
  fill_grid(m, 4, 3);
  m.setValue("r3", "c2", 7.0);
  MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  QPersistentModelIndex last = model.index(3, 2);
  QPersistentModelIndex first = model.index(0, 0);
  CHECK(m.removeRow("r3") && !last.isValid() && first.isValid());
  CHECK(m.removeColumn("c2") && m.rowCount() == 3 && m.columnCount() == 2);
  CHECK(m.removeRows({"r0", "r1", "r2"}) == 3 && model.rowCount() == 0 && !first.isValid());
  CHECK(m.columnCount() == 2 && m.removeColumns({"c0", "c1"}) == 2 && model.columnCount() == 0);
  CHECK(!m.removeRow("r0") && !m.removeColumn("c0") && m.removeRows({}) == 0 && m.removeColumns({"zz"}) == 0);
}

struct Point {
  int x = 0;
  int y = 0;
  friend bool operator==(const Point&, const Point&) = default;
};
size_t qHash(const Point& p, size_t seed = 0) { return qHashMulti(seed, p.x, p.y); }
struct Unhashable {
  int v = 0;
  friend bool operator==(const Unhashable&, const Unhashable&) = default;
};

// M-07: a key is anything that is copied, compared and hashed: a number, a string, a struct with qHash. The header of the model
// carries the key as a QVariant that gives the key back. A key without qHash is refused when the type is named.
void key_types() {
  static_assert(observable::MatrixKey<int> && observable::MatrixKey<QString> && observable::MatrixKey<Point> && observable::MatrixKey<double>);
  static_assert(!observable::MatrixKey<Unhashable>);
  ObservableMatrix<Point, int, QString> m(QString{});
  MatrixTableModel<ObservableMatrix<Point, int, QString>> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  CHECK(m.addRows({{1, 2}, {3, 4}, {1, 2}}) == 2 && m.addColumn(7));
  CHECK(m.setValue({3, 4}, 7, "cell"));
  CHECK(model.headerData(1, Qt::Vertical).value<Point>() == (Point{3, 4}));
  CHECK(model.data(model.index(1, 0)).toString() == "cell" && !model.data(model.index(0, 0)).isValid());
  CHECK(m.renameRow({1, 2}, {9, 9}) && model.headerData(0, Qt::Vertical).value<Point>() == (Point{9, 9}));
}

// M-08: a persistent index and a selection follow their cells through every structural change, and are gone with the row or
// column that goes (a clear drops them all).
void persistent_indexes_and_selections() {
  Mat m(kNaN);
  fill_grid(m, 5, 4);
  MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  QItemSelectionModel selection(&model);
  QPersistentModelIndex cell = model.index(3, 2);  // r3 c2
  selection.select(model.index(3, 2), QItemSelectionModel::Select);
  CHECK(m.addRow("new") && m.addColumn("newc") && cell.row() == 3 && cell.column() == 2);
  CHECK(m.removeRows({"r0", "r1"}) == 2 && cell.row() == 1);
  CHECK(m.removeColumn("c0") && cell.column() == 1);
  CHECK(m.renameRow("r3", "renamed") && m.renameColumn("c2", "renamedc") && cell.row() == 1 && cell.column() == 1);
  CHECK(selection.isSelected(model.index(1, 1)) && selection.selectedIndexes().size() == 1);
  CHECK(m.setValue("renamed", "renamedc", 4.0) && cell.data().toDouble() == 4.0 && selection.isSelected(cell));
  CHECK(m.removeColumn("renamedc") && !cell.isValid() && !selection.hasSelection());
  CHECK(m.addColumn("again"));
  CHECK(!cell.isValid());  // a new column of the same position is a new cell
  m.clear();
  CHECK(!selection.hasSelection() && model.rowCount() == 0);
}

// M-09: a read between the matrix's signal and the model's: a receiver connected BEFORE the model sees the matrix changed and
// the model not yet. Every read of the model there is empty or right, never out of range.
void a_read_before_the_model_has_heard() {
  Mat m(kNaN);
  fill_grid(m, 4, 3);
  m.setValue("r3", "c2", 1.0);
  QStringList reads;
  QObject ctx;
  Mat* matrix = &m;
  MatrixTableModel<Mat>* seen = nullptr;
  const auto read_all = [&] {
    if (!seen) return;
    for (int row = 0; row < 6; ++row)
      for (int column = 0; column < 5; ++column) (void)seen->data(seen->index(row, column));
    for (int section = -1; section < 7; ++section) (void)seen->headerData(section, Qt::Vertical), (void)seen->headerData(section, Qt::Horizontal);
    reads << QStringLiteral("%1x%2/%3x%4").arg(seen->rowCount()).arg(seen->columnCount()).arg(matrix->rowCount()).arg(matrix->columnCount());
  };
  QObject::connect(&m, &Mat::rowsRemoved, &ctx, read_all);
  QObject::connect(&m, &Mat::columnsRemoved, &ctx, read_all);
  QObject::connect(&m, &Mat::rowsAdded, &ctx, read_all);
  QObject::connect(&m, &Mat::columnsAdded, &ctx, read_all);
  QObject::connect(&m, &Mat::cleared, &ctx, read_all);
  MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  seen = &model;
  CHECK(m.removeRows({"r0", "r1", "r3"}) == 3);
  CHECK(m.removeColumns({"c1"}) == 1);
  CHECK(m.addRows({"x", "y"}) == 2 && m.addColumns({"p"}) == 1);
  m.clear();
  CHECK(reads.size() == 5 + 1);  // two row runs, one column run, two adds, one clear
  seen = nullptr;
}

// M-10: the model is read only (no editable flag, setData says no), answers the edit role as the display role, answers nothing
// for another role or for an invalid index, and nothing for a cell that has no value.
void the_model_is_read_only() {
  Mat m(kNaN);
  fill_grid(m, 2, 2);
  m.setValue("r1", "c1", 3.5);
  MatrixTableModel<Mat> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  const QModelIndex cell = model.index(1, 1);
  CHECK(!model.flags(cell).testFlag(Qt::ItemIsEditable) && !model.setData(cell, 1.0));
  CHECK(model.data(cell, Qt::DisplayRole).toDouble() == 3.5 && model.data(cell, Qt::EditRole).toDouble() == 3.5);
  CHECK(!model.data(cell, Qt::UserRole).isValid() && !model.data(QModelIndex()).isValid());
  CHECK(!model.data(model.index(0, 0), Qt::EditRole).isValid());
  CHECK(!model.headerData(5, Qt::Vertical).isValid() && !model.headerData(-1, Qt::Horizontal).isValid());
  CHECK(!model.headerData(0, Qt::Vertical, Qt::EditRole).isValid());
  CHECK(std::isnan(m.value("zz", "c0")) && m.isUninitialized(m.value("zz", "c0")));
}

// M-11: a matrix whose uninitialised value is an int: equality decides what an unchanged cell is, and the model shows a cell
// that holds the uninitialised value as empty.
void an_integer_sentinel() {
  ObservableMatrix<QString, QString, int> m(-1);
  MatrixTableModel<ObservableMatrix<QString, QString, int>> model(m);
  QAbstractItemModelTester tester(&model, kTesterMode);
  m.addRow("r");
  m.addColumn("c");
  CHECK(!model.data(model.index(0, 0)).isValid());
  CHECK(m.setValue("r", "c", -1) && m.setValue("r", "c", 4) && model.data(model.index(0, 0)).toInt() == 4);
  CHECK(m.setValue("r", "c", -1) && !model.data(model.index(0, 0)).isValid());  // written back to the sentinel: empty again
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  a_rename_is_an_identity_that_keeps_everything();
  nan_keys_in_a_matrix();
  setValues_makes_full_rectangles();
  a_matrix_with_no_rows_or_no_columns();
  a_column_added_to_a_matrix_with_rows();
  the_last_row_and_column();
  key_types();
  persistent_indexes_and_selections();
  a_read_before_the_model_has_heard();
  the_model_is_read_only();
  an_integer_sentinel();
  return rqt_test::finish("capability_edge_matrix_contract");
}
