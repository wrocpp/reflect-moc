// Model protocol, part 5: data(), flags() and the roles. An invalid index, a stale index after a reset or a remove, an
// unknown role, the roles a model does not serve, flags of valid and invalid indexes, itemData and roleNames.
// QAbstractItemModelTester runs in fatal mode in every fixture. Catalogue rows: docs/qt-edge-cases/protocol.md
// (P-35 to P-44).
#include "protocol_support.hpp"

#include <climits>

namespace {
using namespace edge;

constexpr Qt::ItemDataRole kOtherRoles[] = {
    Qt::DecorationRole,  Qt::EditRole,         Qt::ToolTipRole,           Qt::StatusTipRole,         Qt::WhatsThisRole,
    Qt::FontRole,        Qt::TextAlignmentRole, Qt::BackgroundRole,        Qt::ForegroundRole,        Qt::CheckStateRole,
    Qt::AccessibleTextRole, Qt::AccessibleDescriptionRole, Qt::SizeHintRole, Qt::InitialSortOrderRole,
};

// P-35
void data_of_an_invalid_index_is_empty() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        for (const int role : {static_cast<int>(Qt::DisplayRole), static_cast<int>(Qt::EditRole), static_cast<int>(Qt::ToolTipRole),
                               static_cast<int>(Qt::UserRole), -1, kUnknownRole})
          KCHECK(!fx.model->data(QModelIndex(), role).isValid());
        KCHECK(!fx.model->data(QModelIndex()).isValid());
        KCHECK(!fx.model->data(fx.model->index(fx.rows(), 0)).isValid());  // index() says invalid, data() agrees
        KCHECK(fx.model->itemData(QModelIndex()).isEmpty());
      },
      false, kSmallFill);
}

// P-36
void data_of_a_stale_index_after_a_reset_or_a_clear() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const int last = fx.model->columnCount() - 1;
        const QModelIndex stale4 = fx.index(kSmallFill - 1, last);
        const QModelIndex stale0 = fx.index(0, last);
        fx.source.resetWith(fx.content({7, 8}));
        KCHECK(stale4.isValid());                                                  // a plain QModelIndex never learns
        KCHECK(!fx.model->data(stale4).isValid());                                 // its row is gone
        KCHECK(fx.model->data(stale0).isValid());                                  // its row is there again, with new content
        KCHECK(fx.model->data(stale0) == fx.model->data(fx.index(0, last)));
        fx.source.clear();
        KCHECK(!fx.model->data(stale0).isValid() && !fx.model->data(stale4).isValid());
        KCHECK(fx.model->itemData(stale0).isEmpty());
      },
      false, kSmallFill);
}

// P-37
void data_of_a_stale_index_after_a_remove() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const QModelIndex stale = fx.index(kSmallFill - 1);
        KCHECK(fx.remove(2));
        KCHECK(!fx.model->data(stale).isValid());                                   // the last row is past the end
        const QModelIndex shifted = fx.index(kSmallFill - 2);
        KCHECK(fx.remove(1));
        KCHECK(!fx.model->data(shifted).isValid());
        const QModelIndex still = fx.index(0);
        KCHECK(fx.model->data(still).toInt() == fx.shown_at(0));
      },
      false, kSmallFill);
}

// P-38
void data_of_an_unknown_role_is_empty() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        for (int row = 0; row < fx.rows(); ++row)
          for (int column = 0; column < fx.model->columnCount(); ++column)
            for (const int role : {-1, Qt::UserRole + 1, Qt::UserRole + kUnknownRole, kUnknownRole, INT_MAX, INT_MIN})
              KCHECK(!fx.model->data(fx.index(row, column), role).isValid());
      },
      false, kSmallFill);
}

// P-39
void key_role_is_served_where_a_key_is_stable() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        for (int row = 0; row < fx.rows(); ++row) {
          const QVariant key = fx.model->data(fx.index(row), Qt::UserRole);
          if constexpr (Fx::keyed) {
            KCHECK(key.isValid() && key.template value<typename Fx::Key>() == Fx::named(fx.shown_at(row)));
          } else {
            KCHECK(!key.isValid());  // a list row and a multi occurrence shift without a signal, so no key role
          }
          KCHECK(!fx.model->data(fx.index(row, fx.model->columnCount() - 1), Qt::UserRole + 1).isValid());
        }
      },
      false, kSmallFill);
}

