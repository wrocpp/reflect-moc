#include "shared_static_signals.hpp"

#include <QtCore/QMetaMethod>

void second_unit_fire(SharedStatic* object, int value) { object->fire(value); }

char const* second_unit_signal_type_name() { return typeid(SharedStatic::changed).name(); }

int second_unit_changed_index() { return QMetaMethod::fromSignal(&SharedStatic::changed).methodIndex(); }

void const* second_unit_changed_address() { return &SharedStatic::changed; }
