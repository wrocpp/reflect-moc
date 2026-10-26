#include "shared_namespace.hpp"

QMetaObject const* second_unit_namespace_meta_object() { return &shared_ns::staticMetaObject; }

QMetaObject const* second_unit_panel_related_meta_object() {
  QMetaObject::SuperData const* related = shared_ns::Panel::staticMetaObject.d.relatedMetaObjects;
  return related ? related[0].direct : nullptr;
}
