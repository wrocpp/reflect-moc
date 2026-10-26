// real moc on twin.h; as in a real project, the moc output is included where the d-pointer class is complete
#include "twin.h"
#include "twin_impl_moc.h"
#include MOC_OUTPUT
#include "probe.hpp"
int main(int argc, char** argv) { QCoreApplication app(argc, argv); Widget w; probe(w); }
