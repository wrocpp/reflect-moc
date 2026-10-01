// Data-member signals of a header class, used from two translation units of one executable:
// a connection made here hears a signal fired there, both see the same index, and the
// closure type of the signal member is the same type in both (reported by its mangled name).
#include "check.hpp"
#include "shared_signals.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>

#include <cstring>

namespace {
constexpr int kValue = 41;
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  SharedSignals object;

  int heard = 0;
  int done = 0;
  CHECK(QObject::connect(&object, &SharedSignals::changed, &object, [&](int v) { heard = v; }));
  CHECK(QObject::connect(&object, &SharedSignals::done, &object, [&] { ++done; }));

  second_unit_fire(&object, kValue);  // emitted from the other translation unit
  CHECK(heard == kValue && done == 1);
  object.fire(kValue + 1);  // and from this one
  CHECK(heard == kValue + 1 && done == 2);

  int const here = QMetaMethod::fromSignal(&SharedSignals::changed).methodIndex();
  CHECK(here == second_unit_changed_index());

  // the member type is one type with external linkage in both translation units (ODR), and the test
  // builds with -Werror and without -Wno-subobject-linkage
  char const* const mine = typeid(object.changed).name();
  char const* const theirs = second_unit_signal_type_name();
  CHECK(std::strcmp(mine, theirs) == 0);
  CHECK(mine[0] != '*');  // GCC marks a type with internal linkage with a leading '*'
  return rqt_test::finish("capability_data_signals_two_tus");
}
