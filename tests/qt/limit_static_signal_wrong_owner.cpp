// Limit: a static signal belongs to its class. Firing it from an object of an unrelated class must NOT
// compile, with
//   static assertion failed: rqt: this signal belongs to another class than the object that emits it
#include <QtCore/QObject>

#include <reflect_moc/compat.hpp>

class Owner : public QObject {
  Q_OBJECT

 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
};

class Stranger : public QObject {
  Q_OBJECT

 public:
  void fire() { emit Owner::valueChanged(1); }  // `this` is a Stranger, not an Owner
};
