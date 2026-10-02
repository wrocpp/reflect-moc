// Static signals of a header class, used from two translation units of one executable: a connection
// made here hears a signal fired there, both see the same index, the signal is one object (one address)
// and one type in both.
#include "check.hpp"
#include "shared_static_signals.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>

#include <cstring>

namespace {
constexpr int kValue = 41;
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  SharedStatic object;

  int heard = 0;
  int done = 0;
  CHECK(QObject::connect(&object, &SharedStatic::changed, &object, [&](int v) { heard = v; }));
  CHECK(QObject::connect(&object, &SharedStatic::done, &object, [&] { ++done; }));

  second_unit_fire(&object, kValue);  // emitted from the other translation unit
  CHECK(heard == kValue && done == 1);
  object.fire(kValue + 1);  // and from this one
  CHECK(heard == kValue + 1 && done == 2);

  CHECK(QMetaMethod::fromSignal(&SharedStatic::changed).methodIndex() == second_unit_changed_index());
  CHECK(static_cast<void const*>(&SharedStatic::changed) == second_unit_changed_address());

  char const* const mine = typeid(SharedStatic::changed).name();
  CHECK(std::strcmp(mine, second_unit_signal_type_name()) == 0);
  CHECK(mine[0] != '*');  // GCC marks a type with internal linkage with a leading '*'
  return rqt_test::finish("capability_static_signal_two_tus");
}
