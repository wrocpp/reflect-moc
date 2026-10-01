// An interface base needs no Q_INTERFACES line: RQT_OBJECT's qt_metacast answers the interface id
// of every direct base that has one (Q_DECLARE_INTERFACE), so qobject_cast<Interface*> works.
// Q_INTERFACES itself is an empty macro without moc, so existing source lines stay valid.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

class Greeter {
 public:
  virtual ~Greeter() = default;
  virtual QString greet() const = 0;
};
Q_DECLARE_INTERFACE(Greeter, "org.rqt.Greeter/1.0")

class Counter {
 public:
  virtual ~Counter() = default;
  virtual int count() const = 0;
};
Q_DECLARE_INTERFACE(Counter, "org.rqt.Counter/1.0")

class Plugin : public QObject, public Greeter, public Counter {
  RQT_OBJECT
  Q_INTERFACES(Greeter Counter)

 public:
  explicit Plugin(QObject* parent = nullptr) : QObject(parent) {}
  QString greet() const override { return "hello"; }
  int count() const override { return 3; }
};

class Sub : public Plugin {
  RQT_OBJECT

 public:
  explicit Sub(QObject* parent = nullptr) : Plugin(parent) {}
};

class Other : public QObject {
  RQT_OBJECT

 public:
  explicit Other(QObject* parent = nullptr) : QObject(parent) {}
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Plugin plugin;
  Sub sub;
  Other other;

  QObject* o = &plugin;
  Greeter* const greeter = qobject_cast<Greeter*>(o);
  CHECK(greeter != nullptr && greeter == static_cast<Greeter*>(&plugin) && greeter->greet() == "hello");
  Counter* const counter = qobject_cast<Counter*>(o);
  CHECK(counter != nullptr && counter == static_cast<Counter*>(&plugin) && counter->count() == 3);

  // the unrelated class has neither interface
  CHECK(qobject_cast<Greeter*>(static_cast<QObject*>(&other)) == nullptr);
  CHECK(qobject_cast<Counter*>(static_cast<QObject*>(&other)) == nullptr);

  // a class derived from the plugin keeps the interfaces through its base's qt_metacast
  CHECK(qobject_cast<Greeter*>(static_cast<QObject*>(&sub)) == static_cast<Greeter*>(&sub));
  CHECK(qobject_cast<Plugin*>(static_cast<QObject*>(&sub)) == &sub);

  // the interface id, by name, as moc's code answers it
  CHECK(plugin.qt_metacast("org.rqt.Greeter/1.0") == static_cast<void*>(static_cast<Greeter*>(&plugin)));
  CHECK(plugin.qt_metacast("org.nobody.Home/1.0") == nullptr);
  return rqt_test::finish("capability_interface_bases");
}
