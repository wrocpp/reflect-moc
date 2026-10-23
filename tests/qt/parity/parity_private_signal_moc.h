// The oracle: signals that take QPrivateSignal as their last parameter, processed by the real moc.
#pragma once

#include <QtCore/QObject>

namespace viamoc {

class Sealed : public QObject {
  Q_OBJECT
  Q_PROPERTY(int value READ value NOTIFY valueChanged)

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
  void valueChanged(int v, QPrivateSignal);
  void tick(QPrivateSignal);
  void pair(int a, int b, QPrivateSignal);
  void open(int plain);

 private:
  int value_ = 0;
};

}  // namespace viamoc
