// QPrivateSignal: the last parameter of a signal that only the class can emit, because only the class can make the tag.
// moc leaves it out of the signature (methodSignature has no QPrivateSignal), the parameter count and the parameter
// list, and passes `QPrivateSignal()` when it calls the signal itself. The meta-object must equal moc's in both signal
// forms (a data member and a function with a body), and connecting by name or by pointer to member must work.
// docs/moc-parity.md: B-11.
#include "parity.hpp"
#include "parity_private_signal_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>

namespace viarqt {

class Sealed : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value NOTIFY valueChanged)

 public:
  explicit Sealed(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  void set(int v) {
    value_ = v;
    emit valueChanged(v, QPrivateSignal());
    emit tick(QPrivateSignal());
    emit pair(v, v + 1, QPrivateSignal());
  }

 signals:
  [[= rqt::names("v")]] rqt::signal<void(int, QPrivateSignal)> valueChanged;
  [[= rqt::signal_function]] void tick(QPrivateSignal) { rqt::activate{this}(); }
  [[= rqt::signal_function]] void pair(int a, int b, QPrivateSignal) { rqt::activate{this}(a, b); }
  [[= rqt::names("plain")]] rqt::signal<void(int)> open;

 private:
  int value_ = 0;
};

}  // namespace viarqt

template <class T>
void behaviour(char const* what) {
  T object;
  int seen = 0, ticks = 0, sum = 0, string_seen = 0, property_seen = 0;
  QObject::connect(&object, &T::valueChanged, [&](int v) { seen = v; });
  QObject::connect(&object, &T::tick, [&] { ++ticks; });
  QObject::connect(&object, &T::pair, [&](int a, int b) { sum = a + b; });
  QObject::connect(&object, SIGNAL(valueChanged(int)), &object, SLOT(deleteLater()), Qt::DirectConnection);
  QObject::disconnect(&object, SIGNAL(valueChanged(int)), &object, SLOT(deleteLater()));
  struct Receiver : QObject {
    int* into;
    explicit Receiver(int* i) : into(i) {}
    void take(int v) { *into = v; }
  } receiver{&string_seen};
  QObject::connect(&object, &T::valueChanged, &receiver, [&](int v) { property_seen = v; });
  object.set(5);
  rqt_test::report(seen == 5 && ticks == 1 && sum == 5 + 6 && property_seen == 5, what, __FILE__, __LINE__);
  QMetaObject const* mo = object.metaObject();
  rqt_test::report(mo->indexOfSignal("valueChanged(int)") >= 0 && mo->indexOfSignal("tick()") >= 0 &&
                       mo->indexOfSignal("pair(int,int)") >= 0 && mo->indexOfSignal("valueChanged(int,QPrivateSignal)") < 0,
                   what, __FILE__, __LINE__);
  QMetaProperty const p = mo->property(mo->indexOfProperty("value"));
  rqt_test::report(p.notifySignal().methodSignature() == QByteArray{"valueChanged(int)"}, what, __FILE__, __LINE__);
  // a call by name reaches the signal through the meta-object, which supplies the tag itself
  int by_name = 0;
  QObject::connect(&object, &T::pair, [&](int a, int) { by_name = a; });
  rqt_test::report(QMetaObject::invokeMethod(&object, "pair", Q_ARG(int, 3), Q_ARG(int, 4)) && by_name == 3, what, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::Sealed moc_object;
  viarqt::Sealed rqt_object;
  PARITY_SAME("Sealed", moc_object.metaObject(), rqt_object.metaObject());
  CHECK(parity::same_totals(moc_object.metaObject(), rqt_object.metaObject()));
  behaviour<viamoc::Sealed>("moc behaviour");
  behaviour<viarqt::Sealed>("rqt behaviour");
  return rqt_test::finish("parity_private_signal");
}
