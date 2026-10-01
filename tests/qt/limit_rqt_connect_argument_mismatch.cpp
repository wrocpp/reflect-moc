// Limit: index-based QMetaObject::connect does not check argument types, so
// rqt::connect does, at compile time. This file must NOT compile with
//   static assertion failed: rqt::connect: the slot's parameters must be a prefix of the signal's
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Job : rqt::Object<> {
  Job() { bind(); }
  [[= rqt::signal_function]] void finished() { rqt::emit{this}(); }
  [[= rqt::slot]] void setProgress(int) {}
};

void connect_mismatch(Job& a, Job& b) { rqt::connect(&a, &Job::finished, &b, &Job::setProgress); }
