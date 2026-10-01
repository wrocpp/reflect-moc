// Tier A, no per-class line beyond bind(): string SIGNAL/SLOT connect on the
// real Qt 6.10, an inherited slot, and a signal with a return-less slot chain.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>

namespace {
constexpr int kFirstValue = 5;
constexpr int kSecondValue = 9;
}  // namespace

struct Source : rqt::Object<> {
  Source() { bind(); }
  [[= rqt::signal_function]] void valueChanged(int value) { rqt::emit{this}(value); }
  [[= rqt::signal_function]] void done() { rqt::emit{this}(); }
  [[= rqt::slot]] void announce(int v) { valueChanged(v); }
};

struct Sink : rqt::Object<> {
  Sink() { bind(); }
  int value = 0;
  int calls = 0;
  [[= rqt::slot]] void setValue(int v) {
    value = v;
    ++calls;
  }
  [[= rqt::slot]] void tick() { ++calls; }
};

struct SubSink : Sink {
  SubSink() { bind(); }
  [[= rqt::slot]] void extra() { value = -1; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Source source;
  Sink sink;
  SubSink sub;

  CHECK(QObject::connect(&source, SIGNAL(valueChanged(int)), &sink, SLOT(setValue(int))));
  source.announce(kFirstValue);
  CHECK(sink.value == kFirstValue);

  // an inherited slot, reached through the derived object's meta-object
  CHECK(QObject::connect(&source, SIGNAL(valueChanged(int)), &sub, SLOT(setValue(int))));
  source.announce(kSecondValue);
  CHECK(sub.value == kSecondValue && sink.value == kSecondValue);

  // a signal with fewer arguments than the slot's connection needs fails to connect
  CHECK(!QObject::connect(&source, SIGNAL(done()), &sink, SLOT(setValue(int))));
  CHECK(QObject::connect(&source, SIGNAL(done()), &sink, SLOT(tick())));
  int const before = sink.calls;
  source.done();
  CHECK(sink.calls == before + 1);

  CHECK(QString{sub.metaObject()->className()} == "SubSink");
  CHECK(sub.inherits("Sink") && sub.inherits("QObject") && !sink.inherits("SubSink"));
  return rqt_test::finish("capability_string_connect_real_qt");
}
