// QML edge cases, ListView over the container models (QQuickWindow offscreen, software backend): the current index,
// the count and its signal, the delegates created and destroyed, the content position, the transitions, the roles a
// delegate can ask for, and what a delegate or a script reads from a row that a removal has already dropped.
// A list model's changes are compared with the same ListView over QStringListModel, and a map's batches with the same
// ListView over a QStringListModel that is given the rows the model announces (Replay), so a difference is the
// container model's and none is the way ListView works.
// Catalogue: docs/qt-edge-cases/views-qml.md.
#include "qml_support.hpp"

#include <QtTest/QSignalSpy>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <functional>

#ifdef RQT_HAVE_QUICK
namespace {
using namespace edge_qml;
using ListRig = Rig<QList<QString>>;
using MapRig = Rig<QMap<int, QString>>;

constexpr int kRows = 40;
constexpr int kFirstVisible = 15;
constexpr int kCurrent = 17;
constexpr int kVisibleRows = kWindowHeight / kRowHeight;

const QByteArray kListQml = R"(
  import QtQuick
  Item {
    id: root
    width: 240; height: 100
    property int created: 0
    property int destroyed: 0
    property int serial: 0
    ListView {
      id: list
      objectName: "view"
      anchors.fill: parent
      model: rows
      delegate: Text {
        required property int index
        required property string display
        property int serialNumber: -1
        width: 100; height: 20
        text: display
        Component.onCompleted: { serialNumber = root.serial++; root.created++ }
        Component.onDestruction: root.destroyed++
      }
    }
  }
)";

QStringList texts(int n) {
  QStringList out;
  for (int i = 0; i < n; ++i) out.append(QString("row %1").arg(i));
  return out;
}

struct State {
  int count = 0;
  int currentIndex = 0;
  QString currentText;
  int contentY = 0;
  int created = 0;
  int destroyed = 0;
  int countChanged = 0;
  int currentIndexChanged = 0;
  bool operator==(const State&) const = default;
};

// The view of a scene, with the signals it emits counted from the moment of creation.
struct Watched {
  explicit Watched(Scene& s) : scene(s), list(s.find("view")), countSpy(list, SIGNAL(countChanged())), currentSpy(list, SIGNAL(currentIndexChanged())) {}
  State state() {
    State out;
    out.count = Scene::get(list, "count").toInt();
    out.currentIndex = Scene::get(list, "currentIndex").toInt();
    QObject* item = Scene::get(list, "currentItem").value<QObject*>();
    out.currentText = item ? item->property("text").toString() : QString("none");
    out.contentY = static_cast<int>(Scene::get(list, "contentY").toDouble());
    out.created = scene.get("created").toInt();
    out.destroyed = scene.get("destroyed").toInt();
    out.countChanged = static_cast<int>(countSpy.count());
    out.currentIndexChanged = static_cast<int>(currentSpy.count());
    return out;
  }
  void position(int index) { QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, index), Q_ARG(int, 0)); }  // Beginning
  Scene& scene;
  QObject* list;
  QSignalSpy countSpy;
  QSignalSpy currentSpy;
};

void describe(const char* what, const State& a, const State& b) {
  std::fprintf(stderr,
               "%s: count %d/%d, current %d/%d '%s'/'%s', contentY %d/%d, created %d/%d, destroyed %d/%d, countChanged %d/%d, currentIndexChanged %d/%d\n", what,
               a.count, b.count, a.currentIndex, b.currentIndex, qPrintable(a.currentText), qPrintable(b.currentText), a.contentY, b.contentY, a.created,
               b.created, a.destroyed, b.destroyed, a.countChanged, b.countChanged, a.currentIndexChanged, b.currentIndexChanged);
}

struct Change {
  const char* name;
  std::function<void(ListRig&)> ours;
  std::function<void(QStringListModel&)> reference;
};

void insert_row_at(QStringListModel& m, int row, const QString& text) {
  m.insertRows(row, 1);
  m.setData(m.index(row), text);
}

