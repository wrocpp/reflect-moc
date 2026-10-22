// View edge cases, widget views offscreen: QListView, QTableView and QTreeView over the container models. The
// state a view keeps (current index, selection, scroll position, geometry) is compared with the same view over Qt's
// own QStringListModel under the same change, so any difference is the model's and none is Qt's way of working.
// Catalogue: docs/qt-edge-cases/views-qml.md.
#include "view_support.hpp"

#include <QtCore/QStringListModel>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QStandardItemModel>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>

#include <algorithm>

namespace {
using namespace edge_view;
using ListRig = Rig<QList<QString>>;
using MapRig = Rig<QMap<int, QString>>;

constexpr int kRows = 60;
constexpr int kViewWidth = 200;
constexpr int kViewHeight = 120;
constexpr int kScrolledTo = 15;   // items, the view scrolls per item here
constexpr int kCurrentRow = 18;
constexpr int kSelectedFrom = 17;
constexpr int kSelectedTo = 20;

QStringList texts(int n) {
  QStringList out;
  for (int i = 0; i < n; ++i) out.append(QString("row %1").arg(i));
  return out;
}

struct ViewState {
  QString current;
  QStringList selected;
  int scroll = 0;
  int scrollMax = 0;
  QString firstVisible;
  int rows = 0;
  bool operator==(const ViewState&) const = default;
};

ViewState state_of(QListView& view) {
  ViewState s;
  s.current = view.currentIndex().isValid() ? QString::number(view.currentIndex().row()) : QString("none");
  for (const QModelIndex& i : view.selectionModel()->selectedRows()) s.selected.append(QString::number(i.row()));
  s.selected.sort();
  s.scroll = view.verticalScrollBar()->value();
  s.scrollMax = view.verticalScrollBar()->maximum();
  s.firstVisible = view.indexAt(QPoint(2, 2)).data().toString();
  s.rows = view.model()->rowCount();
  return s;
}

void show_and_scroll(QListView& view, QAbstractItemModel* model) {
  view.setModel(model);
  view.resize(kViewWidth, kViewHeight);
  view.show();
  settle();
  view.verticalScrollBar()->setValue(kScrolledTo);
  view.setCurrentIndex(model->index(kCurrentRow, 0));
  view.selectionModel()->select(QItemSelection(model->index(kSelectedFrom, 0), model->index(kSelectedTo, 0)), QItemSelectionModel::Select);
  settle();
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
      {"insert in the selection", [](ListRig& r) { r.source.insert(19, "new"); }, [](QStringListModel& m) { insert_row_at(m, 19, "new"); }},
      {"insert below the view", [](ListRig& r) { r.source.insert(50, "new"); }, [](QStringListModel& m) { insert_row_at(m, 50, "new"); }},
      {"remove above the view", [](ListRig& r) { r.source.remove(3); }, [](QStringListModel& m) { m.removeRows(3, 1); }},
      {"remove the current row", [](ListRig& r) { r.source.remove(kCurrentRow); }, [](QStringListModel& m) { m.removeRows(kCurrentRow, 1); }},
      {"remove below the view", [](ListRig& r) { r.source.remove(55); }, [](QStringListModel& m) { m.removeRows(55, 1); }},
      {"insert a run above", [](ListRig& r) { r.source.insertRange(1, QList<QString>{"a", "b", "c", "d", "e"}); },
       [](QStringListModel& m) {
         m.insertRows(1, 5);
         for (int i = 0; i < 5; ++i) m.setData(m.index(1 + i), QString(QChar('a' + i)));
       }},
      {"remove a run through the selection", [](ListRig& r) { r.source.removeRange(16, 5); }, [](QStringListModel& m) { m.removeRows(16, 5); }},
      {"move into the view", [](ListRig& r) { r.source.move(2, 17); }, [](QStringListModel& m) { m.moveRows({}, 2, 1, {}, 18); }},
      {"replace in the view", [](ListRig& r) { r.source.replace(19, "other"); }, [](QStringListModel& m) { m.setData(m.index(19), "other"); }},
      {"reset to fewer rows", [](ListRig& r) { r.source.resetWith(texts(10)); }, [](QStringListModel& m) { m.setStringList(texts(10)); }},
      {"reset to as many rows", [](ListRig& r) { r.source.resetWith(texts(kRows)); }, [](QStringListModel& m) { m.setStringList(texts(kRows)); }},
      {"clear", [](ListRig& r) { r.source.clear(); }, [](QStringListModel& m) { m.setStringList({}); }},
  };
}

