// A data-member signal as the receiver of a pointer-to-member connect: signal to signal, a source with
// more arguments than the target, disconnect, a Connection handle, exactly one emission, and a queued
// connection across a thread.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>

#include <atomic>

namespace {
constexpr int kValue = 6;
constexpr int kFrom = 2;
constexpr int kTo = 8;
}  // namespace

class Source : public QObject {
  RQT_OBJECT

 public:
  explicit Source(QObject* parent = nullptr) : QObject(parent) {}
  void fire(int v) { emit valueChanged(v); }
  void fireMoved(int from, int to) { emit moved(from, to); }

 signals:
  rqt::signal<void(int)> valueChanged;
  rqt::signal<void(int, int)> moved;
};

class Relay : public QObject {
  RQT_OBJECT

 public:
  explicit Relay(QObject* parent = nullptr) : QObject(parent) {}

 signals:
  rqt::signal<void(int)> valueChanged;
  rqt::signal<void(int)> other;
};

class Probe : public QObject {
  RQT_OBJECT

 public:
  explicit Probe(QObject* parent = nullptr) : QObject(parent) {}
  std::atomic<int> got{0};
  std::atomic<int> calls{0};
  std::atomic<QThread*> thread_seen{nullptr};
  QSemaphore done;

 public slots:
  [[= rqt::slot]] void onValue(int v) {
    got = v;
    ++calls;
    thread_seen = QThread::currentThread();
    done.release();
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Source source;
  Relay relay;
  Probe probe;

  // signal to signal, same signature: the relay's signal is emitted exactly once per source emission
  auto const link = QObject::connect(&source, &Source::valueChanged, &relay, &Relay::valueChanged);
  CHECK(link);
  CHECK(QObject::connect(&relay, &Relay::valueChanged, &probe, &Probe::onValue));
  source.fire(kValue);
  CHECK(probe.got == kValue && probe.calls == 1);

  // a source with more arguments than the target
  CHECK(QObject::connect(&source, &Source::moved, &relay, &Relay::other));
  CHECK(QObject::connect(&relay, &Relay::other, &probe, &Probe::onValue));
  source.fireMoved(kFrom, kTo);
  CHECK(probe.got == kFrom && probe.calls == 2);

  // disconnect through the Connection handle, and by sender and signal with any receiver. The pair form
  // disconnect(sender, &S::sig, receiver, &R::sigAsSlot) cannot match a data-member signal used as the
  // slot: Qt implements the Compare call only for pointers to member functions (a documented limit).
  CHECK(QObject::disconnect(link));
  source.fire(kValue + 1);
  CHECK(probe.calls == 2);
  CHECK(!QObject::disconnect(link));  // already gone
  QMetaObject::Connection const again = QObject::connect(&source, &Source::valueChanged, &relay, &Relay::valueChanged);
  CHECK(again);
  CHECK(!QObject::disconnect(&source, &Source::valueChanged, &relay, &Relay::valueChanged));
  CHECK(QObject::disconnect(&source, &Source::valueChanged, &relay, nullptr));
  source.fire(kValue + 2);
  CHECK(probe.calls == 2);

  // queued across a thread: signal to signal, then to a slot that runs on the worker thread
  Relay threaded_relay;
  Probe threaded_probe;
  QThread thread;
  threaded_probe.moveToThread(&thread);
  thread.start();
  CHECK(QObject::connect(&source, &Source::valueChanged, &threaded_relay, &Relay::valueChanged));
  CHECK(QObject::connect(&threaded_relay, &Relay::valueChanged, &threaded_probe, &Probe::onValue,
                         Qt::QueuedConnection));
  source.fire(kTo);
  threaded_probe.done.acquire();
  CHECK(threaded_probe.got == kTo && threaded_probe.thread_seen == &thread);
  thread.quit();
  thread.wait();
  return rqt_test::finish("capability_signal_to_signal_connect");
}