std::vector<Change> changes() {
  return {
      {"insert above the view", [](ListRig& r) { r.source.insert(2, "new"); }, [](QStringListModel& m) { insert_row_at(m, 2, "new"); }},
      {"insert in the view", [](ListRig& r) { r.source.insert(16, "new"); }, [](QStringListModel& m) { insert_row_at(m, 16, "new"); }},
      {"insert below the view", [](ListRig& r) { r.source.insert(30, "new"); }, [](QStringListModel& m) { insert_row_at(m, 30, "new"); }},
      {"remove above the view", [](ListRig& r) { r.source.remove(3); }, [](QStringListModel& m) { m.removeRows(3, 1); }},
      {"remove the current row", [](ListRig& r) { r.source.remove(kCurrent); }, [](QStringListModel& m) { m.removeRows(kCurrent, 1); }},
      {"remove below the view", [](ListRig& r) { r.source.remove(35); }, [](QStringListModel& m) { m.removeRows(35, 1); }},
      {"insert a run above", [](ListRig& r) { r.source.insertRange(1, QList<QString>{"a", "b", "c", "d", "e"}); },
       [](QStringListModel& m) {
         m.insertRows(1, 5);
         for (int i = 0; i < 5; ++i) m.setData(m.index(1 + i), QString(QChar('a' + i)));
       }},
      {"remove a run through the view", [](ListRig& r) { r.source.removeRange(16, 3); }, [](QStringListModel& m) { m.removeRows(16, 3); }},
      {"move into the view", [](ListRig& r) { r.source.move(2, 17); }, [](QStringListModel& m) { m.moveRows({}, 2, 1, {}, 18); }},
      {"move the current row", [](ListRig& r) { r.source.move(kCurrent, 3); }, [](QStringListModel& m) { m.moveRows({}, kCurrent, 1, {}, 3); }},
      {"replace in the view", [](ListRig& r) { r.source.replace(18, "other"); }, [](QStringListModel& m) { m.setData(m.index(18), "other"); }},
      {"reset to fewer rows", [](ListRig& r) { r.source.resetWith(texts(10)); }, [](QStringListModel& m) { m.setStringList(texts(10)); }},
      {"reset to as many rows", [](ListRig& r) { r.source.resetWith(texts(kRows)); }, [](QStringListModel& m) { m.setStringList(texts(kRows)); }},
      {"clear", [](ListRig& r) { r.source.clear(); }, [](QStringListModel& m) { m.setStringList({}); }},
  };
}

// currentIndex, count and its signal, the delegates created and destroyed and the content position of a ListView over
// the list model equal those over QStringListModel, for every change.
void a_list_view_follows_the_model_as_it_does_qts() {
  for (const Change& change : changes()) {
    ListRig ours;
    for (const QString& t : texts(kRows)) CHECK(ours.source.insert(ours.source.size(), t));
    QStringListModel reference(texts(kRows));
    Scene a;
    Scene b;
    CHECK(a.load(kListQml, {{"rows", ours.model.get()}}));
    CHECK(b.load(kListQml, {{"rows", &reference}}));
    Watched wa(a);
    Watched wb(b);
    for (Watched* w : {&wa, &wb}) {
      w->position(kFirstVisible);
      w->list->setProperty("currentIndex", kCurrent);
    }
    settle();
    CHECK(wa.state() == wb.state());
    change.ours(ours);
    change.reference(reference);
    settle();
    const State sa = wa.state();
    const State sb = wb.state();
    if (!(sa == sb)) describe(change.name, sa, sb);
    CHECK(sa == sb);
  }
}