// A QListView over the list model keeps the current index, the selection and the scroll position exactly as it does
// over QStringListModel, for every change.
void a_list_view_keeps_what_it_keeps_over_qts_model() {
  for (const Change& change : changes()) {
    ListRig ours;
    for (const QString& t : texts(kRows)) CHECK(ours.source.insert(ours.source.size(), t));
    QStringListModel reference(texts(kRows));
    QListView oursView;
    QListView referenceView;
    show_and_scroll(oursView, ours.model.get());
    show_and_scroll(referenceView, &reference);
    CHECK(state_of(oursView) == state_of(referenceView));
    change.ours(ours);
    change.reference(reference);
    settle();
    const ViewState a = state_of(oursView);
    const ViewState b = state_of(referenceView);
    if (!(a == b)) std::fprintf(stderr, "list view, %s: scroll %d/%d vs %d/%d, current %s vs %s, first %s vs %s\n", change.name, a.scroll, a.scrollMax, b.scroll,
                                b.scrollMax, qPrintable(a.current), qPrintable(b.current), qPrintable(a.firstVisible), qPrintable(b.firstVisible));
    CHECK(a == b);
  }
}

// Where every row of a view is, in the view's viewport.
QList<QRect> geometry(QAbstractItemView& view) {
  QList<QRect> out;
  for (int row = 0; row < view.model()->rowCount(); ++row) out.append(view.visualRect(view.model()->index(row, 0)));
  return out;
}

void lay_out(QAbstractItemView& view) {
  view.resize(kViewWidth, kViewHeight);
  view.show();
  view.doItemsLayout();
  settle();
}

// indexAt gives back the row whose visualRect it is asked about, for every row in the viewport.
void indexAt_matches_visualRect(QAbstractItemView& view) {
  for (int row = 0; row < view.model()->rowCount(); ++row) {
    const QModelIndex index = view.model()->index(row, 0);
    const QRect rect = view.visualRect(index);
    if (rect.isValid() && view.viewport()->rect().contains(rect.center())) CHECK(view.indexAt(rect.center()) == index);
  }
}

// A view made before the rows exist and one made after them show the rows in the same places, as the views of Qt's
// list model do; this holds for a list view, a table view and a tree view.
template <class View>
void views_before_and_after_the_rows() {
  MapRig rig;
  View before;
  before.setModel(rig.model.get());
  lay_out(before);
  CHECK(before.model()->rowCount() == 0);
  QMap<int, QString> rows;
  for (int k = 0; k < 40; ++k) rows.insert(k * 10, QString("row %1").arg(k));
  CHECK(rig.source.insertMany(rows) == 40);
  View after;
  after.setModel(rig.model.get());
  lay_out(after);
  before.doItemsLayout();
  settle();
  CHECK(geometry(before) == geometry(after));
  indexAt_matches_visualRect(before);
  indexAt_matches_visualRect(after);
  QStringListModel reference(texts(40));
  View referenceView;
  referenceView.setModel(&reference);
  lay_out(referenceView);
  CHECK(geometry(after) == geometry(referenceView));
  // After a batch the geometry is that of the final rows, not of the rows before.
  CHECK(rig.source.insertMany(QMap<int, QString>{{5, "a"}, {15, "b"}, {25, "c"}, {395, "d"}}) == 4);
  CHECK(rig.source.removeMany({0, 100, 200}) == 3);
  before.doItemsLayout();
  after.doItemsLayout();
  settle();
  CHECK(geometry(before) == geometry(after));
  indexAt_matches_visualRect(after);
}

