// reflect-moc QML demo: a Qt Quick window over moc-free C++ classes.
//   QT_QPA_PLATFORM=vnc ... rqt_demo_qml            interactive, over VNC (see run-qml.sh)
//   QT_QPA_PLATFORM=offscreen ... rqt_demo_qml --screenshot DIR    two PNGs, no display needed
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>

namespace {
constexpr int kWorkerStepMs = 150;
constexpr int kPercentMax = 100;
constexpr int kFirstShotMs = 1200;
constexpr int kSecondShotMs = 2600;
constexpr int kQuitMs = 2800;
constexpr int kDemoSteps = 6;
constexpr int kDemoAdd = 5;
}  // namespace

class Counter : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
  RQT_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)

 public:
  explicit Counter(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  QString label() const { return label_; }

 public slots:
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    emit valueChanged(v);
  }
  [[= rqt::slot]] void setLabel(QString l) {
    if (l == label_) return;
    label_ = l;
    emit labelChanged(l);
  }
  [[= rqt::slot]] void reset() {
    setValue(0);
    setLabel(QString());
  }
  [[= rqt::invokable]] int add(int n) {
    setValue(value_ + n);
    return value_;
  }

 signals:
  [[= rqt::names("value")]] rqt::signal<void(int)> valueChanged;
  [[= rqt::names("label")]] rqt::signal<void(QString)> labelChanged;

 private:
  int value_ = 0;
  QString label_;
};

class Worker : public QObject {
  RQT_OBJECT

 public:
  explicit Worker(QObject* parent = nullptr) : QObject(parent) {}

 public slots:
  [[= rqt::slot]] void run(int steps) {
    for (int i = 1; i <= steps; ++i) {
      QThread::msleep(kWorkerStepMs);
      emit progress(i * kPercentMax / steps, QString("step %1 of %2").arg(i).arg(steps));
    }
    emit finished(steps);
  }

 signals:
  [[= rqt::names("percent, text")]] rqt::signal<void(int, QString)> progress;
  [[= rqt::names("steps")]] rqt::signal<void(int)> finished;
};

class Controller : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(bool busy READ busy NOTIFY busyChanged)

 public:
  // QML must not bind to an object that lives on another thread, so the Controller (main
  // thread) relays the worker's signals. Each relay crosses threads as a queued connection.
  Controller(Counter* c, Worker* w, QObject* parent = nullptr) : QObject(parent), counter_(c), worker_(w) {
    QObject::connect(worker_, &Worker::progress, this, [this](int percent, QString text) { emit progress(percent, text); });
    QObject::connect(worker_, &Worker::finished, this, [this](int steps) {
      setBusy(false);
      emit finished(steps);
    });
  }
  bool busy() const { return busy_; }

  [[= rqt::invokable]] void start(int steps) {
    setBusy(true);
    QMetaObject::invokeMethod(worker_, "run", Qt::QueuedConnection, Q_ARG(int, steps));
  }

  // one line per class, read back from the QMetaObject Qt itself reports
  [[= rqt::invokable]] QString describe(QString cls) const {
    QObject* o = cls == "Worker" ? static_cast<QObject*>(worker_) : static_cast<QObject*>(counter_);
    QMetaObject const* mo = o->metaObject();
    QStringList parts;
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) parts << QString("property %1 %2").arg(mo->property(i).typeName(), mo->property(i).name());
    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
      QMetaMethod const m = mo->method(i);
      QString const kind = m.methodType() == QMetaMethod::Signal ? "signal" : m.methodType() == QMetaMethod::Slot ? "slot" : "method";
      parts << QString("%1 %2").arg(kind, QString::fromLatin1(m.methodSignature()));
    }
    return QString("%1: %2").arg(mo->className(), parts.join("  |  "));
  }

 signals:
  rqt::signal<void(bool)> busyChanged;
  [[= rqt::names("percent, text")]] rqt::signal<void(int, QString)> progress;
  [[= rqt::names("steps")]] rqt::signal<void(int)> finished;

 private:
  void setBusy(bool b) {
    if (b == busy_) return;
    busy_ = b;
    emit busyChanged(b);
  }
  Counter* counter_;
  Worker* worker_;
  bool busy_ = false;
};

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QString const qmlPath = qEnvironmentVariable("RQT_QML", "examples/demo/qml/main.qml");
  QString shotDir;
  for (int i = 1; i + 1 < argc; ++i)
    if (QString(argv[i]) == "--screenshot") shotDir = argv[i + 1];

  Counter counter;
  Worker worker;
  QThread thread;
  worker.moveToThread(&thread);
  thread.start();
  Controller controller(&counter, &worker);

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("counter", &counter);
  engine.rootContext()->setContextProperty("controller", &controller);
  engine.load(QUrl::fromLocalFile(QDir::current().absoluteFilePath(qmlPath)));
  if (engine.rootObjects().isEmpty()) return 1;

  if (!shotDir.isEmpty()) {
    auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    QDir().mkpath(shotDir);
    QTimer::singleShot(kFirstShotMs, [&] { win->grabWindow().save(shotDir + "/1-start.png"); });
    QTimer::singleShot(kFirstShotMs + 100, [&] {
      counter.add(kDemoAdd);
      counter.setLabel("hello from C++");
      controller.start(kDemoSteps);
    });
    QTimer::singleShot(kSecondShotMs, [&] { win->grabWindow().save(shotDir + "/2-running.png"); });
    QTimer::singleShot(kQuitMs, &app, &QCoreApplication::quit);
  }
  int const rc = app.exec();
  thread.quit();
  thread.wait();
  return rc;
}
