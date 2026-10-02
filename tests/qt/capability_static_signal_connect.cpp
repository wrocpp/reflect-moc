// rqt::static_signal: a signal that takes no space in the object, declared
//   static inline rqt::static_signal<void(int)> valueChanged{};
// Every way of connecting to it that Qt offers works unchanged: stock pointer-to-member connect to a
// slot and to a lambda, signal to signal, disconnect, string SIGNAL/SLOT, invokeMethod by name,
// QMetaMethod::fromSignal, two signals of one signature that stay two signals, and an inherited class.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QObject>

namespace {
constexpr int kFirst = 5;
constexpr int kSecond = 6;
constexpr int kThird = 7;
constexpr int kFrom = 2;
constexpr int kTo = 9;
constexpr int kNone = 0;
}  // namespace

class Sensor : public QObject {
  RQT_OBJECT

 public:
  explicit Sensor(QObject* parent = nullptr) : QObject(parent) {}
  void fire(int v) { valueChanged(v).from(this); }
  void fireOther(int v) { otherChanged(v).from(this); }
  void fireMoved(int from, int to) { moved(from, to).from(this); }

 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
  static inline rqt::static_signal<void(int)> otherChanged{};
  static inline rqt::static_signal<void(int, int)> moved{};
};

// Declares a signal of its own after the base's: its index starts after the base's signals.
class Gauge : public Sensor {
  RQT_OBJECT

 public:
  explicit Gauge(QObject* parent = nullptr) : Sensor(parent) {}
  void fireLevel(int v) { levelChanged(v).from(this); }

 signals:
  static inline rqt::static_signal<void(int)> levelChanged{};
};

class Probe : public QObject {
  RQT_OBJECT

 public:
  explicit Probe(QObject* parent = nullptr) : QObject(parent) {}
  int got = kNone;
  int calls = 0;

 public slots:
  [[= rqt::slot]] void onValue(int v) {
    got = v;
    ++calls;
  }
};

// The receiver of a signal-to-signal connect: its own static signal is the slot.
class Relay : public QObject {
  RQT_OBJECT

 public:
  explicit Relay(QObject* parent = nullptr) : QObject(parent) {}

 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Sensor sensor;
  Probe probe;

  // pointer to member, to a slot and to a lambda
  auto const to_slot = QObject::connect(&sensor, &Sensor::valueChanged, &probe, &Probe::onValue);
  CHECK(to_slot);
  int heard = kNone;
  CHECK(QObject::connect(&sensor, &Sensor::valueChanged, &sensor, [&](int v) { heard = v; }));
  sensor.fire(kFirst);
  CHECK(probe.got == kFirst && probe.calls == 1 && heard == kFirst);

  // two signals of one signature are two signals
  int other = kNone;
  CHECK(QObject::connect(&sensor, &Sensor::otherChanged, &sensor, [&](int v) { other = v; }));
  sensor.fireOther(kSecond);
  CHECK(other == kSecond && heard == kFirst && probe.calls == 1);

  // a source with more arguments than the slot
  int pair = kNone;
  CHECK(QObject::connect(&sensor, &Sensor::moved, &sensor, [&](int from, int to) { pair = from * 10 + to; }));
  sensor.fireMoved(kFrom, kTo);
  CHECK(pair == kFrom * 10 + kTo);

  // disconnect: the Connection handle, and by pointers
  CHECK(QObject::disconnect(to_slot));
  sensor.fire(kSecond);
  CHECK(probe.calls == 1);
  CHECK(QObject::connect(&sensor, &Sensor::valueChanged, &probe, &Probe::onValue));
  CHECK(QObject::disconnect(&sensor, &Sensor::valueChanged, &probe, &Probe::onValue));
  sensor.fire(kThird);
  CHECK(probe.calls == 1);

  // signal to signal: the receiver's static signal is the slot, and fires once per emission
  Relay relay;
  int relayed = kNone;
  int relay_count = 0;
  CHECK(QObject::connect(&sensor, &Sensor::valueChanged, &relay, &Relay::valueChanged));
  CHECK(QObject::connect(&relay, &Relay::valueChanged, &relay, [&](int v) {
    relayed = v;
    ++relay_count;
  }));
  sensor.fire(kFirst);
  CHECK(relayed == kFirst && relay_count == 1);

  // string SIGNAL/SLOT
  Probe stringly;
  CHECK(QObject::connect(&sensor, SIGNAL(valueChanged(int)), &stringly, SLOT(onValue(int))));
  sensor.fire(kSecond);
  CHECK(stringly.got == kSecond);

  // invokeMethod by name calls the signal, so its connections run
  Probe invoked;
  CHECK(QObject::connect(&sensor, &Sensor::valueChanged, &invoked, &Probe::onValue));
  CHECK(QMetaObject::invokeMethod(&sensor, "valueChanged", Q_ARG(int, kThird)));
  CHECK(invoked.got == kThird && invoked.calls == 1);

  // the method index Qt derives from the member pointer is the index of the signal by name
  QMetaObject const* const mo = sensor.metaObject();
  CHECK(QMetaMethod::fromSignal(&Sensor::valueChanged).methodIndex() == mo->indexOfSignal("valueChanged(int)"));
  CHECK(QMetaMethod::fromSignal(&Sensor::otherChanged).methodIndex() == mo->indexOfSignal("otherChanged(int)"));
  CHECK(QMetaMethod::fromSignal(&Sensor::moved).methodIndex() == mo->indexOfSignal("moved(int,int)"));
  CHECK(mo->indexOfSignal("valueChanged(int)") != mo->indexOfSignal("otherChanged(int)"));

  // an inherited class: its own signal and the base's both fire, each to its own connection
  Gauge gauge;
  int level = kNone;
  int base_value = kNone;
  CHECK(QObject::connect(&gauge, &Gauge::levelChanged, &gauge, [&](int v) { level = v; }));
  CHECK(QObject::connect(&gauge, &Gauge::valueChanged, &gauge, [&](int v) { base_value = v; }));
  gauge.fireLevel(kFirst);
  gauge.fire(kSecond);
  CHECK(level == kFirst && base_value == kSecond);
  CHECK(QMetaMethod::fromSignal(&Gauge::levelChanged).methodIndex() ==
        gauge.metaObject()->indexOfSignal("levelChanged(int)"));
  return rqt_test::finish("capability_static_signal_connect");
}