void views_attached_before_and_after() {
  views_before_and_after_the_rows<QListView>();
  views_before_and_after_the_rows<QTableView>();
  views_before_and_after_the_rows<QTreeView>();
}

// A batch keeps the entries that are current and selected, and the view's scroll position stays at the same item
// index, so rows inserted above push the content down: the first visible entry changes, as it does for Qt's model. A
// reset drops the current index and the selection and keeps the scroll position (clamped to the new range); a clear
// drops all of it.
void a_batch_keeps_a_reset_drops() {
  MapRig rig;
  QMap<int, QString> rows;
  for (int k = 0; k < kRows; ++k) rows.insert(k * 10, QString::number(k * 10));
  rig.source.insertMany(rows);
  QListView view;
  view.setModel(rig.model.get());
  view.resize(kViewWidth, kViewHeight);
  view.show();
  settle();
  view.verticalScrollBar()->setValue(kScrolledTo);
  view.setCurrentIndex(rig.model->index(kCurrentRow, 0));  // key 180
  view.selectionModel()->select(QItemSelection(rig.model->index(kSelectedFrom, 0), rig.model->index(kSelectedTo, 0)), QItemSelectionModel::Select);
  settle();
  const auto keys = [&] {
    QList<int> out;
    for (const QModelIndex& i : view.selectionModel()->selectedRows()) out.append(i.data(kKeyRole).toInt());
    std::sort(out.begin(), out.end());
    return out;
  };
  const QList<int> selected{170, 180, 190, 200};
  CHECK(keys() == selected && view.currentIndex().data(kKeyRole).toInt() == 180);

  CHECK(rig.source.insertMany(QMap<int, QString>{{1, "1"}, {2, "2"}, {3, "3"}, {4, "4"}, {5, "5"}, {185, "185"}, {900, "900"}}) == 7);
  settle();
  CHECK(keys() == selected);                                   // the new 185 inside the range is not selected
  CHECK(view.currentIndex().data(kKeyRole).toInt() == 180);
  CHECK(view.verticalScrollBar()->value() == kScrolledTo);   // the same item index: the content moved under it
  CHECK(view.indexAt(QPoint(2, 2)).row() == kScrolledTo);

  CHECK(rig.source.removeMany({0, 10, 20, 195, 800}) == 3);
  settle();
  CHECK(keys() == QList<int>({170, 180, 190, 200}));
  CHECK(view.currentIndex().data(kKeyRole).toInt() == 180);
  CHECK(view.verticalScrollBar()->value() == kScrolledTo);

  CHECK(rig.source.removeMany({180, 190}) == 2);  // the current row and a selected row go
  settle();
  // The view is in SingleSelection mode: when the current row goes, Qt moves the current index to the entry that
  // took its place (185, the new neighbour) and selects it, clearing the rest. The same happens over QStringListModel
  // when the same row removals are replayed (checked while writing this test).
  CHECK(view.currentIndex().isValid() && view.currentIndex().data(kKeyRole).toInt() == 185);
  CHECK(keys() == QList<int>({185}));

  rig.source.resetWith(rows);
  settle();
  CHECK(!view.currentIndex().isValid() && view.selectionModel()->selectedRows().isEmpty());
  CHECK(view.verticalScrollBar()->value() == kScrolledTo && rig.model->rowCount() == kRows);
  view.setCurrentIndex(rig.model->index(3, 0));
  CHECK(rig.source.clear());
  settle();
  CHECK(!view.currentIndex().isValid() && view.verticalScrollBar()->value() == 0 && view.verticalScrollBar()->maximum() == 0);
}

