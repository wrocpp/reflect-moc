// Thread edge cases, the one-thread rule in a debug build: a container and the models that follow it live in one
// thread, and Q_ASSERT_X says so. Each scenario that breaks the rule runs in a forked child and must die of SIGABRT; the
// control that keeps the rule must not. (With the assertion compiled out, limit_edge_thread_no_assert shows what
// happens instead.)
// Catalogue: docs/qt-edge-cases/threads.md.
#include "thread_support.hpp"

#ifndef QT_NO_DEBUG
namespace {
using namespace edge_thread;

const auto kFormat = [](const int&, const QString& v) { return QVariant(v); };

// Runs `body` on a second thread, while the caller waits, so that the call happens off the container's thread.
template <class F>
void on_other_thread(F&& body) {
  QThread worker;
  QObject::connect(&worker, &QThread::started, &worker, [&] { body(); worker.quit(); }, Qt::DirectConnection);
  worker.start();
  if (!worker.wait(kWaitMs)) _exit(3);  // a hang is not an abort
}

void the_control_keeps_the_rule() {
  CHECK(!aborts([] {
    Source source;
    Model model(&source, kFormat);
    source.insert(1, "a");
    source.insertMany(C{{2, "b"}});
    source.resetWith({});
  }));
}

// Every mutator asserts first, so a refused call from another thread asserts too.
void every_mutator_asserts_off_its_thread() {
  const auto from_other = [](auto&& call) {
    return aborts([&] {
      Source source;
      source.insert(1, "a");
      on_other_thread([&] { call(source); });
    });
  };
  CHECK(from_other([](Source& s) { s.insert(2, "b"); }));
  CHECK(from_other([](Source& s) { s.insert(1, "dup"); }));  // refused, still asserts
  CHECK(from_other([](Source& s) { s.remove(1); }));
  CHECK(from_other([](Source& s) { s.remove(99); }));
  CHECK(from_other([](Source& s) { s.replace(1, "c"); }));
  CHECK(from_other([](Source& s) { s.editValue(1, [](QString& v) { v = "d"; }); }));
  CHECK(from_other([](Source& s) { s.insertMany(C{{5, "e"}}); }));
  CHECK(from_other([](Source& s) { s.removeMany({1}); }));
  CHECK(from_other([](Source& s) { s.takeMany({1}); }));
  CHECK(from_other([](Source& s) { s.resetWith(C{{7, "f"}}); }));
  CHECK(from_other([](Source& s) { s.clear(); }));
  // A const read is not asserted: it is the caller's race.
  CHECK(!aborts([&] {
    Source source;
    source.insert(1, "a");
    on_other_thread([&] { (void)source.size(); (void)source.contains(1); (void)source.value(1); });
  }));
  // A list's mutators.
  const auto list_from_other = [](auto&& call) {
    return aborts([&] {
      observable::ObservableContainer<QList<QString>> source;
      source.insert(0, "a");
      source.insert(1, "b");
      on_other_thread([&] { call(source); });
    });
  };
  CHECK(list_from_other([](auto& s) { s.move(0, 1); }));
  CHECK(list_from_other([](auto& s) { s.insertRange(0, QList<QString>{"x"}); }));
  CHECK(list_from_other([](auto& s) { s.removeRange(0, 1); }));
}

// A container moved to another thread is no longer on the thread that calls it.
void a_moved_container_asserts_on_the_next_change() {
  CHECK(aborts([] {
    Source source;
    QThread other;
    source.moveToThread(&other);
    source.insert(1, "a");
  }));
  // Moving it to the thread it is on is a no-op. (Moving it back from the calling thread does not work: Qt only lets
  // an object be moved by the thread it lives in, so it stays where it was and the next change asserts.)
  CHECK(!aborts([] {
    Source source;
    source.moveToThread(QThread::currentThread());
    source.insert(1, "a");
  }));
  CHECK(aborts([] {
    Source source;
    QThread other;
    source.moveToThread(&other);
    source.moveToThread(QThread::currentThread());
    source.insert(1, "a");
  }));
}

// A model made on another thread than its container, or on the container's thread and then moved away, is refused at
// construction only in the first case.
void a_model_on_another_thread_asserts_at_construction() {
  CHECK(aborts([] {  // the container moved, then a model made here
    Source source;
    QThread other;
    source.moveToThread(&other);
    Model model(&source, kFormat);
  }));
  CHECK(aborts([] {  // a model made on a worker thread for a container on the main one
    Source source;
    on_other_thread([&] { Model model(&source, kFormat); });
  }));
  CHECK(!aborts([] {  // moved after construction: nothing is checked any more (limit_edge_thread_no_assert)
    Source source;
    Model model(&source, kFormat);
    QThread other;
    model.moveToThread(&other);
    model.moveToThread(QThread::currentThread());
  }));
}
}  // namespace
#endif

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
#ifndef QT_NO_DEBUG
  // The forks come first: they need a process with no other thread.
  the_control_keeps_the_rule();
  every_mutator_asserts_off_its_thread();
  a_moved_container_asserts_on_the_next_change();
  a_model_on_another_thread_asserts_at_construction();
#else
  std::printf("capability_edge_thread_rules: skipped, assertions are compiled out\n");
#endif
  return rqt_test::finish("capability_edge_thread_rules");
}
