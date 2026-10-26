// Limit: an enum registered after RQT_NAMESPACE is missing from the namespace's meta-object (the line sees what is declared
// above it), and the build says so instead of dropping it. This file must NOT compile with
//   an enum annotated rqt::enum_ or rqt::flag is declared after RQT_NAMESPACE
#include <reflect_moc/qt/qt.hpp>

namespace shapes {
enum class [[= rqt::enum_]] Mode { off, on };
RQT_NAMESPACE
enum class [[= rqt::enum_]] Late { c };
}  // namespace shapes

QMetaObject const* meta = &shapes::staticMetaObject;