// A view that reads the model at "rows about to be removed" gets nothing for a row the container has dropped already (a
// batch is announced at its "done" signal), and the geometry calls it makes there do not fail.
void a_view_reading_at_the_about_signal() {
  MapRig rig;
  for (int k = 0; k < 10; ++k) CHECK(rig.source.insert(k, QString("row %1").arg(k)));
  QListView view;
  view.setModel(rig.model.get());
  lay_out(view);
  QStringList readAtAbout;
  int visualRects = 0;
  QObject::connect(rig.model.get(), &QAbstractItemModel::rowsAboutToBeRemoved, rig.model.get(), [&](const QModelIndex&, int first, int last) {
    for (int row = first; row <= last; ++row) {
      readAtAbout.append(view.model()->index(row, 0).data().toString());
      visualRects += view.visualRect(view.model()->index(row, 0)).isValid() ? 1 : 0;
    }
  });
  CHECK(rig.source.removeMany({2, 3, 7}) == 3);
  CHECK(readAtAbout == QStringList({"", "", ""}));  // the container no longer holds them
  settle();
  CHECK(rig.model->rowCount() == 7);
  // Qt's model keeps the data until the rows are gone; that is the one difference, and the contract of the container
  // model (the container already holds the final content when a batch is announced).
  QStringListModel reference(texts(10));
  QStringList readByReference;
  QObject::connect(&reference, &QAbstractItemModel::rowsAboutToBeRemoved, &reference, [&](const QModelIndex&, int first, int last) {
    for (int row = first; row <= last; ++row) readByReference.append(reference.index(row, 0).data().toString());
  });
  reference.removeRows(7, 1);
  CHECK(readByReference == QStringList({"row 7"}));
}

struct EditingListView : QListView {
  bool editing() const { return state() == EditingState; }
};

// A persistent editor is a widget on a row: it stays on its entry through a batch elsewhere, and goes with its row
// (a removal, a reset or a clear). An editor in progress is closed with its row. Same as over QStringListModel.
void editors_follow_their_rows() {
  MapRig rig(true);
  for (int k = 0; k < 10; ++k) CHECK(rig.source.insert(k * 10, QString("v%1").arg(k * 10)));
  EditingListView view;
  view.setModel(rig.model.get());
  lay_out(view);
  QList<QPointer<QWidget>> editors;
  for (int row = 0; row < 10; ++row) {
    view.openPersistentEditor(rig.model->index(row, 0));
    editors.append(view.indexWidget(rig.model->index(row, 0)));
    CHECK(editors.last());
  }
  const auto editor_of_key = [&](int key) -> QWidget* {
    for (int row = 0; row < rig.model->rowCount(); ++row)
      if (rig.model->index(row, 0).data(kKeyRole).toInt() == key) return view.indexWidget(rig.model->index(row, 0));
    return nullptr;
  };
  const auto flush = [] {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    settle();
  };
  CHECK(rig.source.insertMany(QMap<int, QString>{{5, "v5"}, {55, "v55"}, {105, "v105"}}) == 3);
  flush();
  for (int k = 0; k < 10; ++k) CHECK(editor_of_key(k * 10) == editors[static_cast<qsizetype>(k)].data());  // each editor is on its entry
  CHECK(rig.source.removeMany({20, 40, 60, 5}) == 4);
  flush();
  for (int k = 0; k < 10; ++k) {
    const bool removed = k == 2 || k == 4 || k == 6;
    CHECK(editors[static_cast<qsizetype>(k)].isNull() == removed);
    CHECK(removed ? editor_of_key(k * 10) == nullptr : editor_of_key(k * 10) == editors[static_cast<qsizetype>(k)].data());
  }
  rig.source.resetWith(QMap<int, QString>{{0, "v0"}, {10, "v10"}});
  flush();
  for (const QPointer<QWidget>& e : editors) CHECK(e.isNull());
  CHECK(view.indexWidget(rig.model->index(0, 0)) == nullptr);

  // An editor in progress.
  view.edit(rig.model->index(1, 0));
  QPointer<QWidget> inProgress = view.indexWidget(rig.model->index(1, 0));
  CHECK(inProgress && view.editing());
  CHECK(rig.source.remove(0));  // the row above it: the editor stays, on its entry
  flush();
  CHECK(inProgress && view.indexWidget(rig.model->index(0, 0)) == inProgress.data());
  CHECK(rig.source.removeMany({10}) == 1);
  flush();
  CHECK(inProgress.isNull() && !view.editing());
  CHECK(rig.source.insert(3, "x"));
  view.edit(rig.model->index(0, 0));
  QPointer<QWidget> second = view.indexWidget(rig.model->index(0, 0));
  CHECK(second);
  rig.source.resetWith({});
  flush();
  CHECK(second.isNull() && !view.editing());
}

