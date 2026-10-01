#include "shared_widget.hpp"

namespace {
// runs during static initialization of this translation unit
char const* const name_at_startup = Shared::staticMetaObject.className();
}  // namespace

QMetaObject const* second_unit_meta_object() { return &Shared::staticMetaObject; }

Shared* second_unit_cast(QObject* o) { return qobject_cast<Shared*>(o); }

char const* meta_name_seen_by_static_initializer() { return name_at_startup; }
