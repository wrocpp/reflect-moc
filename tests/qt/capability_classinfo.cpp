// Q_CLASSINFO through class annotations.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>

namespace {
constexpr int kInfoCount = 2;
}  // namespace

struct [[= rqt::classinfo{"author", "Filip"}]] [[= rqt::classinfo{"version", "1.0"}]] Documented : rqt::Object<> {
  Documented() { bind(); }
};

struct Plain : rqt::Object<> {
  Plain() { bind(); }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Documented documented;
  Plain plain;
  QMetaObject const* mo = documented.metaObject();

  CHECK(mo->classInfoCount() - mo->classInfoOffset() == kInfoCount);
  int const author = mo->indexOfClassInfo("author");
  CHECK(author >= 0 && QByteArray{mo->classInfo(author).value()} == "Filip");
  int const version = mo->indexOfClassInfo("version");
  CHECK(version >= 0 && QByteArray{mo->classInfo(version).value()} == "1.0");
  CHECK(plain.metaObject()->indexOfClassInfo("author") < 0);
  return rqt_test::finish("capability_classinfo");
}