// The delegate's setModelData reaches the model's setData and so the container's replace: one announcement, the
// new text in the entry; an editor that the model refuses (empty text) changes nothing.
void the_delegate_writes_through_setData() {
  MapRig rig(true);
  for (int k = 0; k < 3; ++k) CHECK(rig.source.insert(k, QString("v%1").arg(k)));
  QListView view;
  view.setModel(rig.model.get());
  lay_out(view);
  view.openPersistentEditor(rig.model->index(1, 0));
  auto* editor = qobject_cast<QLineEdit*>(view.indexWidget(rig.model->index(1, 0)));
  CHECK(editor);
  int announced = 0;
  QList<int> roles;
  QObject::connect(&rig.source, &decltype(rig.source)::entryChanged, &rig.source, [&](const int&, const QList<int>& r) {
    ++announced;
    roles = r;
  });
  editor->setText("edited");
  emit view.itemDelegate()->commitData(editor);
  CHECK(rig.source.value(1) == "edited" && announced == 1 && roles.isEmpty());
  CHECK(rig.model->index(1, 0).data().toString() == "edited");
  editor->setText("");
  emit view.itemDelegate()->commitData(editor);
  CHECK(rig.source.value(1) == "edited" && announced == 1);  // the editor said no
  CHECK(!rig.model->setData(rig.model->index(1, 0), "x", Qt::DisplayRole));  // not the edit role
  CHECK(announced == 1);
}

// A delegate sizes rows by their text. The table view's sizeHintForRow follows the data (it asks the delegate each time)
// while the row's height stays what it was until the row is resized to contents; the same over QStandardItemModel.
struct OpenTableView : QTableView {
  using QTableView::sizeHintForRow;
};

class LinesDelegate : public QStyledItemDelegate {
 public:
  QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override {
    return QSize(50, kLineHeight * static_cast<int>(index.data().toString().count('\n') + 1));
  }
  static constexpr int kLineHeight = 20;
  static constexpr int kGridLine = 1;  // sizeHintForRow adds the grid line to the delegate's height
};

void size_hints_follow_the_data() {
  MapRig rig;
  OpenTableView view;
  LinesDelegate delegate;
  view.setItemDelegate(&delegate);
  view.setModel(rig.model.get());
  QStandardItemModel reference;
  OpenTableView referenceView;
  referenceView.setItemDelegate(&delegate);
  referenceView.setModel(&reference);
  for (int k = 0; k < 4; ++k) {
    CHECK(rig.source.insert(k, "one"));
    reference.appendRow(new QStandardItem("one"));
  }
  lay_out(view);
  lay_out(referenceView);
  view.resizeRowsToContents();
  referenceView.resizeRowsToContents();
  CHECK(view.rowHeight(1) == referenceView.rowHeight(1));
  CHECK(view.sizeHintForRow(1) == LinesDelegate::kLineHeight + LinesDelegate::kGridLine);

  CHECK(rig.source.replace(1, "a\nb\nc"));
  reference.item(1)->setText("a\nb\nc");
  settle();
  CHECK(view.sizeHintForRow(1) == 3 * LinesDelegate::kLineHeight + LinesDelegate::kGridLine);
  CHECK(view.sizeHintForRow(1) == referenceView.sizeHintForRow(1));
  CHECK(view.rowHeight(1) == referenceView.rowHeight(1));  // not resized: the same stale height as Qt's model gives
  view.resizeRowToContents(1);
  referenceView.resizeRowToContents(1);
  CHECK(view.rowHeight(1) == 3 * LinesDelegate::kLineHeight + LinesDelegate::kGridLine && view.rowHeight(1) == referenceView.rowHeight(1));
  CHECK(rig.source.insertMany(QMap<int, QString>{{-1, "x\ny"}, {9, "z"}}) == 2);  // a row above: heights follow the rows
  settle();
  view.resizeRowsToContents();
  CHECK(view.rowHeight(0) == 2 * LinesDelegate::kLineHeight + LinesDelegate::kGridLine);
  CHECK(view.rowHeight(2) == 3 * LinesDelegate::kLineHeight + LinesDelegate::kGridLine);
}

