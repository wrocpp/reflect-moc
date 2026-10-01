// The oracle test: the same classes built once by the real moc and once by reflect-moc must
// produce QMetaObjects that agree on everything Qt exposes: class names, superclass, every
// method (signature, access, type, attributes, return and parameter types, names, tag), every
// property (type, flags, notify signal), every enum and flag, and the class info.
#include "check.hpp"
#include "differential_dump.hpp"
#include "differential_moc.h"
#include "differential_rqt.hpp"

#include <QtCore/QCoreApplication>

#include <algorithm>

namespace {

void compare(char const* what, QMetaObject const* oracle, QMetaObject const* ours) {
  QStringList const expected = dump_class(oracle);
  QStringList const actual = dump_class(ours);
  bool same = expected == actual;
  if (!same) {
    std::fprintf(stderr, "--- %s differs ---\n", what);
    for (qsizetype i = 0; i < std::max(expected.size(), actual.size()); ++i) {
      QString const e = i < expected.size() ? expected[i] : QString{"<missing>"};
      QString const a = i < actual.size() ? actual[i] : QString{"<missing>"};
      if (e != a) std::fprintf(stderr, "moc : %s\nrqt : %s\n", e.toUtf8().constData(), a.toUtf8().constData());
    }
  }
  rqt_test::report(same, what, __FILE__, __LINE__);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::Base moc_base;
  viarqt::Base rqt_base;
  viamoc::Derived moc_derived;
  viarqt::Derived rqt_derived;

  compare("Plugin", viamoc::Plugin{}.metaObject(), viarqt::Plugin{}.metaObject());

  // the interface base: both answer the interface id with the interface subobject
  viamoc::Plugin moc_plugin;
  viarqt::Plugin rqt_plugin;
  CHECK(moc_plugin.qt_metacast("org.rqt.diff.Greeter/1.0") ==
        static_cast<void*>(static_cast<DiffGreeter*>(&moc_plugin)));
  CHECK(rqt_plugin.qt_metacast("org.rqt.diff.Greeter/1.0") ==
        static_cast<void*>(static_cast<DiffGreeter*>(&rqt_plugin)));
  CHECK(moc_plugin.qt_metacast("org.rqt.diff.Nobody/1.0") == nullptr);
  CHECK(rqt_plugin.qt_metacast("org.rqt.diff.Nobody/1.0") == nullptr);

  compare("Base", moc_base.metaObject(), rqt_base.metaObject());
  compare("Derived", moc_derived.metaObject(), rqt_derived.metaObject());

  // the chain and the totals Qt computes from it
  QMetaObject const* moc_mo = moc_derived.metaObject();
  QMetaObject const* rqt_mo = rqt_derived.metaObject();
  CHECK(moc_mo->methodCount() == rqt_mo->methodCount());
  CHECK(moc_mo->propertyCount() == rqt_mo->propertyCount());
  CHECK(moc_mo->enumeratorCount() == rqt_mo->enumeratorCount());
  CHECK(moc_mo->classInfoCount() == rqt_mo->classInfoCount());
  CHECK(moc_mo->superClass()->superClass() == &QObject::staticMetaObject);
  CHECK(rqt_mo->superClass()->superClass() == &QObject::staticMetaObject);
  return rqt_test::finish("capability_differential_against_moc");
}
