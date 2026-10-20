// One specialization of the observable container and its model, used from two translation units of one
// executable. The class template is instantiated in both; the meta-object, the signal indexes and the signal
// member types must be the same in both (one definition), and a model built in one unit must hear a container
// changed in the other.
#pragma once

#include <container_model.hpp>

#include <QtCore/QHash>
#include <QtCore/QMetaObject>

#include <typeinfo>

namespace two_tus {
struct Sample {
  int value = 0;
};

using Container = observable::ObservableContainer<QHash<unsigned, Sample>>;
using Model = observable::ContainerModel<QHash<unsigned, Sample>>;

// defined in the second translation unit
void insert_from_second_unit(Container* container, unsigned key, int value);
const QMetaObject* container_meta_in_second_unit();
const QMetaObject* model_meta_in_second_unit();
int added_index_in_second_unit();
char const* signal_type_name_in_second_unit();
}  // namespace two_tus
