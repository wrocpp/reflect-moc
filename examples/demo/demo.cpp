// reflect-moc demo: Qt classes with NO moc, driven from a small command line.
//
//   examples/demo/run.sh           interactive (type `help`)
//   examples/demo/run.sh --tour    a scripted tour, no typing
//
// Every class below is plain C++26. RQT_OBJECT stands in for Q_OBJECT, RQT_PROPERTY takes
// the exact Q_PROPERTY text, signals are bodyless members, slots carry an annotation.
// Qt sees an ordinary QMetaObject, so connect, invokeMethod and QMetaProperty all work.
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QVariant>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int kWorkerStepMs = 120;
constexpr int kPercentMax = 100;

std::string str(QString const& s) { return s.toStdString(); }

char const* threadName() { return QThread::currentThread() == QCoreApplication::instance()->thread() ? "main" : "worker"; }
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

class Logger : public QObject {
  RQT_OBJECT

 public:
  explicit Logger(QObject* parent = nullptr) : QObject(parent) {}

 public slots:
  // reached through the string-based SIGNAL()/SLOT() connect
  [[= rqt::slot]] void onLabel(QString l) { std::cout << "  [logger slot, string connect] label is now \"" << str(l) << "\"\n"; }
};

class Worker : public QObject {
  RQT_OBJECT

 public:
  explicit Worker(QObject* parent = nullptr) : QObject(parent) {}

 public slots:
  [[= rqt::slot]] void run(int steps) {
    std::cout << "  [worker thread] running " << steps << " steps\n";
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

static void dumpMeta(QMetaObject const* mo) {
  std::cout << "class " << mo->className() << " : " << (mo->superClass() ? mo->superClass()->className() : "-")
            << "   (a real QMetaObject, no moc)\n";
  for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
    QMetaMethod const m = mo->method(i);
    char const* kind = m.methodType() == QMetaMethod::Signal ? "signal" : m.methodType() == QMetaMethod::Slot ? "slot  " : "method";
    std::string names;
    for (QByteArray const& n : m.parameterNames()) names += (names.empty() ? "" : ", ") + n.toStdString();
    std::cout << "  " << kind << "  " << m.methodSignature().constData() << "   param names: [" << names << "]\n";
  }
  for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
    QMetaProperty const p = mo->property(i);
    std::cout << "  property  " << p.typeName() << " " << p.name() << (p.isWritable() ? "  writable" : "  read-only")
              << (p.hasNotifySignal() ? "  notify=" + std::string(p.notifySignal().name().constData()) : std::string{}) << "\n";
  }
}

struct Demo {
  Counter counter;
  Logger logger;
  Worker worker;
  QThread thread;

  Demo() {
    // stock Qt connections: lambda, string-based, and queued across a thread
    QObject::connect(&counter, &Counter::valueChanged, &counter, [](int v) { std::cout << "  [signal] valueChanged(" << v << ")\n"; });
    QObject::connect(&counter, SIGNAL(labelChanged(QString)), &logger, SLOT(onLabel(QString)));
    QObject::connect(&worker, &Worker::progress, QCoreApplication::instance(), [](int p, QString t) {
      std::cout << "  [" << threadName() << " thread] progress " << p << "%  " << str(t) << "\n";
    });
    worker.moveToThread(&thread);
    thread.start();
  }
  ~Demo() {
    thread.quit();
    thread.wait();
  }

  QObject* byName(std::string const& n) {
    if (n == "Counter" || n == "counter") return &counter;
    if (n == "Worker" || n == "worker") return &worker;
    if (n == "Logger" || n == "logger") return &logger;
    return nullptr;
  }

  static void help() {
    std::cout << "commands:\n"
                 "  meta [Counter|Worker|Logger]   dump the QMetaObject Qt sees (signals, slots, properties, parameter names)\n"
                 "  get <property>                 read a Counter property through QMetaProperty\n"
                 "  set <property> <value>         write it (emits the NOTIFY signal)\n"
                 "  slot <name> [arg]              call a Counter slot through QMetaObject::invokeMethod\n"
                 "  invoke add <n>                 call the invokable that returns a value\n"
                 "  run <steps>                    run the Worker on its own thread; progress arrives queued on main\n"
                 "  help | quit\n";
  }

