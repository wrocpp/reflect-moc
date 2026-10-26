// A namespace with RQT_NAMESPACE in a header that two translation units include (ODR), with a class that has a property of
// the namespace's enum type.
#pragma once

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QMetaEnum>

namespace shared_ns {
enum class [[= rqt::enum_]] Mode { off, on };
RQT_NAMESPACE

class Panel : public rqt::ObjectOf<> {
 public:
  [[= rqt::property{}]] Mode mode = Mode::off;
};
}  // namespace shared_ns

QMetaObject const* second_unit_namespace_meta_object();
QMetaObject const* second_unit_panel_related_meta_object();
