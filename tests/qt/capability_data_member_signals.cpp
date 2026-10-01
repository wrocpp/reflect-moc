// Signals as bodyless data members: `rqt::signal<void(int)> levelChanged;`. Stock Qt
// works with nothing else declared: pointer-to-member, functor and string connect,
// queued connections across a QThread, QMetaMethod::fromSignal, QSignalSpy, a QML
// handler that reads its argument by name, a signal declared in a base class and
// two signals of one signature.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include <atomic>
#include <memory>

#ifdef RQT_HAVE_QTTEST
#include <QtTest/QSignalSpy>
#endif

namespace {
constexpr int kLevel = 7;
constexpr int kFrom = 2;
constexpr int kTo = 5;
constexpr int kOther = 11;
}  // namespace

class Base : public QObject {
  RQT_OBJECT

 public:
  explicit Base(QObject* parent = nullptr) : QObject(parent) {}
  void fireBase(int v) { emit baseChanged(v); }

 signals:
  [[= rqt::names("value")]] rqt::signal<void(int)> baseChanged;
};

class Sensor : public Base {
  RQT_OBJECT
  RQT_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged)

 public:
  explicit Sensor(QObject* parent = nullptr) : Base(parent) {}
  int level() const { return level_; }

 public slots:
  [[= rqt::slot]] void setLevel(int l) {
    if (l == level_) return;
    level_ = l;
    emit levelChanged(l);
  }
  void fireMoved(int from, int to) { emit moved(from, to); }
  void fireReset() { emit reset(); }
  void fireAlias(int v) { alias(v); }

 signals:
  [[= rqt::names("level")]] rqt::signal<void(int)> levelChanged;
  [[= rqt::names("from, to")]] rqt::signal<void(int, int)> moved;
  rqt::signal<void()> reset;
  [[= rqt::names("level")]] rqt::signal<void(int)> alias;  // the same signature as levelChanged

 private:
  int level_ = 0;
};

class Sink : public QObject {
  RQT_OBJECT

 public:
  explicit Sink(QObject* parent = nullptr) : QObject(parent) {}
  std::atomic<int> got{0};
  std::atomic<int> calls{0};
  std::atomic<QThread*> thread_seen{nullptr};
  QSemaphore done;