// P-40
void roles_the_model_does_not_serve_are_empty() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        for (int row = 0; row < fx.rows(); ++row) {
          for (int column = 0; column < fx.model->columnCount(); ++column) {
            const QModelIndex index = fx.index(row, column);
            KCHECK(fx.model->data(index, Qt::DisplayRole).isValid() && fx.model->data(index).isValid());
            for (const Qt::ItemDataRole role : kOtherRoles) KCHECK(!fx.model->data(index, role).isValid());
          }
        }
      },
      false, kSmallFill);
}

// P-41
void flags_of_valid_and_invalid_indexes() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        for (int row = 0; row < fx.rows(); ++row) {
          for (int column = 0; column < fx.model->columnCount(); ++column) {
            const Qt::ItemFlags flags = fx.model->flags(fx.index(row, column));
            KCHECK(flags.testFlag(Qt::ItemIsEnabled) && flags.testFlag(Qt::ItemIsSelectable) && flags.testFlag(Qt::ItemNeverHasChildren));
            KCHECK(!flags.testFlag(Qt::ItemIsEditable) && !flags.testFlag(Qt::ItemIsUserCheckable) && !flags.testFlag(Qt::ItemIsUserTristate));
            KCHECK(!flags.testFlag(Qt::ItemIsDragEnabled) && !flags.testFlag(Qt::ItemIsDropEnabled));
          }
        }
        const Qt::ItemFlags none = fx.model->flags(QModelIndex());
        KCHECK(!none.testFlag(Qt::ItemIsEnabled) && !none.testFlag(Qt::ItemIsSelectable) && !none.testFlag(Qt::ItemIsEditable));
      },
      false, kSmallFill);
}

// P-42: an editor makes a column editable, and only that column
void flags_of_an_editable_column() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::assignable) {
          for (int row = 0; row < fx.rows(); ++row) {
            KCHECK(fx.model->flags(fx.index(row, 0)).testFlag(Qt::ItemIsEditable));
            if constexpr (Fx::table) KCHECK(!fx.model->flags(fx.index(row, 1)).testFlag(Qt::ItemIsEditable));  // no editor on it
          }
          KCHECK(!fx.model->flags(QModelIndex()).testFlag(Qt::ItemIsEditable));
        }
      },
      true, kSmallFill);
}

// P-43
void item_data_of_an_index() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        for (int row = 0; row < fx.rows(); ++row) {
          for (int column = 0; column < fx.model->columnCount(); ++column) {
            const QModelIndex index = fx.index(row, column);
            const QMap<int, QVariant> data = fx.model->itemData(index);
            KCHECK(data.size() == 1);  // Qt's default reads the roles below UserRole: the display, and not the key role
            KCHECK(data.value(Qt::DisplayRole) == fx.model->data(index, Qt::DisplayRole));
            KCHECK(!data.contains(Qt::UserRole) && !data.contains(Qt::EditRole));
          }
        }
      },
      false, kSmallFill);
}

// P-44
void role_names_are_stable_and_free_of_collisions() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        const QHash<int, QByteArray> names = fx.model->roleNames();
        KCHECK(names == fx.model->roleNames());  // QML caches it once, so a later call must agree
        QSet<QByteArray> distinct;
        for (const QByteArray& name : names) distinct.insert(name);
        KCHECK(distinct.size() == names.size());
        for (const int role : {static_cast<int>(Qt::DisplayRole), static_cast<int>(Qt::DecorationRole), static_cast<int>(Qt::EditRole),
                               static_cast<int>(Qt::ToolTipRole), static_cast<int>(Qt::StatusTipRole), static_cast<int>(Qt::WhatsThisRole)})
          KCHECK(names.contains(role));
        for (auto it = names.begin(); it != names.end(); ++it) {
          KCHECK(it.key() >= 0);
          if (it.key() >= Qt::UserRole) KCHECK(Fx::keyed && it.key() == Qt::UserRole && it.value() == "key");
        }
        KCHECK(names.contains(Qt::UserRole) == Fx::keyed);
        KCHECK(!names.contains(Qt::UserRole + 1));  // the first role of a subclass is free
        fx.fill(kSmallFill);
        fx.source.clear();
        KCHECK(fx.model->roleNames() == names);
      });
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  data_of_an_invalid_index_is_empty();
  data_of_a_stale_index_after_a_reset_or_a_clear();
  data_of_a_stale_index_after_a_remove();
  data_of_an_unknown_role_is_empty();
  key_role_is_served_where_a_key_is_stable();
  roles_the_model_does_not_serve_are_empty();
  flags_of_valid_and_invalid_indexes();
  flags_of_an_editable_column();
  item_data_of_an_index();
  role_names_are_stable_and_free_of_collisions();
  return rqt_test::finish("capability_edge_protocol_data");
}
