// One specialization of a class template with typed signals, instantiated in two translation units. The
// meta-object is one object, the signal index is the same in both, and a model made here follows a container
// changed from the other unit. Built with -flto -Wodr, so a second definition of anything would be reported.
#include "check.hpp"
#include "two_tus_template.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>

#include <cstring>

namespace {
constexpr unsigned kFirstKey = 7;
constexpr unsigned kSecondKey = 9;
constexpr int kFirstValue = 11;
constexpr int kSecondValue = 22;
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  using namespace two_tus;

  Container container;
  Model model(&container, [](const unsigned&, const Sample& s) { return QVariant(s.value); });

  CHECK(&Container::staticMetaObject == container_meta_in_second_unit());
  CHECK(&Model::staticMetaObject == model_meta_in_second_unit());
  CHECK(QMetaMethod::fromSignal(&Container::entryAdded).methodIndex() == added_index_in_second_unit());

  char const* const mine = typeid(container.entryAdded).name();
  CHECK(std::strcmp(mine, signal_type_name_in_second_unit()) == 0);
  CHECK(mine[0] != '*');  // GCC marks a type with internal linkage with a leading '*'

  int added = 0;
  CHECK(QObject::connect(&container, &Container::entryAdded, &container, [&added](const unsigned&) { ++added; }));
  insert_from_second_unit(&container, kFirstKey, kFirstValue);  // changed from the other unit
  container.insert(kSecondKey, Sample{kSecondValue});           // and from this one
  CHECK(added == 2);
  CHECK(model.rowCount() == 2);
  CHECK(model.index(0).data().toInt() == kFirstValue);
  CHECK(model.index(1).data().toInt() == kSecondValue);
  return rqt_test::finish("capability_class_template_two_tus");
}