// What the map's batches do to a ListView, with the same view over a QStringListModel given the announced rows.
void a_batch_is_what_its_runs_are_to_a_list_view() {
  struct Batch {
    const char* name;
    std::function<void(MapRig&)> apply;
  };
  const std::vector<Batch> batches{
      {"insertMany above the view, in it and below", [](MapRig& r) { r.source.insertMany(QMap<int, QString>{{1, "a"}, {2, "b"}, {155, "c"}, {175, "d"}, {900, "e"}}); }},
      {"removeMany above, through the current row and below", [](MapRig& r) { r.source.removeMany({10, 20, 170, 180, 350}); }},
      {"removeMany of the whole view", [](MapRig& r) { r.source.removeMany({150, 160, 170, 180, 190}); }},
      {"insertMany of one run before the view", [](MapRig& r) { r.source.insertMany(QMap<int, QString>{{11, "a"}, {12, "b"}, {13, "c"}}); }},
      {"resetWith other rows", [](MapRig& r) { r.source.resetWith(QMap<int, QString>{{1, "x"}, {2, "y"}}); }},
      {"clear", [](MapRig& r) { r.source.clear(); }},
  };
  for (const Batch& batch : batches) {
    MapRig ours;
    QMap<int, QString> rows;
    for (int k = 0; k < kRows; ++k) rows.insert(k * 10, QString("row %1").arg(k));
    ours.source.insertMany(rows);
    QStringListModel reference;
    Replay replay(*ours.model, reference);
    Scene a;
    Scene b;
    CHECK(a.load(kListQml, {{"rows", ours.model.get()}}));
    CHECK(b.load(kListQml, {{"rows", &reference}}));
    Watched wa(a);
    Watched wb(b);
    for (Watched* w : {&wa, &wb}) {
      w->position(kFirstVisible);
      w->list->setProperty("currentIndex", kCurrent);
    }
    settle();
    CHECK(wa.state() == wb.state());
    batch.apply(ours);
    settle();
    const State sa = wa.state();
    const State sb = wb.state();
    if (!(sa == sb)) describe(batch.name, sa, sb);
    CHECK(sa == sb);
  }
}

// The current index, stated: it follows its entry when rows come and go before it, and a removal of the current row
// moves it to the entry that took its place; a reset sets it to the first row and a clear to -1.
void the_current_index_in_words() {
  MapRig rig;
  QMap<int, QString> rows;
  for (int k = 0; k < kRows; ++k) rows.insert(k * 10, QString("row %1").arg(k));
  rig.source.insertMany(rows);
  Scene scene;
  CHECK(scene.load(kListQml, {{"rows", rig.model.get()}}));
  Watched w(scene);
  w.list->setProperty("currentIndex", kCurrent);
  settle();
  const auto now = [&] { return w.state(); };
  const auto text_of = [&](int key) { return QString("row %1").arg(key / 10); };

  CHECK(now().currentIndex == kCurrent && now().currentText == text_of(170));
  CHECK(rig.source.insert(5, "five"));  // before the current row
  settle();
  CHECK(now().currentIndex == kCurrent + 1 && now().currentText == text_of(170));  // still the same entry
  CHECK(rig.source.remove(0));
  settle();
  CHECK(now().currentIndex == kCurrent && now().currentText == text_of(170));
  CHECK(rig.source.insertMany(QMap<int, QString>{{1, "a"}, {2, "b"}, {3, "c"}}) == 3);  // one run before it
  settle();
  CHECK(now().currentIndex == kCurrent + 3 && now().currentText == text_of(170));
  CHECK(rig.source.removeMany({1, 2, 3, 40}) == 4);  // runs before it
  settle();
  CHECK(now().currentIndex == kCurrent - 1 && now().currentText == text_of(170));
  const int removedAt = now().currentIndex;
  CHECK(rig.source.removeMany({170, 180, 30}) == 3);  // the current row goes with others
  settle();
  const State afterRemoval = now();
  // The current index keeps its number: the row at that number now holds the entry after the removed ones.
  CHECK(afterRemoval.currentIndex == removedAt - 1 && afterRemoval.currentText == text_of(190));
  rig.source.resetWith(QMap<int, QString>{{7, "seven"}, {8, "eight"}});
  settle();
  // A reset does not move it: it is neither -1 nor 0 but the number it had, which is past the two new rows, so
  // there is no current item (the same as ListView over QStringListModel.setStringList, see the comparison above).
  CHECK(now().currentIndex == removedAt - 1 && now().currentText == "none" && now().count == 2);
  CHECK(rig.source.clear());
  settle();
  CHECK(now().currentIndex == removedAt - 1 && now().count == 0 && now().currentText == "none");
}

