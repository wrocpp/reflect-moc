#pragma once
#include "namespace_meta_experiment.hpp"
namespace Outer::N {
enum class [[= rqt::enum_]] E { a, b };
using nsx::qt_getEnumMetaObject;
using nsx::qt_getEnumName;
inline QMetaObject const& staticMetaObject = nsx::ns_meta<>;
inline bool const rqt_verified = nsx::verify<>();
}
