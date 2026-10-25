// BINDABLE properties: the meta-object equals moc's (the B flag of the property row), and a binding set through
// QMetaProperty::bindable() behaves the same: the property follows its source, a signal fires through a function-form
// NOTIFY, setProperty removes the binding, and a read-only bindable stays read-only. Q_OBJECT_BINDABLE_PROPERTY works in a
// class with RQT_OBJECT and in an ObjectOf class when NOTIFY names a function-form signal (z). docs/moc-parity.md: C-07, C-08.
#include "parity.hpp"
#include "parity_bindable_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>
#include <QtCore/QProperty>

#include <vector>

namespace viarqt {

class Meter : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int x READ x WRITE setX NOTIFY xChanged BINDABLE bindableX)
  RQT_PROPERTY(int y READ y BINDABLE bindableY)
  RQT_PROPERTY(int z READ z WRITE setZ NOTIFY zChanged BINDABLE bindableZ)

 public:
  explicit Meter(QObject* parent = nullptr) : QObject(parent) {}
  int x() const { return x_.value(); }
  void setX(int v) {
    if (x_.value() == v) return;
    x_ = v;
    emit xChanged(v);
  }
  QBindable<int> bindableX() { return QBindable<int>(&x_); }
  int y() const { return y_.value(); }
  QBindable<int> bindableY() const { return QBindable<int>(&y_); }
  int z() const { return z_.value(); }
  void setZ(int v) { z_ = v; }
  QBindable<int> bindableZ() { return QBindable<int>(&z_); }

 signals:
  [[= rqt::signal_function]] void xChanged(int x) { rqt::activate{this}(x); }
  [[= rqt::signal_function]] void zChanged() { rqt::activate{this}(); }

 private:
  QProperty<int> x_{1};
  QProperty<int> y_{2};
  Q_OBJECT_BINDABLE_PROPERTY(Meter, int, z_, &Meter::zChanged)
};

// The same with no macro: annotations, and a private bindable getter.
class FreeMeter : public rqt::ObjectOf<> {
 public:
  explicit FreeMeter(QObject* parent = nullptr) : ObjectOf(parent) {}
  [[= rqt::property{.write = "setX", .notify = "xChanged", .bindable = "bindableX"}]] int x() const { return x_.value(); }
  void setX(int v) {
    if (x_.value() == v) return;
    x_ = v;
    xChanged(v);
  }
  [[= rqt::property{.bindable = "bindableY"}]] int y() const { return y_.value(); }
  [[= rqt::property{.write = "setZ", .notify = "zChanged", .bindable = "bindableZ"}]] int z() const { return z_.value(); }
  void setZ(int v) { z_ = v; }

  [[= rqt::signal_function]] void xChanged(int x) { rqt::activate{this}(x); }
  [[= rqt::signal_function]] void zChanged() { rqt::activate{this}(); }

 private:
  QBindable<int> bindableX() { return QBindable<int>(&x_); }
  QBindable<int> bindableY() const { return QBindable<int>(&y_); }
  QBindable<int> bindableZ() { return QBindable<int>(&z_); }

  QProperty<int> x_{1};
  QProperty<int> y_{2};
  Q_OBJECT_BINDABLE_PROPERTY(FreeMeter, int, z_, &FreeMeter::zChanged)
};

}  // namespace viarqt

namespace {

constexpr int source_start = 5;
constexpr int source_next = 9;
constexpr int written = 3;
constexpr int source_after_write = 100;
constexpr int z_source_start = 7;
constexpr int z_source_next = 8;

// What a user of the meta-object sees, in order, so the two classes compare by value.
template <class T>
std::vector<int> trace() {
  T object;
  QMetaObject const* meta = object.metaObject();
  QMetaProperty const px = meta->property(meta->indexOfProperty("x"));
  QMetaProperty const py = meta->property(meta->indexOfProperty("y"));
  QMetaProperty const pz = meta->property(meta->indexOfProperty("z"));
  std::vector<int> seen;
  auto const note = [&](int v) { seen.push_back(v); };

  note(px.isBindable() && py.isBindable() && pz.isBindable());
  QUntypedBindable bx = px.bindable(&object);
  QUntypedBindable by = py.bindable(&object);
  note(bx.isValid() && by.isValid());
  note(by.isReadOnly());
  note(bx.isReadOnly());
  note(py.read(&object).toInt());

  int x_signals = 0;
  QObject::connect(&object, &T::xChanged, [&](int) { ++x_signals; });
  QProperty<int> source{source_start};
  bx.setBinding(Qt::makePropertyBinding(source));
  note(bx.hasBinding());
  note(object.x());
  source = source_next;
  note(px.read(&object).toInt());
  note(x_signals);  // QProperty does not notify by itself: the manual NOTIFY fires only from the setter

  note(object.setProperty("x", written));
  note(bx.hasBinding());
  source = source_after_write;
  note(object.x());
  note(x_signals);

  int z_signals = 0;
  QObject::connect(&object, &T::zChanged, [&] { ++z_signals; });
  QProperty<int> z_source{z_source_start};
  QUntypedBindable bz = pz.bindable(&object);
  bz.setBinding(Qt::makePropertyBinding(z_source));
  note(object.z());
  z_source = z_source_next;
  note(object.z());
  note(z_signals);  // Q_OBJECT_BINDABLE_PROPERTY notifies through the function-form signal
  note(object.setProperty("z", written));
  note(bz.hasBinding());
  note(z_signals);
  return seen;
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::Meter moc_meter;
  viarqt::Meter rqt_meter;
  PARITY_SAME("Meter", moc_meter.metaObject(), rqt_meter.metaObject());
  CHECK(parity::same_totals(moc_meter.metaObject(), rqt_meter.metaObject()));

  std::vector<int> const oracle = trace<viamoc::Meter>();
  std::vector<int> const ours = trace<viarqt::Meter>();
  CHECK(oracle == ours);
  // the oracle itself must show the behaviour, or two empty traces would agree
  std::vector<int> const expected{1, 1, 1, 0, 2, 1, source_start, source_next, 0, 1, 0, written, 1, z_source_start, z_source_next, 2, 1, 0, 3};
  CHECK(oracle == expected);

  viarqt::FreeMeter free_meter;
  CHECK(trace<viarqt::FreeMeter>() == oracle);
  CHECK(free_meter.metaObject()->property(free_meter.metaObject()->indexOfProperty("y")).isBindable());
  return rqt_test::finish("parity_bindable");
}