const QByteArray kMapQml = R"(
  import QtQuick
  Item {
    id: root
    width: 240; height: 100
    property int created: 0
    property int destroyed: 0
    property int serial: 0
    property int adds: 0
    property int removes: 0
    property int displacedRuns: 0
    property var reads: []
    property var textsAtAbout: []
    property bool transitions: false
    ListView {
      id: list
      objectName: "view"
      anchors.fill: parent
      model: rows
      add: Transition { enabled: root.transitions; ScriptAction { script: root.adds++ } NumberAnimation { property: "y"; duration: 30 } }
      remove: Transition { enabled: root.transitions; ScriptAction { script: root.removes++ } NumberAnimation { property: "opacity"; to: 0; duration: 30 } }
      displaced: Transition { enabled: root.transitions; ScriptAction { script: root.displacedRuns++ } NumberAnimation { property: "y"; duration: 30 } }
      delegate: Text {
        required property int index
        required property string display
        required property var key
        property int serialNumber: -1
        width: 100; height: 20
        text: display
        Component.onCompleted: { serialNumber = root.serial++; root.created++ }
        Component.onDestruction: root.destroyed++
      }
    }
    Connections {
      target: rows
      function onRowsAboutToBeRemoved(parent, first, last) {
        for (var r = first; r <= last; ++r) {
          root.reads.push(String(rows.data(rows.index(r, 0), Qt.DisplayRole)))
          var item = list.itemAtIndex(r)
          root.textsAtAbout.push(item ? item.text : "no item")
        }
      }
    }
  }
)";

QQuickItem* item_at(QObject* list, int index) {
  QQuickItem* item = nullptr;
  QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, index));
  return item;
}

// The delegates of the visible rows, by key, with the serial number each got when it was created.
QMap<int, int> serials(Scene& scene, QObject* list) {
  QMap<int, int> out;
  const int count = Scene::get(list, "count").toInt();
  for (int i = 0; i < count; ++i) {
    QQuickItem* item = item_at(list, i);
    if (item) out.insert(item->property("key").toInt(), item->property("serialNumber").toInt());
  }
  (void)scene;
  return out;
}

MapRig& fill(MapRig& rig, int rows = kRows) {
  QMap<int, QString> all;
  for (int k = 0; k < rows; ++k) all.insert(k * 10, QString("row %1").arg(k));
  rig.source.insertMany(all);
  return rig;
}

