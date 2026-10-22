// Model protocol, fixed gaps P-46 and P-47: data() and setData() refuse an index of another model, and a column the
// model does not have, as Qt asks (QAbstractItemModel::checkIndex). checkIndex prints "is for model X which is different from this model" for the refusal; an
// invalid index is refused quietly and an index of the model itself raises no message. Catalogue rows:
// docs/qt-edge-cases/protocol.md (P-46, P-47).
#include "protocol_support.hpp"

namespace {
using namespace edge;

// P-46
void data_refuses_an_index_of_another_model() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        Fx other;
        other.next = 10;
        other.fill(2);
        const QModelIndex foreign = other.index(1);
        {
          MessageCatcher caught;
          KCHECK(fx.model->data(fx.index(1)).isValid() && !fx.model->data(QModelIndex()).isValid());
          KCHECK(fx.model->data(fx.index(1), Qt::UserRole).isValid() == Fx::keyed);
          KCHECK(caught.messages().isEmpty());  // an own index and an invalid one are quiet
        }
        MessageCatcher caught;
        KCHECK(!fx.model->data(foreign).isValid());
        KCHECK(!fx.model->data(foreign, Qt::UserRole).isValid());
        KCHECK(fx.model->itemData(foreign).isEmpty());
        KCHECK(caught.saw("different from this model"));
        KCHECK(other.model->data(foreign).isValid());  // its own model still answers
        if constexpr (!Fx::table) {                    // a list model has one column
          const std::unique_ptr<QAbstractItemModel> table = fx.another_model(true);
          KCHECK(!fx.model->data(table->index(1, 1)).isValid());
          KCHECK(!fx.model->data(table->index(1, 0)).isValid());  // the same container, but another model: refused too
        }
      },
      false, kSmallFill);
}

// P-47
void set_data_refuses_an_index_of_another_model() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        Fx other(true);
        other.next = 10;
        other.fill(2);
        const std::vector<int> mine = fx.shown();
        const std::vector<int> theirs = other.shown();
        SignalLog log(fx.model.get());
        MessageCatcher caught;
        KCHECK(!fx.model->setData(other.index(1), kEdited));
        KCHECK(caught.saw("different from this model"));
        KCHECK(fx.shown() == mine && other.shown() == theirs && log.silent());
        if constexpr (!Fx::table) {
          const std::unique_ptr<QAbstractItemModel> table = fx.another_model(true);
          KCHECK(!fx.model->setData(table->index(2, 1), kEdited));
          KCHECK(!fx.model->setData(table->index(2, 0), kEdited));
          KCHECK(fx.shown() == mine && log.silent());
        }
        if constexpr (Fx::assignable) KCHECK(fx.model->setData(fx.index(1), kEdited) && fx.shown_at(1) == kEdited);  // its own index works
      },
      true, kSmallFill);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  data_refuses_an_index_of_another_model();
  set_data_refuses_an_index_of_another_model();
  return rqt_test::finish("capability_edge_protocol_foreign_index");
}
