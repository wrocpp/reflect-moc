// Limit: Qt's pointer-to-member QObject::connect needs the tier B opt-in. This
// file must NOT compile with
//   static assertion failed: No Q_OBJECT in the class with the signal
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Bare : rqt::Object<> {
  Bare() { bind(); }
  [[= rqt::signal]] void changed() { rqt::emit{this}(); }
};

void connect_without_opt_in(Bare& a, Bare& b) { QObject::connect(&a, &Bare::changed, &b, [] {}); }