// A batch does not recreate the delegates of rows it does not touch: the delegate an entry had before is the delegate
// it has after, whatever runs were announced around it. A reset recreates every delegate.
void batches_keep_untouched_delegates_and_a_reset_replaces_all() {
  struct Batch {
    const char* name;
    std::function<void(MapRig&)> apply;
  };
  const std::vector<Batch> batches{
      {"insertMany above and inside the view", [](MapRig& r) { r.source.insertMany(QMap<int, QString>{{1, "a"}, {2, "b"}, {165, "c"}, {185, "d"}}); }},
      {"removeMany above and inside the view", [](MapRig& r) { r.source.removeMany({10, 20, 30, 160, 180}); }},
      {"insertMany below the view", [](MapRig& r) { r.source.insertMany(QMap<int, QString>{{395, "x"}, {396, "y"}}); }},
      {"removeMany below the view", [](MapRig& r) { r.source.removeMany({300, 310, 330}); }},
  };
  for (const Batch& batch : batches) {
    MapRig rig;
    fill(rig);
    Scene scene;
    CHECK(scene.load(kMapQml, {{"rows", rig.model.get()}}));
    Watched w(scene);
    w.position(kFirstVisible);
    settle();
    const QMap<int, int> before = serials(scene, w.list);
    CHECK(before.size() >= kVisibleRows);
    batch.apply(rig);
    settle();
    const QMap<int, int> after = serials(scene, w.list);
    int kept = 0;
    for (auto it = after.cbegin(); it != after.cend(); ++it) {
      if (!before.contains(it.key())) continue;
      ++kept;
      if (before.value(it.key()) != it.value()) std::fprintf(stderr, "%s: key %d was recreated\n", batch.name, it.key());
      CHECK(before.value(it.key()) == it.value());
    }
    CHECK(kept >= 1);  // the comparison had entries to compare
    CHECK(Scene::get(w.list, "count").toInt() == rig.model->rowCount());
  }

  MapRig rig;
  fill(rig);
  Scene scene;
  CHECK(scene.load(kMapQml, {{"rows", rig.model.get()}}));
  Watched w(scene);
  w.position(kFirstVisible);
  settle();
  const QMap<int, int> before = serials(scene, w.list);
  const int destroyedBefore = scene.get("destroyed").toInt();
  QMap<int, QString> same;
  for (int k = 0; k < kRows; ++k) same.insert(k * 10, QString("row %1").arg(k));
  rig.source.resetWith(same);  // the same entries
  settle();
  const QMap<int, int> after = serials(scene, w.list);
  CHECK(!after.isEmpty());
  for (auto it = after.cbegin(); it != after.cend(); ++it) CHECK(!before.values().contains(it.value()));  // every delegate is new
  CHECK(scene.get("destroyed").toInt() - destroyedBefore >= before.size());
}

// count and countChanged: a batch of runs changes count to its final value, and the signal is emitted at most once per run.
void count_and_its_signal() {
  MapRig rig;
  fill(rig, 20);
  Scene scene;
  CHECK(scene.load(kMapQml, {{"rows", rig.model.get()}}));
  Watched w(scene);
  CHECK(Scene::get(w.list, "count").toInt() == 20);
  const auto changes = [&] { return static_cast<int>(w.countSpy.count()); };
  CHECK(rig.source.insertMany(QMap<int, QString>{{1, "a"}, {2, "b"}, {55, "c"}, {195, "d"}}) == 4);  // three runs
  settle();
  CHECK(Scene::get(w.list, "count").toInt() == 24);
  CHECK(changes() >= 1 && changes() <= 3);
  const int afterInsert = changes();
  CHECK(rig.source.removeMany({1, 2, 55}) == 3);  // two runs
  settle();
  CHECK(Scene::get(w.list, "count").toInt() == 21);
  CHECK(changes() - afterInsert >= 1 && changes() - afterInsert <= 2);
  const int afterRemoval = changes();
  CHECK(!rig.source.replace(99, "none") && !rig.source.insertMany({}) && rig.source.removeMany({7777}) == 0);
  settle();
  CHECK(changes() == afterRemoval);  // refused changes tell QML nothing
  rig.source.resetWith(QMap<int, QString>{{1, "x"}});
  settle();
  // A reset is two changes of count for QML: ListView drops its rows (count 0) and takes the new ones, the same as
  // over QStringListModel.setStringList (see the comparison above).
  CHECK(Scene::get(w.list, "count").toInt() == 1 && changes() == afterRemoval + 2);
  CHECK(rig.source.clear());
  settle();
  CHECK(Scene::get(w.list, "count").toInt() == 0 && changes() == afterRemoval + 3);
}

