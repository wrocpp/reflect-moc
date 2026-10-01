// The Qt-like syntax: RQT_OBJECT in place of Q_OBJECT, RQT_PROPERTY with the exact
// Q_PROPERTY text, a plain QObject base, no bind(), no opt-in, and Qt's keywords
// (emit, signals, slots) left on. qobject_cast, pointer-to-member connect and
// functor connect work with nothing else declared.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>

namespace {
constexpr int kFirst = 5;
constexpr int kSecond = 9;
constexpr int kStep = 3;
}  // namespace

class Counter : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged RESET reset FINAL)
  RQT_PROPERTY(QString label MEMBER label_ NOTIFY labelChanged)

 public:
  explicit Counter(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }

 public slots:
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    emit valueChanged(v);
  }
  [[= rqt::slot]] void reset() { setValue(0); }

 signals:
  [[= rqt::signal_function]] void valueChanged(int value) { rqt::activate{this}(value); }
  [[= rqt::signal_function]] void labelChanged(QString const& label) { rqt::activate{this}(label); }

 private:
  [[= rqt::slot]] void hidden() { ++hidden_calls_; }

  int value_ = 0;
  QString label_;
  int hidden_calls_ = 0;

 public:
  int hiddenCalls() const { return hidden_calls_; }
};

class Special : public Counter {
  RQT_OBJECT
  RQT_PROPERTY(int step READ step WRITE setStep)

 public:
  explicit Special(QObject* parent = nullptr) : Counter(parent) {}
  int step() const { return step_; }
  [[= rqt::slot]] void setStep(int s) { step_ = s; }

 private:
  int step_ = 0;
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Counter counter;
  Special special;

  // identity
  QMetaObject const* mo = special.metaObject();
  CHECK(QByteArray{mo->className()} == "Special");
  CHECK(QByteArray{mo->superClass()->className()} == "Counter");
  CHECK(mo->superClass()->superClass() == &QObject::staticMetaObject);
  CHECK(&Special::staticMetaObject == mo && &Counter::staticMetaObject != mo);
  CHECK(special.inherits("Counter") && special.inherits("QObject"));

  // qobject_cast, in every direction, with no declaration beyond RQT_OBJECT
  QObject* o = &special;
  CHECK(qobject_cast<Special*>(o) == &special);
  CHECK(qobject_cast<Counter*>(o) == &special);
  CHECK(qobject_cast<Special*>(static_cast<QObject*>(&counter)) == nullptr);
  CHECK(qobject_cast<Special*>(static_cast<Counter*>(&special)) == &special);

  // Qt's own connect: pointer to member, functor and strings
  int seen = 0;
  CHECK(QObject::connect(&counter, &Counter::valueChanged, &counter, [&](int v) { seen = v; }));
  Counter other;
  CHECK(QObject::connect(&counter, &Counter::valueChanged, &other, &Counter::setValue));
  CHECK(QObject::connect(&special, SIGNAL(valueChanged(int)), &other, SLOT(setValue(int))));
  counter.setValue(kFirst);
  CHECK(seen == kFirst && other.value() == kFirst);
  special.setValue(kSecond);
  CHECK(other.value() == kSecond);

  // properties, including the inherited one and the parsed flags
  QMetaProperty const value = mo->property(mo->indexOfProperty("value"));
  CHECK(value.isValid() && value.isFinal() && value.isResettable() && value.hasNotifySignal());
  CHECK(value.write(&special, kStep) && special.value() == kStep);
  CHECK(value.reset(&special) && special.value() == 0);
  QMetaProperty const label = mo->property(mo->indexOfProperty("label"));
  CHECK(label.isValid() && label.write(&special, QString{"warm"}) && label.read(&special).toString() == "warm");
  CHECK(special.setProperty("step", kStep) && special.step() == kStep);

  // a private slot, by name
  CHECK(QMetaObject::invokeMethod(&counter, "hidden") && counter.hiddenCalls() == 1);
  CHECK(QMetaObject::invokeMethod(&special, "setStep", Q_ARG(int, kSecond)) && special.step() == kSecond);
  return rqt_test::finish("capability_qt_like_syntax");
}
