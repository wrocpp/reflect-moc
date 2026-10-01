// Queued connections across a QThread: string connect, rqt::connect, and a
// QThread-derived reflected class (rqt::Object<QThread>) as the sender.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>

#include <atomic>

namespace {
constexpr int kFirst = 21;
constexpr int kSecond = 22;
constexpr int kTicks = 3;
}  // namespace

struct Producer : rqt::Object<> {
  Producer() { bind(); }
  [[= rqt::signal_function]] void produced(int value) { rqt::emit{this}(value); }
  [[= rqt::slot]] void produce(int v) { produced(v); }
};

struct Consumer : rqt::Object<> {
  Consumer() { bind(); }
  std::atomic<int> got{0};
  std::atomic<QThread*> seen_on{nullptr};
  QSemaphore done;
  [[= rqt::slot]] void consume(int v) {
    got = v;
    seen_on = QThread::currentThread();
    done.release();
  }
};

// A QThread subclass as the reflected base: run() emits from the worker thread.
struct Ticker : rqt::Object<QThread> {
  Ticker() { bind(); }
  [[= rqt::signal_function]] void ticked(int n) { rqt::emit{this}(n); }
  void run() override {
    for (int i = 1; i <= kTicks; ++i) ticked(i);
  }
};

struct Collector : rqt::Object<> {
  Collector() { bind(); }
  int sum = 0;
  int count = 0;
  [[= rqt::slot]] void collect(int n) {
    sum += n;
    ++count;
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  Producer producer;
  Consumer consumer;
  QThread thread;
  consumer.moveToThread(&thread);
  thread.start();

  auto const string_connection =
      QObject::connect(&producer, SIGNAL(produced(int)), &consumer, SLOT(consume(int)), Qt::QueuedConnection);
  CHECK(string_connection);
  producer.produce(kFirst);
  consumer.done.acquire();
  CHECK(consumer.got == kFirst && consumer.seen_on == &thread);
  QObject::disconnect(string_connection);

  auto const index_connection =
      rqt::connect(&producer, &Producer::produced, &consumer, &Consumer::consume, Qt::QueuedConnection);
  CHECK(index_connection);
  producer.produce(kSecond);
  consumer.done.acquire();
  CHECK(consumer.got == kSecond && consumer.seen_on == &thread);

  thread.quit();
  thread.wait();

  // the sender is the thread object itself
  Ticker ticker;
  Collector collector;
  CHECK(QObject::connect(&ticker, SIGNAL(ticked(int)), &collector, SLOT(collect(int))));
  ticker.start();
  ticker.wait();
  QCoreApplication::processEvents();
  CHECK(collector.count == kTicks && collector.sum == kTicks * (kTicks + 1) / 2);
  return rqt_test::finish("capability_queued_across_qthread");
}