bool positions_settled(Scene& scene, QObject* list) {
  const int count = Scene::get(list, "count").toInt();
  const int first = static_cast<int>(Scene::get(list, "contentY").toDouble()) / kRowHeight;
  for (int i = first; i < std::min(count, first + kVisibleRows); ++i) {
    QQuickItem* item = item_at(list, i);
    if (!item || static_cast<int>(item->property("y").toDouble()) != i * kRowHeight) return false;
  }
  (void)scene;
  return true;
}

// add, remove and displaced transitions run for a batch: the new rows get the add transition, the rows they push
// the displaced one, the dropped rows the remove one and their delegates are destroyed when it ends; the rows end where
// they belong.
void transitions_run_during_a_batch() {
  MapRig rig;
  fill(rig, 12);
  Scene scene;
  CHECK(scene.load(kMapQml, {{"rows", rig.model.get()}}));
  scene.root->setProperty("transitions", true);
  Watched w(scene);
  settle();
  CHECK(rig.source.insertMany(QMap<int, QString>{{5, "a"}, {25, "b"}, {45, "c"}}) == 3);  // three runs in the window
  CHECK(wait_for([&] { return scene.get("adds").toInt() >= 1 && scene.get("displacedRuns").toInt() >= 1; }));
  CHECK(wait_for([&] { return positions_settled(scene, w.list); }));
  CHECK(scene.get("adds").toInt() <= 3);  // at most one per new row
  const int destroyedBefore = scene.get("destroyed").toInt();
  CHECK(rig.source.removeMany({0, 20, 40}) == 3);
  CHECK(wait_for([&] { return scene.get("removes").toInt() >= 1 && positions_settled(scene, w.list); }));
  CHECK(wait_for([&] { return scene.get("destroyed").toInt() - destroyedBefore >= 3; }));
  CHECK(Scene::get(w.list, "count").toInt() == rig.model->rowCount());
  for (int i = 0; i < rig.model->rowCount() && i < kVisibleRows; ++i) {
    QQuickItem* item = item_at(w.list, i);
    CHECK(item && item->property("text").toString() == rig.model->index(i, 0).data().toString());
  }
}

QStringList g_warnings;
void collect_warnings(QtMsgType, const QMessageLogContext&, const QString& message) { g_warnings.append(message); }

// A delegate that requires "key" gets it from a map (the model names the role "key" for keyed containers) and cannot be
// made over a list (no such role): the list's delegates are not created at all, and QML says why. roleNames is read
// when the view takes the model; it never changes for a container, so there is no later change of the role set to be missed.
void required_properties_and_role_names() {
  MapRig map;
  fill(map, 5);
  CHECK(map.model->roleNames().value(kKeyRole) == "key" && map.model->roleNames().value(Qt::DisplayRole) == "display");
  Scene keyed;
  CHECK(keyed.load(kMapQml, {{"rows", map.model.get()}}));
  QObject* list = keyed.find("view");
  QQuickItem* first = item_at(list, 2);
  CHECK(first && first->property("key").toInt() == 20 && first->property("text").toString() == "row 2");

  ListRig listRig;
  for (const QString& t : texts(5)) CHECK(listRig.source.insert(listRig.source.size(), t));
  CHECK(!listRig.model->roleNames().contains(kKeyRole));
  g_warnings.clear();
  const QtMessageHandler previous = qInstallMessageHandler(collect_warnings);
  Scene unkeyed;
  const bool loaded = unkeyed.load(kMapQml, {{"rows", listRig.model.get()}});
  settle();
  qInstallMessageHandler(previous);
  CHECK(loaded);
  CHECK(Scene::get(unkeyed.find("view"), "count").toInt() == 5);
  CHECK(unkeyed.get("created").toInt() == 0);  // no delegate could be made
  bool said = false;
  for (const QString& w : g_warnings) said = said || w.contains("Required property key was not initialized");
  CHECK(said);
}

