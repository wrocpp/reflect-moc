#include "namespace_meta_experiment.hpp"
#include <cstdio>
namespace Outer::N {
enum class [[= rqt::enum_]] E { a, b };
using nsx::qt_getEnumMetaObject;
using nsx::qt_getEnumName;
inline QMetaObject const& staticMetaObject = nsx::ns_meta<>;
enum class [[= rqt::enum_]] Late { c };
}
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  auto e = QMetaEnum::fromType<Outer::N::Late>();
  std::printf("Late: valid=%d (ns lists %d enums)\n", e.isValid(), Outer::N::staticMetaObject.enumeratorCount());
}
