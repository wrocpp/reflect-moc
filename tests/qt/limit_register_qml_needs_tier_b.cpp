// Limit: rqt::register_qml<T> goes through qmlRegisterType<T>, which reads
// T::staticMetaObject, so the class needs the tier B opt-in. This file must NOT
// compile with
//   static assertion failed: rqt::register_qml: the class needs the tier B opt-in
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Bare : rqt::Object<> {
  Bare() { bind(); }
};

int register_without_opt_in() { return rqt::register_qml<Bare>("rqt.limit", 1, 0, "Bare"); }
