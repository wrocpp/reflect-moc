#include "shared_signals.hpp"

#include <QtCore/QMetaMethod>

void second_unit_fire(SharedSignals* object, int value) { object->fire(value); }

char const* second_unit_signal_type_name() {
  static SharedSignals* const none = nullptr;
  return typeid(none->changed).name();
}

int second_unit_changed_index() { return QMetaMethod::fromSignal(&SharedSignals::changed).methodIndex(); }
