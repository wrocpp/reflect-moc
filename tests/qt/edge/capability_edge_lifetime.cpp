// Who outlives whom. A container destroyed before its model (the model holds it by QPointer and resets to empty), the
// reverse, with a selection model, a proxy and a persistent index alive, several models on one container destroyed in
// every order, a model made in the middle of an announcement, a container or a model deleted from a receiver, a
// connection cut during an emission, and a container with a parent that is deleted along with its models.
#include "edge_support.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtTest/QAbstractItemModelTester>

#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include <vector>

namespace {
using namespace edge;
using Listed = observable::ObservableContainer<QList<Sample>>;
using ListModel = observable::ContainerModel<QList<Sample>>;
using ListTable = observable::ContainerTableModel<QList<Sample>>;
using M = QAbstractItemModel;

constexpr int kModels = 3;  // models on one container in the order tests

// Counts the resets a model announces. A QObject, so that the connection goes with it.
class Resets : public QObject {
 public:
  explicit Resets(QAbstractItemModel* model) {
    connect(model, &M::modelAboutToBeReset, this, [this] { ++about; });
    connect(model, &M::modelReset, this, [this] { ++done; });
  }
  int about = 0;
  int done = 0;
};

std::unique_ptr<ListTable> tableOver(Listed* src) {
  std::vector<observable::Column<QList<Sample>>> columns;
  columns.push_back({"value", ListKit::show, parse});
  return std::make_unique<ListTable>(src, std::move(columns));
}

// The container goes first and the models are still there: each resets to empty, once, and every call is safe.
void container_destroyed_before_its_model() {
  auto* src = new Listed(ListKit::initial());
  const auto table = tableOver(src);
  const auto list = std::make_unique<ListModel>(src, ListKit::show, parse);
  QAbstractItemModelTester tableTester(table.get(), QAbstractItemModelTester::FailureReportingMode::Fatal);
  QAbstractItemModelTester listTester(list.get(), QAbstractItemModelTester::FailureReportingMode::Fatal);
  Resets tableResets(table.get());
  Resets listResets(list.get());
  CHECK(table->rowCount() == kEntries && list->rowCount() == kEntries);
  const QPersistentModelIndex persistent(list->index(2));
  const QModelIndex staleList = list->index(2, 0);  // a plain index a caller kept
  const QModelIndex staleTable = table->index(0, 0);
  delete src;
  for (const Resets* resets : {&tableResets, &listResets}) CHECK(resets->about == 1 && resets->done == 1);
  CHECK(table->rowCount() == 0 && list->rowCount() == 0);
  CHECK(!persistent.isValid());
  CHECK(!table->index(0, 0).isValid() && !list->index(0, 0).isValid());
  CHECK(!table->data(staleTable).isValid());  // an index made up by a caller
  CHECK(!list->data(staleList).isValid());
  CHECK(!list->data(staleList, Qt::UserRole).isValid());
  CHECK(!list->setData(staleList, kEditedValue));
  CHECK(!table->setData(staleTable, kEditedValue));
  (void)list->flags(staleList);
  (void)table->flags(staleTable);
  CHECK(table->headerData(0, Qt::Horizontal).toString() == "value");
  CHECK(table->columnCount() == 1);
  (void)list->roleNames();
  CHECK(list->rowCount(list->index(0, 0)) == 0);
}

// R-11: the model goes first, and the container goes on working.
void model_destroyed_before_its_container() {
  Listed src(ListKit::initial());
  auto model = std::make_unique<ListModel>(&src, ListKit::show);
  model.reset();
  CHECK(src.insert(0, Sample{kNestedValue}));
  CHECK(src.remove(0));
  CHECK(src.size() == kEntries);
}

// A proxy, a selection model and a persistent index on a model. The selection model is a child of the model: in the
// Qt 6.10 image one that is destroyed after its model crashes (a plain QStringListModel does it too), so the order
// "model first, selection model later" is not something these tests can exercise.
struct Attached {
  QSortFilterProxyModel proxy;
  QPointer<QItemSelectionModel> selection;
  QPersistentModelIndex persistent;
  explicit Attached(ListModel* model) : selection(new QItemSelectionModel(model, model)) {
    proxy.setSourceModel(model);
    selection->select(model->index(1), QItemSelectionModel::ClearAndSelect);
    persistent = model->index(2);
  }
};

void container_destroyed_before_selection_proxy_and_persistent_index() {
  auto* src = new Listed(ListKit::initial());
  auto* model = new ListModel(src, ListKit::show);
  Attached attached(model);
  CHECK(attached.selection->hasSelection() && attached.persistent.isValid() && attached.proxy.rowCount() == kEntries);
  delete src;
  // the model resets: nothing is selected, the persistent index and the proxy's rows are gone
  CHECK(!attached.selection->hasSelection());
  CHECK(!attached.persistent.isValid());
  CHECK(attached.proxy.rowCount() == 0);
  delete model;
  CHECK(!attached.persistent.isValid());
  CHECK(attached.proxy.rowCount() == 0);
}

void model_destroyed_before_container_with_selection_proxy_and_persistent_index() {
  Listed src(ListKit::initial());
  auto* model = new ListModel(&src, ListKit::show);
  Attached attached(model);
  delete model;
  CHECK(!attached.persistent.isValid());
  CHECK(!attached.selection);
  CHECK(attached.proxy.rowCount() == 0);
  CHECK(src.insert(0, Sample{kNestedValue}));
  CHECK(src.size() == kEntries + 1);
}

// R-14: three models and a container, destroyed in every order of the models, the container last. After each
// destruction the container changes and every model that is left must follow.
void models_destroyed_in_every_order_before_the_container() {
  std::array<int, kModels> order{};
  std::iota(order.begin(), order.end(), 0);
  do {
    auto* src = new Listed(ListKit::initial());
    std::array<ListModel*, kModels> models{};
    std::array<std::unique_ptr<ModelWatcher>, kModels> watchers;
    for (int i = 0; i < kModels; ++i) {
      models[i] = new ListModel(src, ListKit::show);
      watchers[i] = std::make_unique<ModelWatcher>(models[i], false);
    }
    int alive = kModels;
    for (int which : order) {
      watchers[which].reset();
      delete models[which];
      models[which] = nullptr;
      --alive;
      CHECK(src->insert(0, Sample{kNestedValue}));
      for (int i = 0; i < kModels; ++i)
        if (models[i]) CHECK(agrees(*models[i], src->items()));
    }
    CHECK(alive == 0);
    delete src;
  } while (std::next_permutation(order.begin(), order.end()));
}

// The container is one of four objects destroyed in every order. While the container is there the models follow it.
// Once it is gone, each model that is left has reset to empty, once.
void container_destroyed_at_every_position_among_three_models() {
  constexpr int kContainer = kModels;
  std::array<int, kModels + 1> order{};
  std::iota(order.begin(), order.end(), 0);
  int survivors = 0;
  do {
    auto* src = new Listed(ListKit::initial());
    std::array<ListModel*, kModels> models{};
    std::array<std::unique_ptr<Resets>, kModels> resets;
    for (int i = 0; i < kModels; ++i) {
      models[i] = new ListModel(src, ListKit::show);
      resets[i] = std::make_unique<Resets>(models[i]);
    }
    bool sourceGone = false;
    for (int which : order) {
      if (which == kContainer) {
        const int size = static_cast<int>(src->size());
        delete src;
        sourceGone = true;
        for (int i = 0; i < kModels; ++i) {
          if (!models[i]) continue;
          CHECK(size > 0 && models[i]->rowCount() == 0 && resets[i]->about == 1 && resets[i]->done == 1);
          ++survivors;
        }
      } else {
        resets[which].reset();
        delete models[which];
        models[which] = nullptr;
      }
      if (sourceGone) continue;
      CHECK(src->insert(0, Sample{kNestedValue}));
      for (int i = 0; i < kModels; ++i)
        if (models[i]) CHECK(agrees(*models[i], src->items()));
    }
  } while (std::next_permutation(order.begin(), order.end()));
  CHECK(survivors > 0);
}

// A model made inside a receiver, for every change of a list and of a map, at the four places a receiver can sit.
enum class At { containerAbout, containerDone, modelAbout, modelDone };

template <class Kit>
int createdInside(const Outer<Kit>& out, At at) {
  using F = Fx<Kit>;
  const bool onModel = at == At::modelAbout || at == At::modelDone;
  const bool about = at == At::containerAbout || at == At::modelAbout;
  const bool present = onModel ? static_cast<bool>(about ? out.modelAbout : out.modelDone)
                               : static_cast<bool>(about ? out.containerAbout : out.containerDone);
  if (!present) return kNoHook;
  F fx;
  std::unique_ptr<typename F::Mdl> late;
  std::unique_ptr<ModelWatcher> lateWatcher;
  const Receiver receiver = [&] {
    if (late) return;
    late = std::make_unique<typename F::Mdl>(&fx.src, Kit::show);
    lateWatcher = std::make_unique<ModelWatcher>(late.get(), Kit::kStableKeys);
  };
  if (onModel) (about ? out.modelAbout : out.modelDone)(*fx.model, receiver);
  else (about ? out.containerAbout : out.containerDone)(fx.src, receiver);
  out.run(fx);
  fx.watcher->check();
  int code = 0;
  if (!fx.watcher->clean() || !agrees(*fx.model, fx.src.items())) code |= kDirty;
  if (!late) return code | kSkipped;
  lateWatcher->check();
  if (!lateWatcher->clean() || !agrees(*late, fx.src.items())) code |= kDirty;
  fx.src.resetWith(Kit::initial());  // and the new model follows the next change
  lateWatcher->check();
  if (!lateWatcher->clean() || !agrees(*late, fx.src.items())) code |= kDirty;
  return code;
}

struct CreatedTally {
  int total = 0, crashed = 0, dirty = 0;
};

template <class Kit>
CreatedTally createdSweep(const std::vector<Outer<Kit>>& outers, At at) {
  CreatedTally t;
  for (const auto& out : outers) {
    const int code = forked([&] { return createdInside<Kit>(out, at); });
    if (code == kNoHook) continue;
    ++t.total;
    if (code < 0) ++t.crashed;
    else if (code & kDirty) ++t.dirty;
    if (code < 0 || (code & kDirty)) std::printf("  %s: %s at %d: %s\n", Kit::kName, out.name.c_str(), static_cast<int>(at), code < 0 ? "crashed" : "out of step");
  }
  return t;
}

CreatedTally createdBoth(At at) {
  const CreatedTally l = createdSweep<ListKit>(listOuters(), at);
  const CreatedTally m = createdSweep<MapKit>(mapOuters(), at);
  std::printf("created at %d: %d cases, %d crashed, %d out of step\n", static_cast<int>(at), l.total + m.total, l.crashed + m.crashed, l.dirty + m.dirty);
  return {l.total + m.total, l.crashed + m.crashed, l.dirty + m.dirty};
}

// The model did not hear the "about" signal, so at the "done" signal it follows the container with a reset.
void model_created_in_an_about_receiver() {
  for (At at : {At::containerAbout, At::modelAbout}) {
    const CreatedTally t = createdBoth(at);
    CHECK(t.total > 0);
    CHECK(t.crashed == 0 && t.dirty == 0);
  }
}

void model_created_in_a_done_receiver() {
  const CreatedTally t = createdBoth(At::containerDone);
  CHECK(t.total > 0);
  CHECK(t.crashed == 0 && t.dirty == 0);
}

void model_created_in_a_model_done_receiver() {
  const CreatedTally t = createdBoth(At::modelDone);
  CHECK(t.total > 0);
  CHECK(t.crashed == 0 && t.dirty == 0);
}

// The container is deleted with deleteLater from its own receiver: the announcement completes, and the container goes
// when control returns to the event loop. (Deleting it inside the receiver is undefined behaviour, as for any
// QObject that is deleted while it emits.)
void container_deleted_later_inside_its_own_receiver() {
  auto* src = new Listed(ListKit::initial());
  QPointer<Listed> guard(src);
  auto* model = new ListModel(src, ListKit::show);
  auto* watcher = new ModelWatcher(model, false);  // a child of the model, so it is not on the stack
  Resets resets(model);
  QObject::connect(src, &Listed::entryAdded, src, [src](const qsizetype&) { src->deleteLater(); });
  CHECK(src->insert(0, Sample{kNestedValue}));
  watcher->check();
  CHECK(watcher->clean());
  CHECK(model->rowCount() == kEntries + 1);
  CHECK(guard);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  CHECK(!guard);
  CHECK(resets.done == 1 && model->rowCount() == 0);
  delete model;
}

// A model deleted inside a receiver of the container: before or after it heard the announcement, with a proxy on it
// and a second model on the same container that has to carry on.
template <class Kit>
int modelDeletedInside(const Outer<Kit>& out, Where where, Phase phase) {
  using F = Fx<Kit>;
  const auto& hook = phase == Phase::about ? out.containerAbout : out.containerDone;
  if (!hook) return kNoHook;
  F* fxp = nullptr;
  bool fired = false;
  const Receiver receiver = [&] {
    if (fired || !fxp) return;
    fired = true;
    fxp->watcher.reset();
    fxp->model.reset();
  };
  std::unique_ptr<F> owned = where == Where::containerEarly
                                 ? std::make_unique<F>([&](typename F::Src& s) { hook(s, receiver); })
                                 : std::make_unique<F>();
  F& fx = *owned;
  fxp = &fx;
  if (where == Where::containerLate) hook(fx.src, receiver);
  QSortFilterProxyModel proxy;
  proxy.setSourceModel(fx.model.get());
  typename F::Mdl other(&fx.src, Kit::show);
  ModelWatcher otherWatcher(&other, Kit::kStableKeys);
  out.run(fx);
  otherWatcher.check();
  int code = 0;
  if (!fired || fx.model) code |= kSkipped;
  if (!otherWatcher.clean() || !agrees(other, fx.src.items())) code |= kDirty;
  if (proxy.rowCount() != 0) code |= kDirty;
  fx.src.resetWith(Kit::initial());
  otherWatcher.check();
  if (!otherWatcher.clean() || !agrees(other, fx.src.items())) code |= kDirty;
  return code;
}

template <class Kit>
void deleted_inside_sweep(const std::vector<Outer<Kit>>& outers, int& cases) {
  for (const auto& out : outers)
    for (Where where : {Where::containerEarly, Where::containerLate})
      for (Phase phase : {Phase::about, Phase::done}) {
        const int code = forked([&] { return modelDeletedInside<Kit>(out, where, phase); });
        if (code == kNoHook) continue;
        ++cases;
        CHECK(code == 0);
        if (code != 0) std::printf("  %s %s early=%d about=%d: code %d\n", Kit::kName, out.name.c_str(), where == Where::containerEarly, phase == Phase::about, code);
      }
}

void model_deleted_inside_a_container_receiver() {
  int cases = 0;
  deleted_inside_sweep<ListKit>(listOuters(), cases);
  deleted_inside_sweep<MapKit>(mapOuters(), cases);
  CHECK(cases > 0);
}

// The model's connections to the container are cut during an emission. Before the model has heard the "about" signal
// it hears nothing of the change: it keeps its rows and reads the live container for them. After it has, it has begun a change that nothing ends.
void a_connection_cut_before_the_model_heard_the_announcement() {
  Fx<ListKit>* fxp = nullptr;
  Fx<ListKit> fx([&](Listed& s) {
    QObject::connect(&s, &Listed::entryAboutToBeAdded, &s, [&](const qsizetype&) {
      if (fxp) QObject::disconnect(&fxp->src, nullptr, fxp->model.get(), nullptr);
    });
  });
  fxp = &fx;
  CHECK(fx.src.insert(0, Sample{kNestedValue}));
  // Deaf, the model keeps its row count and reads the live container for the rows it has: nothing is read out of range.
  CHECK(fx.model->rowCount() == kEntries && fx.src.size() == kEntries + 1);
  for (int row = 0; row < fx.model->rowCount(); ++row) CHECK(fx.model->index(row).data().toInt() == fx.src.items()[row].value);
}

// A container with a parent, deleted with the parent and the models that are its children. The children go in the
// order they were given to the parent: models first, or the container first.
void parent_deletes_models_before_the_container() {
  auto* parent = new QObject;
  auto* src = new Listed(ListKit::initial());
  auto* model = new ListModel(src, ListKit::show, parent);
  auto* proxy = new QSortFilterProxyModel(parent);
  proxy->setSourceModel(model);
  src->setParent(parent);
  Resets resets(model);
  delete parent;
  CHECK(resets.done == 0);
}

void parent_deletes_the_container_before_the_models() {
  auto* parent = new QObject;
  auto* src = new Listed(ListKit::initial(), parent);
  auto* model = new ListModel(src, ListKit::show, parent);
  auto* proxy = new QSortFilterProxyModel(parent);
  proxy->setSourceModel(model);
  Resets resets(model);
  delete parent;
  CHECK(resets.about == 1 && resets.done == 1);  // the container went first, and the model reset
}

void a_model_that_is_a_child_of_its_container() {
  auto* src = new Listed(ListKit::initial());
  auto* model = new ListModel(src, ListKit::show, src);
  Resets resets(model);
  delete src;
  CHECK(resets.about == 1 && resets.done == 1);
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  container_destroyed_before_its_model();
  model_destroyed_before_its_container();
  container_destroyed_before_selection_proxy_and_persistent_index();
  model_destroyed_before_container_with_selection_proxy_and_persistent_index();
  models_destroyed_in_every_order_before_the_container();
  container_destroyed_at_every_position_among_three_models();
  model_created_in_an_about_receiver();
  model_created_in_a_done_receiver();
  model_created_in_a_model_done_receiver();
  container_deleted_later_inside_its_own_receiver();
  model_deleted_inside_a_container_receiver();
  a_connection_cut_before_the_model_heard_the_announcement();
  parent_deletes_models_before_the_container();
  parent_deletes_the_container_before_the_models();
  a_model_that_is_a_child_of_its_container();
  return rqt_test::finish("capability_edge_lifetime");
}
