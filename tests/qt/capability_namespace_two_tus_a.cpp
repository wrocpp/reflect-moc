// The namespace meta-object in a header included from two translation units of one executable (ODR): both see the same
// QMetaObject, QMetaEnum::fromType agrees, and the class's related meta-object is that one. Built with -flto -Wodr.
#include "check.hpp"
#include "shared_namespace.hpp"

#include <QtCore/QCoreApplication>

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  CHECK(second_unit_namespace_meta_object() == &shared_ns::staticMetaObject);
  CHECK(second_unit_panel_related_meta_object() == &shared_ns::staticMetaObject);
  CHECK(shared_ns::staticMetaObject.enumeratorCount() == 1);
  QMetaEnum const mode = QMetaEnum::fromType<shared_ns::Mode>();
  CHECK(mode.isValid() && mode.enclosingMetaObject() == &shared_ns::staticMetaObject);
  CHECK(QMetaType::fromType<shared_ns::Mode>().metaObject() == &shared_ns::staticMetaObject);
  return rqt_test::finish("capability_namespace_two_tus");
}
