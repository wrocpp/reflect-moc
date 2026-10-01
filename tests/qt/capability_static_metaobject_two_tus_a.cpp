// The class in a header included from two translation units of one executable
// (ODR): both see the same QMetaObject. A static initializer in the other TU
// that reads Shared::staticMetaObject finds it initialized, because that TU
// includes the class definition first.
#include "check.hpp"
#include "shared_widget.hpp"

#include <QtCore/QCoreApplication>

#include <cstring>

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Shared shared;

  CHECK(second_unit_meta_object() == &Shared::staticMetaObject);
  CHECK(second_unit_meta_object() == shared.metaObject());
  CHECK(second_unit_cast(&shared) == &shared);
  CHECK(qobject_cast<Shared*>(&shared) == &shared);
  CHECK(QMetaObject::invokeMethod(&shared, "poke") && shared.pokes == 1);

  char const* seen = meta_name_seen_by_static_initializer();
  CHECK(seen != nullptr && std::strcmp(seen, "Shared") == 0);
  return rqt_test::finish("capability_static_metaobject_two_tus");
}
