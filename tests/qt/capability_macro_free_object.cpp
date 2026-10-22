// A class with no macro at all: `rqt::ObjectOf<D, B>` supplies the meta-object, qt_metacast, qt_metacall and tr,
// and the owner for data-member signals. Checked for a QObject, for a reflected base and for a Qt class that is not
// a QObject itself, with stock Qt on the outside: qobject_cast, pointer-to-member connect, string connect,
// invokeMethod, QMetaProperty and tr.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>

namespace {
constexpr int kStart = 4;
constexpr int kStep = 3;
}  // namespace

class Counter : public rqt::ObjectOf<Counter> {
 public:
  explicit Counter(QObject* parent = nullptr) : rqt::ObjectOf<Counter>(parent) {}

  [[= rqt::property{.write = "setValue", .notify = "valueChanged"}]] int value() const { return value_; }
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    valueChanged(v);
  }
  [[= rqt::invokable]] int doubled() const { return 2 * value_; }

  rqt::signal<void(int)> valueChanged;
  [[= rqt::signal_function]] void finished() { rqt::activate{this}(); }

 private:
  int value_ = kStart;
};

class Scaled : public rqt::ObjectOf<Scaled, Counter> {
 public:
  explicit Scaled(QObject* parent = nullptr) : rqt::ObjectOf<Scaled, Counter>(parent) {}

  [[= rqt::slot]] void step() { setValue(value() + kStep); }
  rqt::signal<void(QString)> labelled;
  void label() { labelled(QString{"scaled"}); }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  Counter counter;
  QMetaObject const* mo = counter.metaObject();
  CHECK(QByteArray{mo->className()} == "Counter");
  CHECK(mo == &Counter::staticMetaObject);
  CHECK(mo->superClass() == &QObject::staticMetaObject);
  CHECK(mo->indexOfProperty("value") >= 0);
  CHECK(mo->indexOfSignal("valueChanged(int)") >= 0);
  CHECK(mo->indexOfSignal("finished()") >= 0);
  CHECK(mo->indexOfSlot("setValue(int)") >= 0);
  CHECK(qobject_cast<Counter*>(&counter) == &counter);
  QObject* as_object = &counter;
  CHECK(qobject_cast<Counter*>(as_object) == &counter);
  CHECK(as_object->inherits("Counter"));

  int heard = 0;
  int last = 0;
  QObject::connect(&counter, &Counter::valueChanged, &counter, [&](int v) {
    ++heard;
    last = v;
  });
  counter.setValue(kStart + 1);
  CHECK(heard == 1 && last == kStart + 1);
  CHECK(counter.setProperty("value", kStart + 2) && heard == 2 && last == kStart + 2);
  CHECK(counter.property("value").toInt() == kStart + 2);
  int doubled = 0;
  CHECK(QMetaObject::invokeMethod(&counter, "doubled", Q_RETURN_ARG(int, doubled)) && doubled == 2 * (kStart + 2));
  int finished = 0;
  QObject::connect(&counter, SIGNAL(finished()), &counter, SLOT(deleteLater()));  // a string connect resolves too
  QObject::connect(&counter, &Counter::finished, &counter, [&] { ++finished; });
  counter.finished();
  CHECK(finished == 1);
  CHECK(Counter::tr("text") == QString{"text"});

  // A reflected class as the base: indexes continue after the base's, the chain answers qobject_cast both ways.
  Scaled scaled;
  QMetaObject const* smo = scaled.metaObject();
  CHECK(QByteArray{smo->className()} == "Scaled");
  CHECK(smo->superClass() == &Counter::staticMetaObject);
  CHECK(smo->indexOfSignal("valueChanged(int)") >= 0 && smo->indexOfSlot("step()") >= smo->indexOfSlot("setValue(int)"));
  CHECK(qobject_cast<Counter*>(static_cast<QObject*>(&scaled)) == static_cast<Counter*>(&scaled));
  CHECK(qobject_cast<Scaled*>(static_cast<QObject*>(&scaled)) == &scaled);
  CHECK(qobject_cast<Scaled*>(static_cast<QObject*>(&counter)) == nullptr);
  int scaledHeard = 0;
  QObject::connect(&scaled, &Counter::valueChanged, &scaled, [&](int) { ++scaledHeard; });
  QString label;
  QObject::connect(&scaled, &Scaled::labelled, &scaled, [&](QString const& l) { label = l; });
  scaled.step();
  CHECK(scaledHeard == 1 && scaled.value() == kStart + kStep);
  scaled.label();
  CHECK(label == "scaled");
  CHECK(QMetaObject::invokeMethod(&scaled, "step") && scaledHeard == 2);
  return rqt_test::finish("capability_macro_free_object");
}
