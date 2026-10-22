// Pinned: QML cannot bind to the model's row count. rowCount() is an invokable of QAbstractItemModel without a notify
// signal and the container model adds no `count` property, so `property int n: rows.rowCount()` is evaluated once and
// stays at the value it had, while a binding to the view's own count (ListView.count) follows every change. A script
// that wants the number must call rowCount() again or ask the view.
// A fix gives the models a `count` property with a countChanged signal; this file then becomes
// capability_edge_qml_rowcount_binding.
// Catalogue: docs/qt-edge-cases/views-qml.md.
#include "qml_support.hpp"

#include <QtWidgets/QApplication>

#ifdef RQT_HAVE_QUICK
namespace {
using namespace edge_qml;

void a_binding_to_rowCount_goes_stale() {
  Rig<QMap<int, QString>> rig;
  for (int k = 0; k < 3; ++k) CHECK(rig.source.insert(k, "x"));
  CHECK(!rig.model->metaObject()->property(rig.model->metaObject()->indexOfProperty("count")).isValid());  // no count property
  Scene scene;
  CHECK(scene.load(R"(
    import QtQuick
    Item {
      width: 100; height: 100
      property int viaRowCount: rows.rowCount()
      property int viaView: list.count
      function askAgain() { return rows.rowCount() }
      ListView { id: list; anchors.fill: parent; model: rows; delegate: Item { required property string display; height: 10 } }
    }
  )", {{"rows", rig.model.get()}}));
  CHECK(scene.get("viaRowCount").toInt() == 3 && scene.get("viaView").toInt() == 3);
  CHECK(rig.source.insertMany(QMap<int, QString>{{10, "a"}, {11, "b"}}) == 2);
  settle();
  CHECK(scene.get("viaView").toInt() == 5);       // the view's count is notified
  CHECK(scene.get("viaRowCount").toInt() == 3);   // the binding to rowCount() is not
  QVariant again;
  QMetaObject::invokeMethod(scene.root.get(), "askAgain", Q_RETURN_ARG(QVariant, again));
  CHECK(again.toInt() == 5);                      // calling it again gives the truth
}
}  // namespace
#endif

int main(int argc, char** argv) {
  QApplication app(argc, argv);
#ifdef RQT_HAVE_QUICK
  a_binding_to_rowCount_goes_stale();
#else
  std::printf("limit_edge_qml_rowcount_binding: skipped, no Qt Quick\n");
#endif
  return rqt_test::finish("limit_edge_qml_rowcount_binding");
}