  bool command(std::string const& line) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    if (cmd.empty()) return true;
    if (cmd == "quit" || cmd == "exit") return false;
    if (cmd == "help") { help(); return true; }
    if (cmd == "meta") {
      std::string who;
      in >> who;
      if (who.empty()) {
        dumpMeta(counter.metaObject());
        dumpMeta(logger.metaObject());
        dumpMeta(worker.metaObject());
      } else if (QObject* o = byName(who)) {
        dumpMeta(o->metaObject());
      } else {
        std::cout << "  unknown class\n";
      }
      return true;
    }
    if (cmd == "get" || cmd == "set") {
      std::string name, value;
      in >> name >> value;
      QMetaObject const* mo = counter.metaObject();
      int const idx = mo->indexOfProperty(name.c_str());
      if (idx < 0) { std::cout << "  no such property\n"; return true; }
      QMetaProperty p = mo->property(idx);
      if (cmd == "get") {
        std::cout << "  " << name << " = " << str(p.read(&counter).toString()) << "\n";
      } else {
        bool const ok = p.write(&counter, QString::fromStdString(value));
        std::cout << (ok ? "  written\n" : "  write failed\n");
      }
      return true;
    }
    if (cmd == "slot" || cmd == "invoke") {
      std::string name, arg;
      in >> name >> arg;
      QMetaObject const* mo = counter.metaObject();
      for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        QMetaMethod m = mo->method(i);
        if (m.name() != name.c_str() || m.methodType() == QMetaMethod::Signal) continue;
        int ret = 0;
        bool ok = false;
        if (m.parameterCount() == 0) {
          ok = m.invoke(&counter, Qt::DirectConnection);
        } else if (m.parameterType(0) == QMetaType::Int) {
          int const v = std::stoi(arg.empty() ? "0" : arg);
          ok = m.returnType() == QMetaType::Int ? m.invoke(&counter, Qt::DirectConnection, Q_RETURN_ARG(int, ret), Q_ARG(int, v))
                                                 : m.invoke(&counter, Qt::DirectConnection, Q_ARG(int, v));
        } else if (m.parameterType(0) == QMetaType::QString) {
          ok = m.invoke(&counter, Qt::DirectConnection, Q_ARG(QString, QString::fromStdString(arg)));
        }
        std::cout << (ok ? "  invoked " : "  invoke failed: ") << m.methodSignature().constData();
        if (m.returnType() == QMetaType::Int) std::cout << " -> " << ret;
        std::cout << "\n";
        return true;
      }
      std::cout << "  no such slot\n";
      return true;
    }
    if (cmd == "run") {
      int steps = 3;
      in >> steps;
      QEventLoop loop;
      QObject::connect(&worker, &Worker::finished, &loop, &QEventLoop::quit);
      QMetaObject::invokeMethod(&worker, "run", Qt::QueuedConnection, Q_ARG(int, steps));
      loop.exec();
      std::cout << "  [" << threadName() << " thread] worker finished\n";
      return true;
    }
    std::cout << "  unknown command (try `help`)\n";
    return true;
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Demo demo;
  bool const tour = argc > 1 && std::string(argv[1]) == "--tour";

  std::cout << "reflect-moc demo. Qt classes built with NO moc. Type `help`.\n";
  if (tour) {
    for (char const* line : {"meta Counter", "get value", "set value 5", "set label hello", "slot setValue 7", "invoke add 3",
                             "get value", "meta Worker", "run 4", "quit"}) {
      std::cout << "> " << line << "\n";
      if (!demo.command(line)) break;
    }
    return 0;
  }
  std::string line;
  std::cout << "> " << std::flush;
  while (std::getline(std::cin, line)) {
    if (!demo.command(line)) break;
    std::cout << "> " << std::flush;
  }
  return 0;
}
