// Pinned: a signal of the container reaches a receiver on another thread as a queued call, after the change. The
// "about" signal promises that the container still holds the old state; delivered later, a receiver that reads the
// container at the "about" signal finds the NEW state (the entry already added, or already gone), the same as at
// the "done" signal. The key arrives by value and is right.
// A receiver that is quick enough can also read the old state (its queued call can run between the emit of "about"
// and the change itself): which one it sees is a race, so this test does not leave it to chance. It holds the
// receiver's thread in a gate until the change is done, which is the case where the receiver is not faster than the
// change, and then the new state is what it reads.
// A fix is to document that the about/done contract holds for direct connections only, or to carry the old state in
// the signal; this file then becomes capability_edge_thread_queued_about.
// Catalogue: docs/qt-edge-cases/threads.md.
#include "thread_support.hpp"

namespace {
using namespace edge_thread;

struct Reading {
  int key = -1;
  qsizetype size = -1;
  bool contains = false;
};

void a_receiver_on_another_thread_reads_the_new_state_at_about() {
  Source source;
  QThread thread;
  QObject receiver;
  receiver.moveToThread(&thread);
  QMutex mutex;
  Reading addAbout, addDone, removeAbout, removeDone;
  std::atomic<int> heard{0};
  const auto reader = [&](Reading& into) {
    return [&, &into = into](const int& key) {
      QMutexLocker lock(&mutex);
      into = {key, source.size(), source.contains(key)};
      ++heard;
    };
  };
  QObject::connect(&source, &Source::entryAboutToBeAdded, &receiver, reader(addAbout));
  QObject::connect(&source, &Source::entryAdded, &receiver, reader(addDone));
  QObject::connect(&source, &Source::entryAboutToBeRemoved, &receiver, reader(removeAbout));
  QObject::connect(&source, &Source::entryRemoved, &receiver, reader(removeDone));
  thread.start();

  // The gate: the receiver's thread waits here, with its queue filling, until `release` is set (bounded).
  std::atomic<bool> release{false};
  const auto close_gate = [&] {
    QMetaObject::invokeMethod(&receiver, [&] {
      for (int waited = 0; !release.load() && waited < kWaitMs; ++waited) QThread::msleep(1);
    }, Qt::QueuedConnection);
  };
  close_gate();
  CHECK(source.insert(7, "x"));
  release = true;
  CHECK(wait_for([&] { return heard.load() == 2; }));
  {
    QMutexLocker lock(&mutex);
    CHECK(addAbout.key == 7 && addDone.key == 7);       // by value, right
    CHECK(addDone.size == 1 && addDone.contains);        // done: the new state, as promised
    CHECK(addAbout.size == 1 && addAbout.contains);      // about: the new state too, where "about" promises the old one
  }
  release = false;
  close_gate();
  CHECK(source.remove(7));
  release = true;
  CHECK(wait_for([&] { return heard.load() == 4; }));
  {
    QMutexLocker lock(&mutex);
    CHECK(removeAbout.key == 7 && removeDone.key == 7);
    CHECK(removeDone.size == 0 && !removeDone.contains);
    CHECK(removeAbout.size == 0 && !removeAbout.contains);  // the entry is already gone at "about to be removed"
  }
  CHECK(stop(thread));
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  a_receiver_on_another_thread_reads_the_new_state_at_about();
  return rqt_test::finish("limit_edge_thread_queued_about");
}
