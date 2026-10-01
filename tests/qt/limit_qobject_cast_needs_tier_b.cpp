// Limit: qobject_cast<T*> needs the tier B opt-in. HasQ_OBJECT_Macro is not
// specialized for a class without its own staticMetaObject, because T::staticMetaObject
// would silently find QObject's. This file must NOT compile with
//   static assertion failed: qobject_cast requires the type to have a Q_OBJECT macro
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Bare : rqt::Object<> {
  Bare() { bind(); }
};

Bare* cast_without_opt_in(QObject* o) { return qobject_cast<Bare*>(o); }
