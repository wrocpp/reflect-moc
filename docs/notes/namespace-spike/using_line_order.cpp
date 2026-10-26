#include "namespace_meta_experiment.hpp"
namespace Outer::N {
enum class [[= rqt::enum_]] E { a, b };
#ifdef USING_AFTER
inline QMetaObject const& staticMetaObject = nsx::ns_meta<>;
using nsx::qt_getEnumMetaObject;
using nsx::qt_getEnumName;
#else
using nsx::qt_getEnumMetaObject;
using nsx::qt_getEnumName;
inline QMetaObject const& staticMetaObject = nsx::ns_meta<>;
#endif
}
int main() { return QMetaEnum::fromType<Outer::N::E>().isValid() ? 0 : 1; }
