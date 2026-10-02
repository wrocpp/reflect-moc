// Limit: emitting changes the object (Qt's own signals are non-const members too), so a static signal
// cannot be fired from a const member function or through a const pointer. This file must NOT compile,
// with
//   static assertion failed: rqt: a signal is a non-const member: it cannot be emitted from a const
//   member function or through a const pointer
#include <QtCore/QObject>

#include <reflect_moc/compat.hpp>

class Gauge : public QObject {
  Q_OBJECT

 public:
  void fire() const { emit valueChanged(1); }  // `this` is a const Gauge*

 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
};
