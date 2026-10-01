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

  // Is the member type the same in both translation units? The closure in rqt::signal's default
  // template argument has internal linkage, so formally it is not (an ODR violation that GCC reports
  // as -Wsubobject-linkage for a header class). Everything above works anyway; this line records what
  // this compiler does, and the README states it.
  char const* const mine = typeid(object.changed).name();
  char const* const theirs = second_unit_signal_type_name();
  std::printf("signal member type identical across translation units: %s (%s | %s)\n",
              std::strcmp(mine, theirs) == 0 ? "yes" : "no", mine, theirs);
  return rqt_test::finish("capability_data_signals_two_tus");
}
