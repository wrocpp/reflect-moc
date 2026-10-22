// Thread edge cases, lifetime: deleteLater of a container or a model during and after a batch announcement, an event loop
// spun in a receiver at the "about" signal, the order of the destroyed signals, a signal that is still queued when
// its container is deleted, the order in which a thread's finished signal cleans up a container and its model, and
// destructors of value types that run inside the about..done window.
// Catalogue: docs/qt-edge-cases/threads.md.
#include "thread_support.hpp"

#include <QtCore/QEvent>

#include <vector>

namespace {
using namespace edge_thread;

const auto kFormat = [](const int&, const QString& v) { return QVariant(v); };

void flush_deferred_deletes() { QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }

// deleteLater of the model from a slot at the "done" signal of a batch: the batch finishes with the model alive, and the
// model goes at the next turn of the event loop; the container is not affected and goes on.
void a_model_deleted_later_during_a_batch() {
  Source source;
  QPointer<Model> model = new Model(&source, kFormat);
  QObject::connect(&source, &Source::entriesAdded, &source, [&](const QList<int>&) { model->deleteLater(); });
  CHECK(source.insertMany(C{{1, "a"}, {2, "b"}, {4, "c"}}) == 3);
  CHECK(model && model->rowCount() == 3);  // the batch was announced in full
  flush_deferred_deletes();
  CHECK(!model);
  CHECK(source.size() == 3 && source.insert(9, "z") && source.removeMany({1, 2}) == 2 && source.size() == 2);  // the container does not mind
}

// An event loop spun in a receiver at the "about" signal, with the model deleted by deleteLater meanwhile: the model
// goes in the middle of the batch (its pair of signals is half heard by no one), the container finishes its change,
// holds the final content, and announces the "done" signal to whoever is left.
void a_model_deleted_while_a_receiver_spins_the_loop() {
  Source source;
  QPointer<Model> model = new Model(&source, kFormat);
  CHECK(source.insert(1, "a"));
  int doneHeard = 0;
  QObject::connect(&source, &Source::entriesAboutToBeAdded, &source, [&](const QList<int>&) {
    model->deleteLater();
    QCoreApplication::processEvents();               // handles posted events but not a DeferredDelete of this level...
    flush_deferred_deletes();                         // ...so ask for it
  });
  QObject::connect(&source, &Source::entriesAdded, &source, [&](const QList<int>&) { ++doneHeard; });
  CHECK(source.insertMany(C{{2, "b"}, {3, "c"}}) == 2);
  CHECK(!model);
  CHECK(doneHeard == 1 && source.size() == 3);
  // The same for removal and for a reset.
  QPointer<Model> second = new Model(&source, kFormat);
  QObject::connect(&source, &Source::aboutToBeReset, &source, [&] {
    second->deleteLater();
    flush_deferred_deletes();
  });
  source.resetWith(C{{7, "x"}});
  CHECK(!second && source.size() == 1);
}

// deleteLater of the container from a slot at the "done" signal: the batch finishes, the container goes at the next
// turn, a model that is still there keeps its (now unfollowed) rows: see limit_edge_thread_stale_model.
void a_container_deleted_later_during_a_batch() {
  auto* source = new Source();
  QPointer<Source> guard(source);
  Model model(source, kFormat);
  QObject::connect(source, &Source::entriesAdded, source, [&](const QList<int>&) { source->deleteLater(); });
  CHECK(source->insertMany(C{{1, "a"}, {2, "b"}}) == 2);
  CHECK(guard && model.rowCount() == 2);
  flush_deferred_deletes();
  CHECK(!guard);
}

// The destroyed signals: a model that is a child of its container hears of the container's destruction first, and is
// destroyed after it; with the objects side by side the order is the order of the deletes.
void destroyed_signals_in_order() {
  QStringList order;
  {
    auto* source = new Source();
    auto* model = new Model(source, kFormat, source);  // a child of the container
    QObject::connect(source, &QObject::destroyed, [&] { order.append("container"); });
    QObject::connect(model, &QObject::destroyed, [&] { order.append("model"); });
    delete source;
  }
  CHECK(order == QStringList({"container", "model"}));
  order.clear();
  {
    auto* source = new Source();
    auto* model = new Model(source, kFormat);
    QObject::connect(source, &QObject::destroyed, [&] { order.append("container"); });
    QObject::connect(model, &QObject::destroyed, [&] { order.append("model"); });
    delete model;
    delete source;
  }
  CHECK(order == QStringList({"model", "container"}));
  order.clear();
  {
    auto* source = new Source();
    auto* model = new Model(source, kFormat);
    QObject::connect(source, &QObject::destroyed, [&] { order.append("container"); });
    QObject::connect(model, &QObject::destroyed, [&] { order.append("model"); });
    delete source;  // the model outlives it (limit_edge_thread_stale_model)
    delete model;
  }
  CHECK(order == QStringList({"container", "model"}));
  // A model deleted first leaves no connection behind: the container goes on changing.
  Source source;
  delete new Model(&source, kFormat);
  CHECK(source.insert(1, "a") && source.clear());
}

// A signal still queued when its container is deleted: a receiver that gets its signals queued (here a model moved to a
// thread that has not started its loop yet) is delivered the key by value after the sender is gone, and a read
// guarded by QPointer finds nothing; the model does not touch the container for these signals, so nothing crashes.
void a_queued_signal_outlives_its_container() {
  auto* source = new Source();
  QPointer<Source> guard(source);
  auto* model = new Model(source, kFormat);
  QThread thread;
  QObject anchor;
  model->moveToThread(&thread);
  anchor.moveToThread(&thread);
  QList<int> seen;
  QObject::connect(source, &Source::entryAdded, &anchor, [&seen, guard](const int& key) {
    seen.append(key);                  // by value: the key is a copy made when the signal was queued
    if (guard) seen.append(-1);        // would read the container: not reached
  });
  CHECK(source->insert(1, "a") && source->insert(2, "b"));
  delete source;
  CHECK(!guard);
  thread.start();
  QMetaObject::invokeMethod(&anchor, [] {}, Qt::BlockingQueuedConnection);
  CHECK(seen == QList<int>({1, 2}));
  CHECK(model->rowCount() == 0);  // the container's death reset the model: it keeps none of the rows it heard of
  QMetaObject::invokeMethod(&anchor, [&] {
    model->moveToThread(QCoreApplication::instance()->thread());
    anchor.moveToThread(QCoreApplication::instance()->thread());
  }, Qt::BlockingQueuedConnection);
  CHECK(stop(thread));
  delete model;
}

// A container and its model that live in a worker thread, cleaned up by the thread's finished signal with deleteLater: the
// finished signal comes before the thread's last deferred deletes, so both are deleted in the order they were
// asked for, in the worker thread, and a thread that finishes with nothing asked for leaves them to the owner.
void a_finished_thread_cleans_up_in_the_order_asked() {
  for (const bool modelFirst : {true, false}) {
    auto* source = new Source();
    auto* model = new Model(source, kFormat);
    QThread thread;
    source->moveToThread(&thread);
    model->moveToThread(&thread);
    QMutex mutex;
    QStringList order;
    QList<QThread*> deletedIn;
    const auto record = [&](const char* name) {
      QMutexLocker lock(&mutex);
      order.append(name);
      deletedIn.append(QThread::currentThread());
    };
    QObject::connect(source, &QObject::destroyed, [&] { record("container"); });
    QObject::connect(model, &QObject::destroyed, [&] { record("model"); });
    QObject::connect(&thread, &QThread::finished, source, [&] {
      if (modelFirst) {
        model->deleteLater();
        source->deleteLater();
      } else {
        source->deleteLater();
        model->deleteLater();
      }
    }, Qt::DirectConnection);
    QPointer<Source> guardSource(source);
    QPointer<Model> guardModel(model);
    thread.start();
    CHECK(stop(thread));
    CHECK(!guardSource && !guardModel);
    QMutexLocker lock(&mutex);
    CHECK(order == (modelFirst ? QStringList({"model", "container"}) : QStringList({"container", "model"})));
    for (QThread* t : deletedIn) CHECK(t == &thread);
  }
  // Nothing asked for: the objects are still there after the thread, and the owner may delete them from its thread.
  auto* source = new Source();
  auto* model = new Model(source, kFormat);
  QThread thread;
  source->moveToThread(&thread);
  model->moveToThread(&thread);
  thread.start();
  CHECK(stop(thread));
  QPointer<Model> guardModel(model);
  delete model;
  delete source;
  CHECK(!guardModel);
}

// A value type whose destructor runs when its entry is dropped: that is inside the about..done window of the
// change, so anything the destructor does (a signal, a read of the model) happens with the model still showing the old rows.
using Echo = observable::ObservableContainer<QList<int>>;  // a destructor writes here, and its signals follow

enum class Phase { outside, window };
inline Phase g_phase = Phase::outside;
inline Echo* g_echo = nullptr;

struct Tracked {
  int id = 0;
  bool live = true;
  Tracked() = default;
  explicit Tracked(int id) : id(id) {}
  Tracked(const Tracked& o) : id(o.id) {}
  Tracked(Tracked&& o) noexcept : id(o.id), live(o.live) { o.live = false; }
  Tracked& operator=(const Tracked& o) { id = o.id; live = true; return *this; }
  Tracked& operator=(Tracked&& o) noexcept { id = o.id; live = o.live; o.live = false; return *this; }
  ~Tracked() {
    if (live && g_echo) g_echo->insert(g_echo->size(), id);
  }
};

void destructors_run_inside_the_window() {
  using T = QMap<int, Tracked>;
  observable::ObservableContainer<T> source;
  observable::ContainerModel<T> model(&source, [](const int&, const Tracked& t) { return QVariant(t.id); });
  Echo echo;
  g_echo = &echo;
  struct Seen {
    int id;
    Phase phase;
    int rows;
  };
  std::vector<Seen> seen;
  QObject::connect(&echo, &Echo::entryAdded, &echo, [&](const qsizetype& at) { seen.push_back({echo.value(at), g_phase, model.rowCount()}); });
  using S = observable::ObservableContainer<T>;
  const auto enter = [] { g_phase = Phase::window; };
  const auto leave = [] { g_phase = Phase::outside; };
  QObject::connect(&source, &S::entryAboutToBeRemoved, &source, [=](const int&) { enter(); });
  QObject::connect(&source, &S::entryRemoved, &source, [=](const int&) { leave(); });
  QObject::connect(&source, &S::entriesAboutToBeRemoved, &source, [=](const QList<int>&) { enter(); });
  QObject::connect(&source, &S::entriesRemoved, &source, [=](const QList<int>&) { leave(); });
  QObject::connect(&source, &S::aboutToBeCleared, &source, [=] { enter(); });
  QObject::connect(&source, &S::cleared, &source, [=] { leave(); });
  QObject::connect(&source, &S::aboutToBeReset, &source, [=] { enter(); });
  QObject::connect(&source, &S::resetCompleted, &source, [=] { leave(); });

  const auto fill = [&] {
    g_echo = nullptr;
    source.resetWith({});
    for (int k = 1; k <= 3; ++k) source.insert(k, Tracked{k * 10});
    seen.clear();
    g_echo = &echo;
  };
  fill();
  CHECK(source.remove(2));
  CHECK(seen.size() == 1 && seen[0].id == 20 && seen[0].phase == Phase::window && seen[0].rows == 3);  // the model still shows 3 rows
  fill();
  CHECK(source.removeMany({1, 3}) == 2);
  CHECK(seen.size() == 2);
  for (const Seen& s : seen) CHECK(s.phase == Phase::window && s.rows == 3);
  fill();
  CHECK(source.clear());
  CHECK(seen.size() == 3);
  for (const Seen& s : seen) CHECK(s.phase == Phase::window && s.rows == 3);
  fill();
  source.resetWith(T{});
  CHECK(seen.size() == 3);
  for (const Seen& s : seen) CHECK(s.phase == Phase::window && s.rows == 3);
  fill();
  const T taken = source.takeMany({1, 2});
  // takeMany copies each entry into the result before it announces, with no temporary to destroy (it used to copy through
  // a by-value parameter, which left two destructors outside the window), and drops the originals inside it.
  CHECK(taken.size() == 2 && seen.size() == 2);
  for (const Seen& s : seen) CHECK(s.phase == Phase::window && s.rows == 3);
  g_echo = nullptr;
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  a_model_deleted_later_during_a_batch();
  a_model_deleted_while_a_receiver_spins_the_loop();
  a_container_deleted_later_during_a_batch();
  destroyed_signals_in_order();
  a_queued_signal_outlives_its_container();
  a_finished_thread_cleans_up_in_the_order_asked();
  destructors_run_inside_the_window();
  return rqt_test::finish("capability_edge_thread_lifetime");
}
