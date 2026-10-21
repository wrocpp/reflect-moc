// Pinned: the one-thread rule is an assertion and nothing else, so in a build without assertions (a release build) a
// container or model on the wrong thread is accepted, and what follows is queued delivery:
//   - a container changed from another thread tells a model in its own thread through queued signals: for a while
//     the container holds the new content and the model lists the old one;
//   - a model moved to another thread (or made on one) hears the container through queued signals too, so it follows
//     only when its thread runs an event loop, and not at all while that thread has none;
//   - a container moved away from the thread that changes it is changed without a word.
// The results are deterministic because no thread touches the container while another reads it: each thread waits.
// A fix keeps the rule in release builds (a check that refuses, or a documented queued mode); this file then becomes
// capability_edge_thread_no_assert.
// Catalogue: docs/qt-edge-cases/threads.md.
#define QT_NO_DEBUG
#include "thread_support.hpp"

namespace {
using namespace edge_thread;

const auto kFormat = [](const int&, const QString& v) { return QVariant(v); };

// Everything posted to `thread` before this call has been handled when it returns.
void barrier(QThread& thread, QObject& anchor) {
  CHECK(anchor.thread() == &thread);
  QMetaObject::invokeMethod(&anchor, [] {}, Qt::BlockingQueuedConnection);
}

void a_container_changed_from_another_thread_reaches_the_model_late() {
  Source source;
  Model model(&source, kFormat);
  QThread worker;
  QObject::connect(&worker, &QThread::started, &worker, [&] {
    source.insert(1, "a");
    source.insertMany(C{{2, "b"}, {3, "c"}});
    worker.quit();
  }, Qt::DirectConnection);
  worker.start();
  CHECK(worker.wait(kWaitMs));
  CHECK(source.size() == 3);                 // the container has the new content
  CHECK(model.rowCount() == 0);              // and the model has not heard: its signals wait in its thread's queue
  CHECK(wait_for([&] { return model.rowCount() == 3; }));
  CHECK(model.index(0, 0).data().toString() == "a" && model.index(2, 0).data().toString() == "c");
}

void a_model_moved_to_a_thread_without_a_loop_stops_following() {
  Source source;
  auto* model = new Model(&source, kFormat);
  QThread other;
  QObject anchor;
  model->moveToThread(&other);
  anchor.moveToThread(&other);
  CHECK(source.insert(1, "a") && source.insert(2, "b"));
  QThread::msleep(20);
  CHECK(source.size() == 2 && model->rowCount() == 0);  // nothing is delivered: the thread has no loop
  other.start();
  barrier(other, anchor);                                // now it runs the queued signals, in order
  CHECK(model->rowCount() == 2);
  // An object is moved only by the thread it lives in: send both back from there.
  QMetaObject::invokeMethod(&anchor, [&] {
    model->moveToThread(QCoreApplication::instance()->thread());
    anchor.moveToThread(QCoreApplication::instance()->thread());
  }, Qt::BlockingQueuedConnection);
  CHECK(stop(other));
  delete model;
}

void a_model_made_on_another_thread_follows_through_its_loop() {
  Source source;
  Model* model = nullptr;
  QThread worker;
  QObject anchor;
  anchor.moveToThread(&worker);
  std::atomic<bool> made{false};
  QMetaObject::invokeMethod(&anchor, [&] {
    model = new Model(&source, kFormat);  // no assertion: it lives on the worker thread, the container on this one
    made = true;
  }, Qt::QueuedConnection);
  worker.start();
  CHECK(wait_flag(made));
  CHECK(source.insert(1, "a") && source.insert(2, "b"));
  barrier(worker, anchor);
  CHECK(model->thread() == &worker && model->rowCount() == 2);
  QMetaObject::invokeMethod(&anchor, [&] { model->moveToThread(QCoreApplication::instance()->thread()); }, Qt::BlockingQueuedConnection);
  CHECK(stop(worker));
  delete model;
}

void a_moved_container_is_changed_without_a_word() {
  Source source;
  Model model(&source, kFormat);
  QThread other;
  source.moveToThread(&other);
  CHECK(source.insert(1, "a"));  // from the main thread, the container's thread is `other`
  CHECK(model.rowCount() == 1);  // the emitting thread is the model's thread: delivered directly
  CHECK(source.thread() == &other);
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  a_container_changed_from_another_thread_reaches_the_model_late();
  a_model_moved_to_a_thread_without_a_loop_stops_following();
  a_model_made_on_another_thread_follows_through_its_loop();
  a_moved_container_is_changed_without_a_word();
  return rqt_test::finish("limit_edge_thread_no_assert");
}
