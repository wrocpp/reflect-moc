#include "namespace_meta_experiment.hpp"
#include <cstdio>
namespace Outer::N { enum class [[= rqt::enum_]] E1 { a }; }
namespace Outer::N { enum class [[= rqt::enum_]] E2 { b }; }   // reopened before the line
namespace Outer::N {
using nsx::qt_getEnumMetaObject;
using nsx::qt_getEnumName;
inline QMetaObject const& staticMetaObject = nsx::ns_meta<>;
inline bool const rqt_verified = nsx::verify<>();
}
namespace Outer::M {   // a second namespace in the same TU, and an inline namespace inside it
enum class [[= rqt::enum_]] F { z };
using nsx::qt_getEnumMetaObject;
using nsx::qt_getEnumName;
inline QMetaObject const& staticMetaObject = nsx::ns_meta<>;
inline bool const rqt_verified = nsx::verify<>();
}
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  std::printf("N: %s %d enums; M: %s %d enums\n", Outer::N::staticMetaObject.className(), Outer::N::staticMetaObject.enumeratorCount(),
              Outer::M::staticMetaObject.className(), Outer::M::staticMetaObject.enumeratorCount());
  std::printf("E2 valid=%d F valid=%d\n", QMetaEnum::fromType<Outer::N::E2>().isValid(), QMetaEnum::fromType<Outer::M::F>().isValid());
}
