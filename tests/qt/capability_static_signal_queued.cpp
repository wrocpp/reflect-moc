// A static signal across threads: a queued connection from the main thread to a slot on a worker
// thread, and a QThread subclass that emits one from run() to a receiver on the main thread. Run it
// under ThreadSanitizer (scripts/tsan.sh): a static signal object is shared by every instance, so it
// must not carry per-emission state.
#include "check.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>

#include <reflect_moc/compat.hpp>

#include <atomic>

namespace {
constexpr int kFirst = 21;
constexpr int kTicks = 3;
constexpr int kBursts = 50;
}  // namespace

class Producer : public QObject {
  Q_OBJECT

 public:
  explicit Producer(QObject* parent = nullptr) : QObject(parent) {}
  void produce(int v) { emit produced(v); }

 signals:
  static inline rqt::static_signal<void(int)> produced{};
};

class Consumer : public QObject {
  Q_OBJECT

 public:
  explicit Consumer(QObject* parent = nullptr) : QObject(parent) {}
  std::atomic<int> got{0};
  std::atomic<int> calls{0};
  std::atomic<QThread*> seen_on{nullptr};
  QSemaphore done;

 public slots:
  [[= rqt::slot]] void consume(int v) {
    got = v;
    ++calls;
    seen_on = QThread::currentThread();
    done.release();
  }
};

// emits from run(), on the worker thread
class Ticker : public QThread {
  Q_OBJECT

 public:
  explicit Ticker(QObject* parent = nullptr) : QThread(parent) {}
  void run() override {
    for (int i = 1; i <= kTicks; ++i) emit ticked(i);
  }

 signals:
  static inline rqt::static_signal<void(int)> ticked{};
};

class Collector : public QObject {
  Q_OBJECT

 public:
  explicit Collector(QObject* parent = nullptr) : QObject(parent) {}
  int sum = 0;
  int count = 0;

 public slots:
  [[= rqt::slot]] void collect(int n) {
    sum += n;
    ++count;
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  // main thread to a worker thread, queued
  Producer producer;
  Consumer consumer;
  QThread thread;
  consumer.moveToThread(&thread);
  thread.start();
  CHECK(QObject::connect(&producer, &Producer::produced, &consumer, &Consumer::consume, Qt::QueuedConnection));
  producer.produce(kFirst);
  consumer.done.acquire();
  CHECK(consumer.got == kFirst && consumer.seen_on == &thread);

  // many instances share one static signal object: each emission reaches its own receiver only
  Producer second_producer;
  Consumer second_consumer;
  second_consumer.moveToThread(&thread);
  CHECK(QObject::connect(&second_producer, &Producer::produced, &second_consumer, &Consumer::consume,
                         Qt::QueuedConnection));
  for (int i = 0; i < kBursts; ++i) second_producer.produce(i);
  for (int i = 0; i < kBursts; ++i) second_consumer.done.acquire();
  CHECK(second_consumer.calls == kBursts && consumer.calls == 1);
  thread.quit();
  thread.wait();

  // emitted on the worker thread, received on the main thread (queued by default across threads)
  Ticker ticker;
  Collector collector;
  CHECK(QObject::connect(&ticker, &Ticker::ticked, &collector, &Collector::collect));
  ticker.start();
  ticker.wait();
  QCoreApplication::processEvents();
  CHECK(collector.count == kTicks && collector.sum == kTicks * (kTicks + 1) / 2);
  return rqt_test::finish("capability_static_signal_queued");
}
