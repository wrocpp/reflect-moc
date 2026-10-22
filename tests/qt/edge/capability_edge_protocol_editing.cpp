// Model protocol, part 6: editing through the model. setData on an invalid index, with another role, on a row that is
// gone, refused by the editor, twice with one value; what dataChanged carries; read-only columns and a QSet;
// setItemData and clearItemData as Qt defines them. A refused edit changes nothing and says nothing.
// QAbstractItemModelTester runs in fatal mode in every fixture. Catalogue rows: docs/qt-edge-cases/protocol.md
// (P-48 to P-57).
#include "protocol_support.hpp"

namespace {
using namespace edge;

// P-48
void set_data_on_an_invalid_index_is_refused() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        SignalLog log(fx.model.get());
        for (const int role : {static_cast<int>(Qt::EditRole), static_cast<int>(Qt::DisplayRole), static_cast<int>(Qt::UserRole), kUnknownRole})
          KCHECK(!fx.model->setData(QModelIndex(), kEdited, role));
        KCHECK(!fx.model->setData(fx.model->index(fx.rows(), 0), kEdited));  // the row after the last one is no index
        KCHECK(!fx.model->setData(fx.model->index(-1, 0), kEdited));
        KCHECK(log.silent() && fx.shown() == before);
      },
      true, kSmallFill);
}

// P-49
void set_data_with_another_role_is_refused() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        SignalLog log(fx.model.get());
        for (int row = 0; row < fx.rows(); ++row) {
          for (const int role : {static_cast<int>(Qt::DisplayRole), static_cast<int>(Qt::DecorationRole), static_cast<int>(Qt::ToolTipRole),
                                 static_cast<int>(Qt::StatusTipRole), static_cast<int>(Qt::CheckStateRole), static_cast<int>(Qt::UserRole),
                                 Qt::UserRole + 1, -1, kUnknownRole})
            KCHECK(!fx.model->setData(fx.index(row), kEdited, role));
        }
        KCHECK(log.silent() && fx.shown() == before);
      },
      true, kSmallFill);
}

// P-50: an index that was good before rows went away
void set_data_on_a_stale_index_past_the_last_row_is_refused() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const QModelIndex stale4 = fx.index(kSmallFill - 1);
        const QModelIndex stale3 = fx.index(kSmallFill - 2);
        KCHECK(fx.remove(2) && fx.remove(3));
        SignalLog log(fx.model.get());
        const std::vector<int> before = fx.shown();
        KCHECK(!fx.model->setData(stale4, kEdited));
        KCHECK(!fx.model->setData(stale3, kEdited));
        KCHECK(log.silent() && fx.shown() == before);
      },
      true, kSmallFill);
}

// P-51
void set_data_refused_by_the_editor_changes_nothing() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        SignalLog log(fx.model.get());
        for (const QVariant& refused : {QVariant(QStringLiteral("abc")), QVariant(QString()), QVariant(), QVariant(QStringLiteral("1.5")),
                                        QVariant(QStringLiteral("7x"))})
          KCHECK(!fx.model->setData(fx.index(1), refused));
        KCHECK(log.silent() && fx.shown() == before);
      },
      true, kSmallFill);
}

// P-52
void set_data_with_the_same_value_twice_announces_each_time() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::assignable) {
          SignalLog log(fx.model.get());
          KCHECK(fx.model->setData(fx.index(1), kEdited));
          KCHECK(fx.shown_at(1) == kEdited);
          KCHECK(log.count("dataChanged") == 1);
          KCHECK(fx.model->setData(fx.index(1), kEdited));  // equal to what is there: still accepted, still announced
          KCHECK(log.count("dataChanged") == 2);
          KCHECK(fx.shown_at(1) == kEdited);
          KCHECK(log.lines()[0] == log.lines()[1]);
        }
      },
      true, kSmallFill);
}

// P-53
void data_changed_covers_the_row_and_carries_the_roles() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::assignable) {
          const int last = fx.model->columnCount() - 1;
          SignalLog log(fx.model.get());
          const std::vector<int> rows = fx.shown();
          const int row_of_3 = static_cast<int>(std::find(rows.begin(), rows.end(), 3) - rows.begin());
          KCHECK(fx.model->setData(fx.index(row_of_3), kEdited));
          KCHECK(fx.replace(1, 11));
          KCHECK(fx.source.editValue(*fx.key_of(4), [](auto& v) { v = Fx::from_int(44); }));
          KCHECK(fx.source.editValue(*fx.key_of(5), [](auto& v) { v = Fx::from_int(55); }, QList<int>{Qt::UserRole + 1, Qt::UserRole + 2}));
          KCHECK(log.changes().size() == 4);
          for (const SignalLog::Change& change : log.changes()) {
            KCHECK(change.top.isValid() && change.bottom.isValid() && !change.top.parent().isValid() && !change.bottom.parent().isValid());
            KCHECK(change.top.row() == change.bottom.row() && change.top.column() == 0 && change.bottom.column() == last);
          }
          KCHECK(log.changes()[0].top.row() == row_of_3);
          KCHECK(log.changes()[0].roles.isEmpty() && log.changes()[1].roles.isEmpty() && log.changes()[2].roles.isEmpty());  // all roles
          KCHECK(log.changes()[3].roles == QList<int>({Qt::UserRole + 1, Qt::UserRole + 2}));
          for (const SignalLog::Change& change : log.changes())
            KCHECK(fx.model->data(change.top).toInt() == fx.shown_at(change.top.row()));
        }
      },
      true, kSmallFill);
}