 public slots:
  [[= rqt::slot]] void onLevel(int v) {
    got = v;
    ++calls;
    thread_seen = QThread::currentThread();
    done.release();
  }
  [[= rqt::slot]] void onPair(int, int to) { got = to; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Sensor sensor;
  Sink sink;
  QMetaObject const* mo = sensor.metaObject();

  // the method table: signals first, in declaration order, with their parameter names
  int const first = mo->methodOffset();
  CHECK(mo->method(first + 0).methodType() == QMetaMethod::Signal);
  CHECK(mo->method(first + 0).methodSignature() == "levelChanged(int)");
  CHECK(mo->method(first + 0).parameterNames() == (QList<QByteArray>{"level"}));
  CHECK(mo->method(first + 1).methodSignature() == "moved(int,int)");
  CHECK(mo->method(first + 1).parameterNames() == (QList<QByteArray>{"from", "to"}));
  CHECK(mo->method(first + 2).methodSignature() == "reset()");
  CHECK(mo->method(first + 3).methodSignature() == "alias(int)");
  CHECK(mo->indexOfSlot("setLevel(int)") == first + 4);

  // stock Qt connect: pointer to member, lambda, functor, string
  CHECK(QObject::connect(&sensor, &Sensor::levelChanged, &sink, &Sink::onLevel));
  int seen = 0;
  CHECK(QObject::connect(&sensor, &Sensor::levelChanged, &sensor, [&](int v) { seen = v; }));
  CHECK(QObject::connect(&sensor, SIGNAL(moved(int,int)), &sink, SLOT(onPair(int,int))));
  sensor.setLevel(kLevel);
  CHECK(sink.got == kLevel && seen == kLevel);
  sensor.fireMoved(kFrom, kTo);
  CHECK(sink.got == kTo);

  // a signal with several arguments connected to a slot that takes a prefix
  CHECK(QObject::connect(&sensor, &Sensor::moved, &sink, &Sink::onLevel));
  int const before = sink.calls;
  sensor.fireMoved(kFrom, kTo);
  CHECK(sink.calls == before + 1 && sink.got == kFrom);

  // zero arguments
  int resets = 0;
  CHECK(QObject::connect(&sensor, &Sensor::reset, &sensor, [&] { ++resets; }));
  sensor.fireReset();
  CHECK(resets == 1);

  // two signals of the same signature are different signals
  Sink other_sink;
  CHECK(QObject::connect(&sensor, &Sensor::alias, &other_sink, &Sink::onLevel));
  sensor.fireAlias(kOther);
  CHECK(other_sink.got == kOther && other_sink.calls == 1);
  int const level_calls = sink.calls;
  sensor.fireAlias(kOther);
  CHECK(sink.calls == level_calls);  // levelChanged's receiver did not hear alias

  // QMetaMethod::fromSignal, indexOfSignal
  QMetaMethod const method = QMetaMethod::fromSignal(&Sensor::levelChanged);
  CHECK(method.isValid() && method.name() == "levelChanged" && method.methodType() == QMetaMethod::Signal);
  CHECK(QMetaMethod::fromSignal(&Sensor::alias).methodSignature() == "alias(int)");
  CHECK(QMetaMethod::fromSignal(&Sensor::alias).methodIndex() != method.methodIndex());

  // a signal declared in the base class, emitted from the derived object
  int base_seen = 0;
  CHECK(QObject::connect(&sensor, &Sensor::baseChanged, &sensor, [&](int v) { base_seen = v; }));
  sensor.fireBase(kOther);
  CHECK(base_seen == kOther);
  CHECK(QMetaMethod::fromSignal(&Base::baseChanged).methodSignature() == "baseChanged(int)");

  // the property's NOTIFY is a data-member signal
  CHECK(sensor.setProperty("level", kTo) && seen == kTo);
  CHECK(mo->property(mo->indexOfProperty("level")).notifySignal().name() == "levelChanged");

  // queued across a thread
  Sink threaded;
  QThread thread;
  threaded.moveToThread(&thread);
  thread.start();
  CHECK(QObject::connect(&sensor, SIGNAL(levelChanged(int)), &threaded, SLOT(onLevel(int)), Qt::QueuedConnection));
  sensor.setLevel(kOther + 1);
  threaded.done.acquire();
  CHECK(threaded.got == kOther + 1 && threaded.thread_seen == &thread);
  thread.quit();
  thread.wait();

#ifdef RQT_HAVE_QTTEST
  QSignalSpy spy(&sensor, &Sensor::levelChanged);
  sensor.setLevel(kLevel + 100);
  CHECK(spy.count() == 1 && spy.first().first().toInt() == kLevel + 100);
#endif

  // QML: a handler that reads its argument by name, and Connections
  QQmlEngine engine;
  engine.rootContext()->setContextProperty("sensor", &sensor);
  QQmlComponent component(&engine);
  component.setData(R"(
    import QtQml
    QtObject {
      id: root
      property int last: -1
      property int pair: -1
      property var watcher: Connections {
        target: sensor
        function onLevelChanged(level) { root.last = level }
        function onMoved(from, to) { root.pair = from * 100 + to }
      }
    }
  )",
                    QUrl{});
  std::unique_ptr<QObject> root{component.create()};
  if (!root) std::fprintf(stderr, "QML errors: %s\n", component.errorString().toUtf8().constData());
  CHECK(root);
  if (root) {
    sensor.setLevel(kLevel + 200);
    CHECK(root->property("last").toInt() == kLevel + 200);
    sensor.fireMoved(kFrom, kTo);
    CHECK(root->property("pair").toInt() == kFrom * 100 + kTo);
  }
  return rqt_test::finish("capability_data_member_signals");
}
