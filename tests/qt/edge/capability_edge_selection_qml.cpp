// The selection of a host in QML: a delegate reads the selected role as the property isSelected and writes it through model.isSelected,
// a binding to the host's hasSelection, selectedCount and selectedName follows the notifications, a change of the selection does
// not make or drop a delegate, and rows that come and go keep each entry's isSelected.
// Catalogue: docs/qt-edge-cases/matrix-selection.md (S-08, S-09).
#include "qml_support.hpp"
#include "selection_hosts.hpp"

#include <QtWidgets/QApplication>

#ifdef RQT_HAVE_QUICK
namespace {
using namespace edge_qml;
using namespace selection_test;

const QByteArray kQml = R"(
  import QtQuick
  Item {
    id: root
    width: 240; height: 200
    property int created: 0
    property int destroyed: 0
    property bool has: host.hasSelection
    property int selectedCountSeen: host.selectedCount === undefined ? -1 : host.selectedCount
    property string nameSeen: host.selectedName === undefined ? "" : host.selectedName
    function toggle(i) { list.itemAtIndex(i).toggle() }
    function set(i, v) { list.itemAtIndex(i).setSelected(v) }
    ListView {
      id: list
      objectName: "view"
      anchors.fill: parent
      model: host
      delegate: Text {
        id: item
        required property var model
        required property string name
        required property bool isSelected
        width: 100; height: 20
        text: name
        function toggle() { model.isSelected = !isSelected }
        function setSelected(v) { model.isSelected = v }
        Component.onCompleted: root.created++
        Component.onDestruction: root.destroyed++
      }
    }
  }
)";

ColorList colors(int n) {
  ColorList items;
  for (int i = 0; i < n; ++i) items.append(ColorItem{.name = QStringLiteral("color%1").arg(i), .hex = QStringLiteral("#%1").arg(i)});
  return items;
}

QStringList selected_in_view(Scene& scene) {
  QStringList out;
  QObject* list = scene.find("view");
  const int count = Scene::get(list, "count").toInt();
  for (int i = 0; i < count; ++i) {
    QQuickItem* item = nullptr;
    QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, i));
    if (item && item->property("isSelected").toBool()) out << item->property("name").toString();
  }
  return out;
}

void toggle(Scene& scene, int row) { QMetaObject::invokeMethod(scene.root.get(), "toggle", Q_ARG(QVariant, row)); }

// Multi: a delegate toggles its own row, every delegate shows its own isSelected, the bindings follow, no delegate is remade.
void a_multi_host_in_qml() {
  ColorMultiSelectionModel host;
  host.resetWith(colors(5));
  Scene scene;
  CHECK(scene.load(kQml, {{"host", &host}}));
  CHECK(host.roleNames().value(ColorMultiSelectionModel::SelectedRole) == "isSelected");
  const int created = scene.get("created").toInt();
  CHECK(created >= 5 && selected_in_view(scene).isEmpty() && !scene.get("has").toBool());

  toggle(scene, 1);
  toggle(scene, 3);
  settle();
  CHECK(host.selectedCount() == 2 && selected_in_view(scene) == QStringList({"color1", "color3"}));
  CHECK(scene.get("has").toBool() && scene.get("selectedCountSeen").toInt() == 2);
  CHECK(scene.get("created").toInt() == created && scene.get("destroyed").toInt() == 0);  // a selection makes no delegate
  toggle(scene, 1);
  settle();
  CHECK(selected_in_view(scene) == QStringList({"color3"}) && scene.get("selectedCountSeen").toInt() == 1);

  // Rows come and go: each entry keeps its isSelected.
  host.insertItem(0, ColorItem{"first", "#f"});
  host.insertItem(2, ColorItem{"mid", "#m"});
  settle();
  CHECK(selected_in_view(scene) == QStringList({"color3"}));
  host.removeItem(0);
  settle();
  CHECK(selected_in_view(scene) == QStringList({"color3"}) && scene.get("has").toBool());
  host.clearSelection();
  settle();
  CHECK(selected_in_view(scene).isEmpty() && !scene.get("has").toBool() && scene.get("selectedCountSeen").toInt() == 0);

  // A reset remakes the delegates and drops the selection.
  host.setData(host.index(2), true, ColorMultiSelectionModel::SelectedRole);
  settle();
  const int destroyedBefore = scene.get("destroyed").toInt();
  host.resetWith(colors(4));
  settle();
  CHECK(scene.get("destroyed").toInt() > destroyedBefore && selected_in_view(scene).isEmpty() && !scene.get("has").toBool());
}

// Single: selecting a row deselects the other in every delegate, the name binding follows, a removal of the selected row clears it.
void a_single_host_in_qml() {
  ColorSingleSelectionModel host;
  host.resetWith(colors(5));
  Scene scene;
  CHECK(scene.load(kQml, {{"host", &host}}));
  toggle(scene, 2);
  settle();
  CHECK(selected_in_view(scene) == QStringList({"color2"}) && scene.get("nameSeen").toString() == "color2" && scene.get("has").toBool());
  toggle(scene, 4);  // the other row is deselected, in the delegate too
  settle();
  CHECK(selected_in_view(scene) == QStringList({"color4"}) && scene.get("nameSeen").toString() == "color4");
  host.insertItem(0, ColorItem{"x", "#x"});  // the selected entry moves down a row; the name does not change
  settle();
  CHECK(selected_in_view(scene) == QStringList({"color4"}) && scene.get("nameSeen").toString() == "color4");
  host.removeItem(5);  // the selected entry goes
  settle();
  CHECK(selected_in_view(scene).isEmpty() && !scene.get("has").toBool() && scene.get("nameSeen").toString().isEmpty());
  QMetaObject::invokeMethod(scene.root.get(), "set", Q_ARG(QVariant, 1), Q_ARG(QVariant, true));
  settle();
  CHECK(selected_in_view(scene).size() == 1 && scene.get("has").toBool());
  host.clearSelection();
  settle();
  CHECK(selected_in_view(scene).isEmpty());
}
}  // namespace
#endif

int main(int argc, char** argv) {
  QApplication app(argc, argv);
#ifdef RQT_HAVE_QUICK
  a_multi_host_in_qml();
  a_single_host_in_qml();
#else
  std::printf("capability_edge_selection_qml: skipped, no Qt Quick\n");
#endif
  return rqt_test::finish("capability_edge_selection_qml");
}
