// Limit: QMetaEnum::fromType<E>() for an enum declared in a macro-free class and registered only by [[=rqt::enum_]] stops
// the build in Qt's own static assertion. Qt needs friends qt_getEnumMetaObject(E) and qt_getEnumName(E), found by
// argument-dependent lookup, and a base class cannot declare friends of a derived class's enum; RQT_ENUM(E) supplies
// them. This file must NOT compile.
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QMetaEnum>

class Gauge : public rqt::ObjectOf<> {
 public:
  enum class [[= rqt::enum_]] Mode { off, on };
};

QMetaEnum mode_enum() { return QMetaEnum::fromType<Gauge::Mode>(); }
