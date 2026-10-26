#include "oracle_moc.h"
#include "probe_common.hpp"
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Outer::N::Holder h;
  probe<Outer::N::E>(Outer::N::staticMetaObject, Outer::N::Holder::staticMetaObject, h, "");
}
