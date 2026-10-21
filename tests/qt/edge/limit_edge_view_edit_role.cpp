// Pinned: the model answers the display role and no edit role. Qt's convention (QStringListModel, QStandardItemModel)
// is that the edit role carries what an editor starts with, so a delegate opens an editor on `data(EditRole)`. Over
// the container model that is an invalid QVariant: a default editor opens EMPTY, and a commit without typing hands
// the editor "" to the column's editor function, which a function that accepts any text turns into an empty entry.
// A fix answers EditRole with the display value (or a column's own edit value); this file then becomes
// capability_edge_view_edit_role.
// Catalogue: docs/qt-edge-cases/views-qml.md.
#include "view_support.hpp"

#include <QtCore/QStringListModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>

namespace {
using namespace edge_view;

void an_editor_opens_empty() {
  using C = QMap<int, QString>;
  observable::ObservableContainer<C> source;
  observable::ContainerModel<C> model(
      &source, [](const int&, const QString& v) { return QVariant(v); },
      [](const QString&, const QVariant& edited) -> std::optional<QString> { return edited.toString(); });
  CHECK(source.insert(1, "alpha"));

  QStringListModel reference(QStringList{"alpha"});
  CHECK(reference.index(0).data(Qt::EditRole).toString() == "alpha");  // Qt's list model answers it
  CHECK(!model.index(0, 0).data(Qt::EditRole).isValid());               // the container model does not
  CHECK(model.index(0, 0).data(Qt::DisplayRole).toString() == "alpha");

  QListView view;
  view.setModel(&model);
  view.resize(200, 100);
  view.show();
  settle();
  view.openPersistentEditor(model.index(0, 0));
  auto* editor = qobject_cast<QLineEdit*>(view.indexWidget(model.index(0, 0)));
  CHECK(editor);
  CHECK(editor->text().isEmpty());  // over Qt's list model it reads "alpha"

  // Committing without typing writes the empty editor back.
  emit view.itemDelegate()->commitData(editor);
  CHECK(source.value(1).isEmpty());
}
}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  an_editor_opens_empty();
  return rqt_test::finish("limit_edge_view_edit_role");
}
