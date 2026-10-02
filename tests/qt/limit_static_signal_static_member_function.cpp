// Limit: `emit` hands over `this`, and a static member function has none. This file must NOT compile,
// with
//   'this' is unavailable for static member functions
// (write `valueChanged(1).from(object);` there).
#include <QtCore/QObject>

#include <reflect_moc/compat.hpp>

class Gauge : public QObject {
  Q_OBJECT

 public:
  static void fire() { emit valueChanged(1); }

 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
};
