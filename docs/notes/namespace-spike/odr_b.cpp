#include "odr_shared.hpp"
#include <cstdio>
QMetaObject const* address_a();
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  std::printf("same address: %d  enums=%d fromType valid=%d\n", address_a() == &Outer::N::staticMetaObject,
              Outer::N::staticMetaObject.enumeratorCount(), QMetaEnum::fromType<Outer::N::E>().isValid());
}
