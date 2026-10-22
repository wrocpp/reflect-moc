// Model protocol, known gaps in what a model answers (docs/qt-edge-cases/protocol.md, P-45 to P-47). Each test passes
// while the gap exists and shows it; the fix flips it into a capability_edge_ test.
//
//   P-45  data(index, Qt::EditRole) is empty for a cell that flags() says is editable, so the default delegate
//         of a view opens an empty editor over a cell that shows a value.
//   P-46  data() answers an index that belongs to another model (and a column the model does not have) with the
//         data of its own row, where Qt asks a model to refuse it with checkIndex().
//   P-47  setData() accepts such an index too, and edits the row of this model that has the same number.
#include "protocol_support.hpp"

#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyledItemDelegate>

namespace {
using namespace edge;

// P-45
void limit_edit_role_is_empty_for_an_editable_cell() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        QStyledItemDelegate delegate;
        for (int row = 0; row < fx.rows(); ++row) {
          const QModelIndex cell = fx.index(row, 0);
          KCHECK(fx.model->flags(cell).testFlag(Qt::ItemIsEditable));      // the model says: edit me
          KCHECK(fx.model->data(cell, Qt::DisplayRole).isValid());
          KCHECK(!fx.model->data(cell, Qt::EditRole).isValid());           // the gap: a delegate asks for the edit role
          QWidget* editor = delegate.createEditor(nullptr, QStyleOptionViewItem(), cell);
          const auto* line = qobject_cast<QLineEdit*>(editor);
          KCHECK(line != nullptr);
          if (line) {
            delegate.setEditorData(editor, cell);
            KCHECK(line->text().isEmpty());                                // the gap: the editor opens empty, over a cell that shows a value
          }
          delete editor;
        }
      },
      true, kSmallFill);
}

// P-46
void limit_a_foreign_index_is_answered_with_this_models_own_data() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        Fx other;
        other.next = 10;
        other.fill(2);
        const QModelIndex foreign = other.index(1);
        MessageCatcher caught;
        KCHECK(!fx.model->checkIndex(foreign, QAbstractItemModel::CheckIndexOption::IndexIsValid));  // Qt: this is not ours
        KCHECK(caught.saw("another model") || !caught.messages().isEmpty());
        KCHECK(foreign.model() == other.model.get() && foreign.model() != fx.model.get());
        const QVariant answer = fx.model->data(foreign);
        KCHECK(answer.isValid());                                      // the gap: Qt expects an invalid QVariant
        KCHECK(answer == fx.model->data(fx.index(1)));                 // it is this model's row 1 ...
        KCHECK(answer != other.model->data(foreign));                  // ... and not the row the index came from
        if constexpr (!Fx::table) {                                    // a list model has no second column either
          const std::unique_ptr<QAbstractItemModel> table = fx.another_model(true);
          KCHECK(!fx.model->checkIndex(table->index(1, 1), QAbstractItemModel::CheckIndexOption::IndexIsValid));
          KCHECK(fx.model->data(table->index(1, 1)).isValid());
        }
      },
      false, kSmallFill);
}

// P-47
void limit_set_data_accepts_a_foreign_index() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::assignable) {
          Fx other(true);
          other.next = 10;
          other.fill(2);
          const QModelIndex foreign = other.index(1);
          const std::vector<int> theirs = other.shown();
          const int before = fx.shown_at(1);
          MessageCatcher caught;
          KCHECK(!fx.model->checkIndex(foreign, QAbstractItemModel::CheckIndexOption::IndexIsValid));
          KCHECK(fx.model->setData(foreign, kEdited));                 // the gap: Qt expects false
          KCHECK(fx.shown_at(1) == kEdited && before != kEdited);      // it edited this model's row 1
          KCHECK(other.shown() == theirs);                             // the model the index came from is untouched
          if constexpr (!Fx::table) {                                  // the read-only column of a table is no obstacle either
            const std::unique_ptr<QAbstractItemModel> table = fx.another_model(true);
            KCHECK(fx.model->setData(table->index(2, 1), kEdited + 1) && fx.shown_at(2) == kEdited + 1);
          }
        }
      },
      true, kSmallFill);
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  limit_edit_role_is_empty_for_an_editable_cell();
  limit_a_foreign_index_is_answered_with_this_models_own_data();
  limit_set_data_accepts_a_foreign_index();
  return rqt_test::finish("limit_edge_protocol_roles");
}
