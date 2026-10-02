// Limit: calling a static signal only builds the emission. A bare `s(1);` does nothing, so the result
// is [[nodiscard]] and this file must NOT compile (-Werror=unused-result) with
//   ignoring returned value of type 'rqt::pending<...>', declared with attribute 'nodiscard':
//   'a signal call only builds the emission: write `emit sig(args);` or `sig(args).from(obj);`'
#include <QtCore/QObject>

#include <reflect_moc/compat.hpp>

class Forgetful : public QObject {
  Q_OBJECT

 public:
  void fire() { valueChanged(1); }  // no `emit`

 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
};
