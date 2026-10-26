// Limit: an enum in an inline namespace belongs to that namespace, not to the one around it, so it is not in the outer
// meta-object; the build says so instead of dropping it. This file must NOT compile with
//   an enum is registered in a namespace nested in this one (an inline namespace counts) that has no RQT_NAMESPACE of its own
#include <reflect_moc/qt/qt.hpp>

namespace shapes {
inline namespace v1 {
enum class [[= rqt::enum_]] Mode { off, on };
}
RQT_NAMESPACE
}  // namespace shapes

QMetaObject const* meta = &shapes::staticMetaObject;
