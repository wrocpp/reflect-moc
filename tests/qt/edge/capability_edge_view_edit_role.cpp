// The edit role: the model answers it with what an editable column displays, as Qt's own list model does, so a default
// delegate opens an editor that starts from the value, and a commit without typing writes the same value back.
// Before the fix the container model answered no edit role: the editor opened EMPTY and a commit handed "" to the
// column's editor function (pinned as limit_edge_view_edit_role).
// Catalogue: docs/qt-edge-cases/views-qml.md (V-12); protocol P-45 is the model-level row.
#include "view_support.hpp"

#include <QtCore/QStringListModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>

namespace {
using namespace edge_view;

void an_editor_starts_from_the_value() {
  using C = QMap<int, QString>;
  observable::ObservableContainer<C> source;
  observable::ContainerModel<C> model(
      &source, [](const int&, const QString& v) { return QVariant(v); },
      [](const QString&, const QVariant& edited) -> std::optional<QString> { return edited.toString(); });
  CHECK(source.insert(1, "alpha"));

  QStringListModel reference(QStringList{"alpha"});
  CHECK(reference.index(0).data(Qt::EditRole).toString() == "alpha");
  CHECK(model.index(0, 0).data(Qt::EditRole).toString() == "alpha");  // the same as Qt's list model
  CHECK(model.index(0, 0).data(Qt::DisplayRole).toString() == "alpha");

  QListView view;
  view.setModel(&model);
  view.resize(200, 100);
  view.show();
  settle();
  view.openPersistentEditor(model.index(0, 0));
  auto* editor = qobject_cast<QLineEdit*>(view.indexWidget(model.index(0, 0)));
  CHECK(editor);
  CHECK(editor->text() == "alpha");  // as over Qt's list model

  // Committing without typing writes the value back, not an empty text.
  emit view.itemDelegate()->commitData(editor);
  CHECK(source.value(1) == "alpha");
}
}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  an_editor_starts_from_the_value();
  return rqt_test::finish("capability_edge_view_edit_role");
}
