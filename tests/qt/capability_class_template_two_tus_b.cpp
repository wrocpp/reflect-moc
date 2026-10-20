#include "two_tus_template.hpp"

#include <QtCore/QMetaMethod>

namespace two_tus {
void insert_from_second_unit(Container* container, unsigned key, int value) { container->insert(key, Sample{value}); }

const QMetaObject* container_meta_in_second_unit() { return &Container::staticMetaObject; }

const QMetaObject* model_meta_in_second_unit() { return &Model::staticMetaObject; }

int added_index_in_second_unit() { return QMetaMethod::fromSignal(&Container::entryAdded).methodIndex(); }

char const* signal_type_name_in_second_unit() {
  static Container* const none = nullptr;
  return typeid(none->entryAdded).name();
}
}  // namespace two_tus
