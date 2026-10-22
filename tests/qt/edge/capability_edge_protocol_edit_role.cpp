// Model protocol, fixed gap P-45: an editable column answers Qt::EditRole with what it displays, so the default
// delegate of a view opens the editor of the value's type on the value and commits through setData. A column without an
// editor still serves display only. Catalogue row: docs/qt-edge-cases/protocol.md (P-45).
#include "protocol_support.hpp"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyledItemDelegate>

namespace {
using namespace edge;

constexpr int kTyped = 42;  // what a user types into the editor

// P-45
void edit_role_of_an_editable_cell_is_its_display() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        QStyledItemDelegate delegate;
        for (int row = 0; row < fx.rows(); ++row) {
          const QModelIndex cell = fx.index(row, 0);
          KCHECK(fx.model->flags(cell).testFlag(Qt::ItemIsEditable));
          KCHECK(fx.model->data(cell, Qt::EditRole).isValid());
          KCHECK(fx.model->data(cell, Qt::EditRole) == fx.model->data(cell, Qt::DisplayRole));
          KCHECK(fx.model->itemData(cell).value(Qt::EditRole) == fx.model->data(cell, Qt::DisplayRole));
          QWidget* editor = delegate.createEditor(nullptr, QStyleOptionViewItem(), cell);
          KCHECK(editor != nullptr);
          if (editor) {
            const QMetaProperty user = editor->metaObject()->userProperty();  // whatever editor the value's type asks for
            delegate.setEditorData(editor, cell);
            KCHECK(user.read(editor).toInt() == fx.shown_at(row));              // the editor opens on the value
            KCHECK(user.write(editor, kTyped));
            delegate.setModelData(editor, fx.model.get(), cell);
            KCHECK((fx.shown_at(row) == kTyped) == Fx::assignable);              // and commits through setData (a set refuses)
          }
          delete editor;
          if constexpr (Fx::table) {
            KCHECK(!fx.model->data(fx.index(row, 1), Qt::EditRole).isValid());  // the column with no editor
            KCHECK(fx.model->data(fx.index(row, 1), Qt::DisplayRole).isValid());
          }
        }
      },
      true, kSmallFill);
  for_each_model(
      [](auto& fx, const std::string& kind) {
        for (int row = 0; row < fx.rows(); ++row)
          for (int column = 0; column < fx.model->columnCount(); ++column) KCHECK(!fx.model->data(fx.index(row, column), Qt::EditRole).isValid());
      },
      false, kSmallFill);
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  edit_role_of_an_editable_cell_is_its_display();
  return rqt_test::finish("capability_edge_protocol_edit_role");
}
