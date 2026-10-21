// Model protocol, part 4: index() and the questions asked of an index. A row equal to rowCount(), negative rows, a
// column out of range, a valid parent; the counts, parent(), sibling() and the identity of an index.
// QAbstractItemModelTester runs in fatal mode in every fixture. Catalogue rows: docs/qt-edge-cases/protocol.md
// (P-28 to P-34).
#include "protocol_support.hpp"

#include <climits>

namespace {
using namespace edge;

// P-28
void index_of_a_row_equal_to_row_count_or_negative_is_invalid() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const int count = fx.rows();
        for (const int row : {-1, -kSmallFill, INT_MIN, count, count + 1, INT_MAX}) {
          KCHECK(!fx.model->index(row, 0).isValid());
          KCHECK(!fx.model->hasIndex(row, 0));
        }
        for (int row = 0; row < count; ++row) {
          KCHECK(fx.model->index(row, 0).isValid() && fx.model->hasIndex(row, 0));
          KCHECK(fx.model->index(row, 0).row() == row);
        }
      },
      false, kSmallFill);
  for_each_model([](auto& fx, const std::string& kind) {  // an empty model has no first row
    KCHECK(!fx.model->index(0, 0).isValid() && !fx.model->hasIndex(0, 0));
  });
}

// P-29
void index_of_a_column_out_of_range_is_invalid() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const int columns = fx.model->columnCount();
        KCHECK(columns == (fx.table ? kTableColumns : 1));
        for (int column = 0; column < columns; ++column) KCHECK(fx.model->index(0, column).isValid() && fx.model->hasIndex(0, column));
        for (const int column : {-1, columns, columns + 1, INT_MAX, INT_MIN}) {
          KCHECK(!fx.model->index(0, column).isValid());
          KCHECK(!fx.model->hasIndex(0, column));
        }
      },
      false, kSmallFill);
}

// P-30
void index_with_a_valid_parent_is_invalid() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        for (int row = 0; row < fx.rows(); ++row) {
          for (int column = 0; column < fx.model->columnCount(); ++column) {
            const QModelIndex parent = fx.model->index(row, column);
            KCHECK(!fx.model->index(0, 0, parent).isValid());
            KCHECK(!fx.model->hasIndex(0, 0, parent));
          }
        }
      },
      false, kSmallFill);
}

// P-31
void counts_and_children_of_a_valid_parent() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        KCHECK(fx.model->columnCount() == (fx.table ? kTableColumns : 1));
        KCHECK(fx.model->hasChildren());
        for (int row = 0; row < fx.rows(); ++row) {
          for (int column = 0; column < fx.model->columnCount(); ++column) {
            const QModelIndex cell = fx.model->index(row, column);
            KCHECK(fx.model->rowCount(cell) == 0 && fx.model->columnCount(cell) == 0);
            KCHECK(!fx.model->hasChildren(cell));
            KCHECK(!fx.model->canFetchMore(cell));
          }
        }
        KCHECK(!fx.model->canFetchMore(QModelIndex()));
        KCHECK(fx.model->rowCount(QModelIndex()) == kSmallFill);
        fx.source.clear();
        KCHECK(!fx.model->hasChildren());  // a root without rows has no children
      },
      false, kSmallFill);
}

// P-32
void parent_of_every_index_is_invalid() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        KCHECK(!fx.model->parent(QModelIndex()).isValid());
        for (int row = 0; row < fx.rows(); ++row)
          for (int column = 0; column < fx.model->columnCount(); ++column) KCHECK(!fx.model->parent(fx.model->index(row, column)).isValid());
      },
      false, kSmallFill);
}

// P-33
void sibling_and_sibling_at_column() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const int columns = fx.model->columnCount();
        const QModelIndex cell = fx.model->index(1, 0);
        for (int row = 0; row < kSmallFill; ++row) {
          const QModelIndex sibling = fx.model->sibling(row, 0, cell);
          KCHECK(sibling.isValid() && sibling.row() == row && sibling.column() == 0);
          KCHECK(cell.siblingAtRow(row) == sibling);
        }
        for (const int row : {-1, kSmallFill, INT_MAX}) {
          KCHECK(!fx.model->sibling(row, 0, cell).isValid());
          KCHECK(!cell.siblingAtRow(row).isValid());
        }
        for (int column = 0; column < columns; ++column) {
          KCHECK(fx.model->sibling(1, column, cell).isValid() && cell.siblingAtColumn(column).column() == column);
          KCHECK(cell.siblingAtColumn(column).row() == 1);
        }
        for (const int column : {-1, columns, INT_MAX}) {
          KCHECK(!fx.model->sibling(1, column, cell).isValid());
          KCHECK(!cell.siblingAtColumn(column).isValid());
        }
        KCHECK(fx.model->sibling(0, 0, QModelIndex()) == fx.model->index(0, 0));  // Qt: an invalid index is a root's child
        KCHECK(!QModelIndex().siblingAtColumn(0).isValid() && !QModelIndex().siblingAtRow(0).isValid());
      },
      false, kSmallFill);
}

// P-34
void index_identity() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        for (int row = 0; row < fx.rows(); ++row) {
          for (int column = 0; column < fx.model->columnCount(); ++column) {
            const QModelIndex a = fx.model->index(row, column);
            const QModelIndex b = fx.model->index(row, column);
            KCHECK(a == b && !(a != b) && !(a < b) && !(b < a));
            KCHECK(a.model() == fx.model.get() && a.row() == row && a.column() == column);
            KCHECK(a.internalPointer() == nullptr && !a.parent().isValid());
            KCHECK(QPersistentModelIndex(a) == a && QPersistentModelIndex(a) == QPersistentModelIndex(b));
            KCHECK(qHash(a) == qHash(b));
            KCHECK(row + 1 == fx.rows() || a != fx.model->index(row + 1, column));
          }
        }
        KCHECK(!QModelIndex().isValid() && QModelIndex().model() == nullptr);
        KCHECK(QModelIndex() == fx.model->index(fx.rows(), 0));  // every invalid index is the same index
      },
      false, kSmallFill);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  index_of_a_row_equal_to_row_count_or_negative_is_invalid();
  index_of_a_column_out_of_range_is_invalid();
  index_with_a_valid_parent_is_invalid();
  counts_and_children_of_a_valid_parent();
  parent_of_every_index_is_invalid();
  sibling_and_sibling_at_column();
  index_identity();
  return rqt_test::finish("capability_edge_protocol_indexes");
}