// A view on a sorted proxy: its current entry, selection and scroll follow the entries when a replace re-sorts them,
// as they do for the same stack over QStringListModel.
void a_view_over_a_sorted_proxy() {
  ListRig ours;
  QStringList words;
  for (int i = 0; i < kRows; ++i) words.append(QString("w%1").arg((i * 7) % kRows, 2, 10, QChar('0')));
  for (const QString& w : words) CHECK(ours.source.insert(ours.source.size(), w));
  QStringListModel reference(words);
  QSortFilterProxyModel oursProxy;
  QSortFilterProxyModel referenceProxy;
  oursProxy.setSourceModel(ours.model.get());
  referenceProxy.setSourceModel(&reference);
  for (QSortFilterProxyModel* p : {&oursProxy, &referenceProxy}) p->sort(0);
  QListView oursView;
  QListView referenceView;
  show_and_scroll(oursView, &oursProxy);
  show_and_scroll(referenceView, &referenceProxy);
  CHECK(state_of(oursView) == state_of(referenceView));
  const int moving = oursProxy.mapToSource(oursProxy.index(kCurrentRow, 0)).row();
  CHECK(ours.source.replace(moving, "zz"));  // the current entry sorts to the end
  reference.setData(reference.index(moving), "zz");
  settle();
  CHECK(state_of(oursView) == state_of(referenceView));
  CHECK(oursView.currentIndex().data().toString() == "zz");
  CHECK(ours.source.insert(0, "aa") && (insert_row_at(reference, 0, "aa"), true));
  CHECK(ours.source.removeRange(10, 5));
  reference.removeRows(10, 5);
  settle();
  CHECK(state_of(oursView) == state_of(referenceView));
  ours.source.resetWith(texts(7));
  reference.setStringList(texts(7));
  settle();
  CHECK(state_of(oursView) == state_of(referenceView));
}

