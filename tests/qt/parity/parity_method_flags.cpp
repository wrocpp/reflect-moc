// Q_REVISION and Q_SCRIPTABLE on slots, invokables and signals: `[[=rqt::revision{1, 2}]]` and `[[=rqt::scriptable]]`.
// QMetaMethod::revision() and attributes() must equal moc's, also on the cloned rows of a default argument, and the
// annotated methods must still run. A signal data member carries the annotations too (it is a data member with
// attributes). docs/moc-parity.md: B-09, B-10.
#include "parity.hpp"
#include "parity_method_flags_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>

namespace viarqt {

class Flagged : public QObject {
  RQT_OBJECT

 public:
  explicit Flagged(QObject* parent = nullptr) : QObject(parent) {}
  [[= rqt::invokable, = rqt::scriptable]] void scripted() {}
  [[= rqt::invokable, = rqt::revision{3, 4}]] int versioned(int a) { return a + 1; }
  [[= rqt::invokable, = rqt::revision{5, 6}, = rqt::scriptable]] void both(int a, int b = 2) { (void)a; (void)b; }
  void fireData(int v) { emit dataSignal(v); }
  void fireFunction(int v) { emit functionSignal(v); }

 public slots:
  [[= rqt::slot, = rqt::revision{1, 2}, = rqt::scriptable]] void slot(int x, int y = 3) { last = x + y; }
  [[= rqt::slot, = rqt::scriptable]] void scriptedSlot() {}

 signals:
  [[= rqt::names("v"), = rqt::revision{4, 5}, = rqt::scriptable]] rqt::signal<void(int)> dataSignal;
  [[= rqt::signal_function, = rqt::revision{6, 7}]] void functionSignal(int v) { rqt::activate{this}(v); }
  [[= rqt::signal_function, = rqt::scriptable]] void scriptedSignal() { rqt::activate{this}(); }

 public:
  int last = 0;
};

}  // namespace viarqt

template <class T>
void behaviour(char const* what) {
  T object;
  int got = 0;
  QObject::connect(&object, SIGNAL(dataSignal(int)), &object, SLOT(slot(int)));
  object.fireData(4);
  rqt_test::report(object.last == 4 + 3, what, __FILE__, __LINE__);
  QObject::connect(&object, &T::functionSignal, [&](int v) { got = v; });
  object.fireFunction(9);
  int out = 0;
  rqt_test::report(got == 9 && QMetaObject::invokeMethod(&object, "versioned", Q_RETURN_ARG(int, out), Q_ARG(int, 1)) && out == 2,
                   what, __FILE__, __LINE__);
  rqt_test::report(QMetaObject::invokeMethod(&object, "slot", Q_ARG(int, 10)) && object.last == 13, what, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::Flagged moc_object;
  viarqt::Flagged rqt_object;
  QMetaObject const* moc = moc_object.metaObject();
  QMetaObject const* ours = rqt_object.metaObject();
  PARITY_SAME("Flagged", moc, ours);
  CHECK(parity::same_totals(moc, ours));
  // the rows are what the test is about: a revision and the scriptable attribute are really there
  QMetaMethod const slot = ours->method(ours->indexOfMethod("slot(int,int)"));
  CHECK(slot.revision() == (1 << 8 | 2));
  CHECK(slot.attributes() & QMetaMethod::Scriptable);
  CHECK(ours->method(ours->indexOfMethod("slot(int)")).revision() == (1 << 8 | 2));  // the clone keeps both
  CHECK(ours->method(ours->indexOfMethod("scriptedSignal()")).attributes() & QMetaMethod::Scriptable);
  behaviour<viamoc::Flagged>("moc behaviour");
  behaviour<viarqt::Flagged>("rqt behaviour");
  return rqt_test::finish("parity_method_flags");
}