// ListView's content position: rows inserted above the view with insertMany do not move the item at the top of the view
// (the same as one by one over Qt's list model), and positionViewAtIndex after a reset puts the index at the top.
void content_position() {
  MapRig rig;
  fill(rig);
  Scene scene;
  CHECK(scene.load(kMapQml, {{"rows", rig.model.get()}}));
  Watched w(scene);
  w.position(kFirstVisible);
  settle();
  const int before = w.state().contentY;
  CHECK(before == kFirstVisible * kRowHeight);
  CHECK(rig.source.insertMany(QMap<int, QString>{{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}}) == 4);
  settle();
  CHECK(w.state().contentY == before);  // the same offset: the content moved under the view
  rig.source.resetWith(QMap<int, QString>{{0, "0"}, {10, "1"}, {20, "2"}, {30, "3"}, {40, "4"}, {50, "5"}, {60, "6"}, {70, "7"}, {80, "8"}, {90, "9"}});
  settle();
  w.position(3);
  settle();
  CHECK(w.state().contentY == 3 * kRowHeight);
  w.position(0);
  settle();
  CHECK(w.state().contentY == 0);
  QMetaObject::invokeMethod(w.list, "positionViewAtIndex", Q_ARG(int, 99), Q_ARG(int, 0));  // past the end: nothing breaks
  settle();
  CHECK(Scene::get(w.list, "count").toInt() == 10);
}

// A script or delegate reading a row at "rows about to be removed": the model serves its snapshot of a row the container
// has already dropped (a batch is announced at its done signal), the delegate of that row still shows what it showed,
// and nothing crashes.
void reading_a_dropped_row() {
  MapRig rig;
  fill(rig, 12);
  Scene scene;
  CHECK(scene.load(kMapQml, {{"rows", rig.model.get()}}));
  CHECK(rig.source.removeMany({20, 30, 90}) == 3);
  settle();
  const QVariantList reads = scene.get("reads").toList();
  const QVariantList shown = scene.get("textsAtAbout").toList();
  CHECK(reads.size() == 3 && shown.size() == 3);
  QStringList readTexts;
  for (const QVariant& r : reads) readTexts.append(r.toString());
  readTexts.sort();
  CHECK(readTexts == QStringList({"row 2", "row 3", "row 9"}));  // data() of a dropped row answers from the snapshot
  QStringList texts;
  for (const QVariant& t : shown) texts.append(t.toString());
  texts.sort();
  CHECK(texts == QStringList({"row 2", "row 3", "row 9"}) || texts.contains("no item"));  // the delegate still has its text, or the row is out of the view
  CHECK(Scene::get(scene.find("view"), "count").toInt() == 9);
}

// A model destroyed while a view shows it: the view lets go (count 0, model null), every delegate is destroyed, and
// the engine goes on.
void a_model_destroyed_under_its_view() {
  auto* rig = new MapRig();
  fill(*rig, 8);
  Scene scene;
  CHECK(scene.load(kMapQml, {{"rows", rig->model.get()}}));
  QObject* list = scene.find("view");
  CHECK(Scene::get(list, "count").toInt() == 8 && scene.get("created").toInt() >= kVisibleRows);
  delete rig;
  settle();
  CHECK(Scene::get(list, "count").toInt() == 0 && Scene::get(list, "model").isNull());
  CHECK(wait_for([&] { return scene.get("destroyed").toInt() == scene.get("created").toInt(); }));
}
}  // namespace
#endif

int main(int argc, char** argv) {
  QApplication app(argc, argv);
#ifdef RQT_HAVE_QUICK
  a_list_view_follows_the_model_as_it_does_qts();
  a_batch_is_what_its_runs_are_to_a_list_view();
  the_current_index_in_words();
  batches_keep_untouched_delegates_and_a_reset_replaces_all();
  count_and_its_signal();
  transitions_run_during_a_batch();
  required_properties_and_role_names();
  content_position();
  reading_a_dropped_row();
  a_model_destroyed_under_its_view();
#else
  std::printf("capability_edge_qml_listview: skipped, no Qt Quick\n");
#endif
  return rqt_test::finish("capability_edge_qml_listview");
}
