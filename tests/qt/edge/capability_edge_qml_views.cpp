// QML edge cases, the other views: Repeater and TableView over the container models, a TableView over a table
// model with no columns, and a Repeater over a model destroyed under it.
// Catalogue: docs/qt-edge-cases/views-qml.md.
#include "qml_support.hpp"

#include <QtWidgets/QApplication>

#include <algorithm>

#ifdef RQT_HAVE_QUICK
namespace {
using namespace edge_qml;
using MapRig = Rig<QMap<int, QString>>;
using MapTable = observable::ContainerTableModel<QMap<int, QString>>;
using Column = observable::Column<QMap<int, QString>>;

const QByteArray kRepeaterQml = R"(
  import QtQuick
  Item {
    id: root
    width: 240; height: 100
    property int created: 0
    property int destroyed: 0
    property int serial: 0
    Column {
      Repeater {
        id: rep
        objectName: "repeater"
        model: rows
        delegate: Text {
          required property string display
          required property var key
          property int serialNumber: -1
          text: display
          Component.onCompleted: { serialNumber = root.serial++; root.created++ }
          Component.onDestruction: root.destroyed++
        }
      }
    }
  }
)";

QQuickItem* repeater_item(QObject* repeater, int index) {
  QQuickItem* item = nullptr;
  QMetaObject::invokeMethod(repeater, "itemAt", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, index));
  return item;
}

MapRig& fill(MapRig& rig, int rows) {
  QMap<int, QString> all;
  for (int k = 0; k < rows; ++k) all.insert(k * 10, QString("row %1").arg(k));
  rig.source.insertMany(all);
  return rig;
}

QStringList texts_of(QObject* repeater) {
  QStringList out;
  const int count = Scene::get(repeater, "count").toInt();
  for (int i = 0; i < count; ++i) {
    QQuickItem* item = repeater_item(repeater, i);
    out.append(item ? item->property("text").toString() : QString("missing"));
  }
  return out;
}

// A Repeater has one item per row, in the model's order, through batches and a reset; the items of rows a batch does not
// touch are the items they were, and a reset makes all of them again.
void a_repeater_follows_batches() {
  MapRig rig;
  fill(rig, 8);
  Scene scene;
  CHECK(scene.load(kRepeaterQml, {{"rows", rig.model.get()}}));
  QObject* rep = scene.find("repeater");
  CHECK(texts_of(rep) == column(*rig.model) && Scene::get(rep, "count").toInt() == 8);
  std::map<int, int> before;
  for (int i = 0; i < 8; ++i) before[repeater_item(rep, i)->property("key").toInt()] = repeater_item(rep, i)->property("serialNumber").toInt();

  CHECK(rig.source.insertMany(QMap<int, QString>{{5, "five"}, {25, "twenty-five"}, {95, "ninety-five"}}) == 3);
  settle();
  CHECK(texts_of(rep) == column(*rig.model) && Scene::get(rep, "count").toInt() == 11);
  CHECK(rig.source.removeMany({0, 30, 31, 70}) == 3);
  settle();
  CHECK(texts_of(rep) == column(*rig.model));
  for (int i = 0; i < Scene::get(rep, "count").toInt(); ++i) {
    QQuickItem* item = repeater_item(rep, i);
    const auto it = before.find(item->property("key").toInt());
    if (it != before.end()) CHECK(item->property("serialNumber").toInt() == it->second);  // an old row keeps its item
  }
  CHECK(rig.source.replace(10, "changed"));
  settle();
  CHECK(texts_of(rep) == column(*rig.model));

  const int destroyedBefore = scene.get("destroyed").toInt();
  const int countBefore = Scene::get(rep, "count").toInt();
  QMap<int, QString> same;
  for (const int key : rig.source.items().keys()) same.insert(key, rig.source.items().value(key));
  rig.source.resetWith(same);
  settle();
  CHECK(scene.get("destroyed").toInt() - destroyedBefore == countBefore);  // every item of the old rows is gone
  CHECK(texts_of(rep) == column(*rig.model));
  for (int i = 0; i < Scene::get(rep, "count").toInt(); ++i) {
    const auto it = before.find(repeater_item(rep, i)->property("key").toInt());
    if (it != before.end()) CHECK(repeater_item(rep, i)->property("serialNumber").toInt() != it->second);
  }
  CHECK(rig.source.clear());
  settle();
  CHECK(Scene::get(rep, "count").toInt() == 0);
}

const QByteArray kTableQml = R"(
  import QtQuick
  Item {
    id: root
    width: 240; height: 100
    property int created: 0
    property int destroyed: 0
    TableView {
      id: table
      objectName: "table"
      anchors.fill: parent
      model: grid
      delegate: Rectangle {
        required property string display
        implicitWidth: 60; implicitHeight: 20
        property string shown: display
        Component.onCompleted: root.created++
        Component.onDestruction: root.destroyed++
      }
    }
  }
)";

constexpr int kRowsInView = 4;

