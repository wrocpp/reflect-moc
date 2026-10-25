// The oracle: BINDABLE properties processed by the real moc. x is a QProperty with a manual NOTIFY, y a read-only bindable,
// z a Q_OBJECT_BINDABLE_PROPERTY that notifies through its function-form signal.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QProperty>

namespace viamoc {

class Meter : public QObject {
  Q_OBJECT
  Q_PROPERTY(int x READ x WRITE setX NOTIFY xChanged BINDABLE bindableX)
  Q_PROPERTY(int y READ y BINDABLE bindableY)
  Q_PROPERTY(int z READ z WRITE setZ NOTIFY zChanged BINDABLE bindableZ)

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
  void xChanged(int x);
  void zChanged();

 private:
  QProperty<int> x_{1};
  QProperty<int> y_{2};
  Q_OBJECT_BINDABLE_PROPERTY(Meter, int, z_, &Meter::zChanged)
};

}  // namespace viamoc
