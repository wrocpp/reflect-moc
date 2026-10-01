// Limit: explicit property indexes must be 0, 1, 2 ... with no gap or duplicate.
// This file must NOT compile with
//   uncaught exception of type 'std::meta::exception'; 'what()': 'rqt::property .index: in class Gapped property 'late' has index 2 but the indexes must be 0, 1, 2 ... with no gap or duplicate; expected 1'
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Gapped : rqt::Object<> {
  Gapped() { bind(); }
  [[= rqt::property{.index = 0}]] int early() const { return 0; }
  [[= rqt::property{.index = 2}]] int late() const { return 2; }
};