QString cell_text(QObject* table, int row, int column) {
  QQuickItem* item = nullptr;
  QMetaObject::invokeMethod(table, "itemAtCell", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(QPoint, QPoint(column, row)));
  return item ? item->property("shown").toString() : QString("missing");
}

// A TableView over a table model shows rows by columns: rows and columns are the model's, every cell shows the model's
// text (the first row and column are made first; the view is large enough for the few rows here), through a batch
// of runs and a reset.
void a_table_view_over_a_table_model() {
  observable::ObservableContainer<QMap<int, QString>> source;
  const auto first = [](const int&, const QString& v) { return QVariant(v); };
  const auto second = [](const int& k, const QString&) { return QVariant(QString("key %1").arg(k)); };
  MapTable model(&source, std::vector<Column>{{"first", first}, {"second", second}});
  QMap<int, QString> rows;
  for (int k = 0; k < 4; ++k) rows.insert(k * 10, QString("row %1").arg(k));
  source.insertMany(rows);
  Scene scene;
  CHECK(scene.load(kTableQml, {{"grid", &model}}));
  QObject* table = scene.find("table");
  CHECK(Scene::get(table, "rows").toInt() == 4 && Scene::get(table, "columns").toInt() == 2);
  const auto cells_match = [&] {
    for (int r = 0; r < std::min(model.rowCount(), kRowsInView); ++r)  // a TableView makes the cells it shows
      for (int c = 0; c < 2; ++c) {
        if (cell_text(table, r, c) != model.index(r, c).data().toString()) {
          std::fprintf(stderr, "cell %d,%d: '%s' vs '%s'\n", r, c, qPrintable(cell_text(table, r, c)), qPrintable(model.index(r, c).data().toString()));
          return false;
        }
      }
    return true;
  };
  CHECK(cells_match());
  CHECK(source.insertMany(QMap<int, QString>{{5, "five"}, {15, "fifteen"}, {35, "thirty-five"}}) == 3);
  settle();
  CHECK(Scene::get(table, "rows").toInt() == 7 && Scene::get(table, "columns").toInt() == 2);
  CHECK(cells_match());
  CHECK(source.removeMany({0, 15, 20}) == 3);
  settle();
  CHECK(Scene::get(table, "rows").toInt() == 4);
  CHECK(cells_match());
  CHECK(source.replace(10, "changed"));
  settle();
  CHECK(cells_match());
  source.resetWith(QMap<int, QString>{{1, "one"}, {2, "two"}});
  settle();
  CHECK(Scene::get(table, "rows").toInt() == 2 && cells_match());
  CHECK(source.clear());
  settle();
  CHECK(Scene::get(table, "rows").toInt() == 0);
}

// A table model with no columns: the view has rows and no cells, makes no delegate, and the model's data answers
// nothing; adding rows to the container changes nothing it can show.
void a_table_view_over_a_model_with_no_columns() {
  observable::ObservableContainer<QMap<int, QString>> source;
  MapTable model(&source, std::vector<Column>{});
  QAbstractItemModelTester tester(&model, kFatal);
  source.insert(1, "one");
  source.insert(2, "two");
  CHECK(model.columnCount() == 0 && model.rowCount() == 2 && !model.index(0, 0).isValid());
  Scene scene;
  CHECK(scene.load(kTableQml, {{"grid", &model}}));
  QObject* table = scene.find("table");
  CHECK(Scene::get(table, "columns").toInt() == 0);
  CHECK(scene.get("created").toInt() == 0);
  CHECK(source.insertMany(QMap<int, QString>{{5, "five"}, {6, "six"}}) == 2);
  settle();
  CHECK(Scene::get(table, "columns").toInt() == 0 && scene.get("created").toInt() == 0);
  source.resetWith({});
  CHECK(source.insert(3, "three"));
  settle();
  CHECK(scene.get("created").toInt() == 0);
}

// A Repeater over a model destroyed under it lets go of its items.
void a_repeater_over_a_destroyed_model() {
  auto* rig = new MapRig();
  fill(*rig, 5);
  Scene scene;
  CHECK(scene.load(kRepeaterQml, {{"rows", rig->model.get()}}));
  QObject* rep = scene.find("repeater");
  CHECK(Scene::get(rep, "count").toInt() == 5);
  delete rig;
  settle();
  CHECK(Scene::get(rep, "count").toInt() == 0);
  CHECK(wait_for([&] { return scene.get("destroyed").toInt() == scene.get("created").toInt(); }));
}
}  // namespace
#endif

int main(int argc, char** argv) {
  QApplication app(argc, argv);
#ifdef RQT_HAVE_QUICK
  a_repeater_follows_batches();
  a_table_view_over_a_table_model();
  a_table_view_over_a_model_with_no_columns();
  a_repeater_over_a_destroyed_model();
#else
  std::printf("capability_edge_qml_views: skipped, no Qt Quick\n");
#endif
  return rqt_test::finish("capability_edge_qml_views");
}
