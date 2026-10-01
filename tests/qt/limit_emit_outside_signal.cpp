// Limit: rqt::emit works only in the body of an [[=rqt::signal]] member
// function. This file must NOT compile with
//   uncaught exception of type 'std::meta::exception'; 'what()': 'rqt::emit used outside a [[=rqt::signal]] member function'
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Sender : rqt::Object<> {
  Sender() { bind(); }
  [[= rqt::slot]] void notSignal() { rqt::emit{this}(); }
};