// Qt's default drag and drop for a list (InternalMove) needs a model that can move rows: this model offers no drag
// or drop flags, no moveRows and no drop of mime data, so a drag does not start and a drop is refused; the view, the
// model and the container stay as they were.
void internal_move_is_refused() {
  ListRig rig;
  for (const QString& t : texts(6)) CHECK(rig.source.insert(rig.source.size(), t));
  QListView view;
  view.setModel(rig.model.get());
  view.setDragEnabled(true);
  view.setAcceptDrops(true);
  view.setDragDropMode(QAbstractItemView::InternalMove);
  view.setDefaultDropAction(Qt::MoveAction);
  lay_out(view);
  const QModelIndex row = rig.model->index(1, 0);
  CHECK(!rig.model->flags(row).testFlag(Qt::ItemIsDragEnabled) && !rig.model->flags(row).testFlag(Qt::ItemIsDropEnabled));
  CHECK(!rig.model->moveRows({}, 1, 1, {}, 4));

  QStringListModel reference(texts(6));
  const std::unique_ptr<QMimeData> mime(reference.mimeData({reference.index(1)}));
  CHECK(mime);
  CHECK(!rig.model->canDropMimeData(mime.get(), Qt::MoveAction, 4, 0, {}));
  // Qt's default dropMimeData says yes to a well-formed drop even for a model that can insert nothing (a plain
  // QAbstractListModel does too), and does nothing: no row is inserted, so the content below stays as it is.
  CHECK(rig.model->dropMimeData(mime.get(), Qt::MoveAction, 4, 0, {}));
  CHECK(column(*rig.model) == texts(6));

  const QPoint target = view.visualRect(rig.model->index(4, 0)).center();
  QDragEnterEvent enter(target, Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(view.viewport(), &enter);
  QDragMoveEvent move(target, Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
  move.ignore();
  QCoreApplication::sendEvent(view.viewport(), &move);
  CHECK(!move.isAccepted());
  QDropEvent drop(target, Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
  drop.ignore();
  QCoreApplication::sendEvent(view.viewport(), &drop);
  settle();
  CHECK(!drop.isAccepted());
  CHECK(column(*rig.model) == texts(6) && rig.source.size() == 6);
}

// The Delete key is not handled by the view: no row is removed, and the model hears nothing.
void the_delete_key_removes_nothing() {
  ListRig rig;
  for (const QString& t : texts(6)) CHECK(rig.source.insert(rig.source.size(), t));
  QListView view;
  view.setModel(rig.model.get());
  lay_out(view);
  view.setCurrentIndex(rig.model->index(2, 0));
  int removals = 0;
  QObject::connect(rig.model.get(), &QAbstractItemModel::rowsAboutToBeRemoved, rig.model.get(), [&] { ++removals; });
  for (QWidget* target : {static_cast<QWidget*>(&view), view.viewport()}) {
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &press);
    QCoreApplication::sendEvent(target, &release);
  }
  settle();
  CHECK(removals == 0 && rig.model->rowCount() == 6 && rig.source.size() == 6);
}

// A table view and a tree view over the same model keep the current entry and the selected entries through a batch of
// runs, and drop them on a reset, as the list view does.
template <class View>
void other_views_keep_entries_through_a_batch() {
  MapRig rig;
  QMap<int, QString> rows;
  for (int k = 0; k < 30; ++k) rows.insert(k * 10, QString::number(k * 10));
  rig.source.insertMany(rows);
  View view;
  view.setModel(rig.model.get());
  lay_out(view);
  view.setCurrentIndex(rig.model->index(10, 0));  // key 100
  view.selectionModel()->select(QItemSelection(rig.model->index(9, 0), rig.model->index(12, 0)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  const auto keys = [&] {
    QList<int> out;
    for (const QModelIndex& i : view.selectionModel()->selectedRows()) out.append(i.data(kKeyRole).toInt());
    std::sort(out.begin(), out.end());
    return out;
  };
  const QList<int> selected{90, 100, 110, 120};
  CHECK(keys() == selected);
  CHECK(rig.source.insertMany(QMap<int, QString>{{1, "1"}, {2, "2"}, {105, "105"}, {500, "500"}}) == 4);
  settle();
  CHECK(keys() == selected && view.currentIndex().data(kKeyRole).toInt() == 100);
  CHECK(rig.source.removeMany({0, 10, 80, 125, 400}) == 3);
  settle();
  CHECK(keys() == selected && view.currentIndex().data(kKeyRole).toInt() == 100);
  indexAt_matches_visualRect(view);
  rig.source.resetWith(rows);
  settle();
  CHECK(!view.currentIndex().isValid() && view.selectionModel()->selectedRows().isEmpty());
}

void table_and_tree_views_keep_entries() {
  other_views_keep_entries_through_a_batch<QTableView>();
  other_views_keep_entries_through_a_batch<QTreeView>();
}
}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  a_list_view_keeps_what_it_keeps_over_qts_model();
  views_attached_before_and_after();
  a_batch_keeps_a_reset_drops();
  table_and_tree_views_keep_entries();
  a_view_reading_at_the_about_signal();
  editors_follow_their_rows();
  the_delegate_writes_through_setData();
  size_hints_follow_the_data();
  a_view_over_a_sorted_proxy();
  internal_move_is_refused();
  the_delete_key_removes_nothing();
  return rqt_test::finish("capability_edge_view_widgets");
}