// P-54: no editor on the model, or none on the column
void set_data_on_a_read_only_model_or_column_is_refused() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        SignalLog log(fx.model.get());
        for (int row = 0; row < fx.rows(); ++row)
          for (int column = 0; column < fx.model->columnCount(); ++column) {
            KCHECK(!fx.model->setData(fx.index(row, column), kEdited));
            KCHECK(!fx.model->flags(fx.index(row, column)).testFlag(Qt::ItemIsEditable));
          }
        KCHECK(log.silent() && fx.shown() == before);
      },
      false, kSmallFill);
  for_each_model(  // the second column of a table has no editor even when the first one has
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::table && Fx::assignable) {
          SignalLog log(fx.model.get());
          for (int row = 0; row < fx.rows(); ++row) KCHECK(!fx.model->setData(fx.index(row, 1), kEdited));
          KCHECK(log.silent());
          KCHECK(fx.shown_at(0, 1) == kTwinFactor * fx.shown_at(0, 0));
        }
      },
      true, kSmallFill);
}

// P-55: a set has no value apart from its key
void set_data_on_a_set_is_refused() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (!Fx::assignable) {
          const std::vector<int> before = fx.shown();
          SignalLog log(fx.model.get());
          KCHECK(!fx.model->setData(fx.index(1), kEdited));
          KCHECK(!fx.replace(1, kEdited));
          KCHECK(!fx.source.editValue(Fx::named(1), [](auto&) {}));
          KCHECK(log.silent() && fx.shown() == before);
        }
      },
      true, kSmallFill);
}

// P-56
void set_item_data_as_qt_defines_it() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::assignable) {
          SignalLog log(fx.model.get());
          KCHECK(fx.model->setItemData(fx.index(1), {{Qt::EditRole, kEdited}}));
          KCHECK(fx.shown_at(1) == kEdited && log.count("dataChanged") == 1);
          const std::vector<int> before = fx.shown();
          log.take();
          KCHECK(!fx.model->setItemData(fx.index(2), {{Qt::DisplayRole, 5}}));                       // display is not writable
          KCHECK(!fx.model->setItemData(fx.index(2), {{Qt::DisplayRole, 5}, {Qt::ToolTipRole, 6}}));
          KCHECK(!fx.model->setItemData(QModelIndex(), {{Qt::EditRole, kEdited}}));
          KCHECK(log.silent() && fx.shown() == before);
        }
      },
      true, kSmallFill);
  for_each_model(  // nothing is writable without an editor
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        KCHECK(!fx.model->setItemData(fx.index(1), {{Qt::EditRole, kEdited}}));
        KCHECK(fx.shown() == before);
      },
      false, kSmallFill);
}

// P-57
void clear_item_data_is_refused_by_the_editor() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        // clearItemData sets an empty QVariant for every role; the editor refuses it, so nothing is cleared
        const std::vector<int> before = fx.shown();
        SignalLog log(fx.model.get());
        for (int row = 0; row < fx.rows(); ++row) KCHECK(!fx.model->clearItemData(fx.index(row)));
        KCHECK(log.silent() && fx.shown() == before);
      },
      true, kSmallFill);
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        KCHECK(!fx.model->clearItemData(fx.index(0)));
        KCHECK(fx.shown() == before);
      },
      false, kSmallFill);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  set_data_on_an_invalid_index_is_refused();
  set_data_with_another_role_is_refused();
  set_data_on_a_stale_index_past_the_last_row_is_refused();
  set_data_refused_by_the_editor_changes_nothing();
  set_data_with_the_same_value_twice_announces_each_time();
  data_changed_covers_the_row_and_carries_the_roles();
  set_data_on_a_read_only_model_or_column_is_refused();
  set_data_on_a_set_is_refused();
  set_item_data_as_qt_defines_it();
  clear_item_data_is_refused_by_the_editor();
  return rqt_test::finish("capability_edge_protocol_editing");
}
