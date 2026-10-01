// Limit: the one-line in-class staticMetaObject instantiates the meta-object
// while the class is incomplete, so the opt-in takes two lines. This file must
// NOT compile with
//   uncaught exception of type 'std::meta::exception'; 'what()': 'neither complete class type nor namespace'
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Gadget : rqt::Object<> {
  static constexpr QMetaObject const& staticMetaObject = rqt::static_meta_object<Gadget>;
  Gadget() { bind(); }
};
