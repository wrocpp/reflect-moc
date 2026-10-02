// How a static signal is fired: `emit s(v);` through the compat header, `s(v).from(object);` where there
// is no `this` (a free function, a non-QObject, a lambda that does not capture this), and from a lambda
// that does. `emit` of a real Qt signal and of a non-static rqt::signal keep working.
#include "check.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>

#include <reflect_moc/compat.hpp>

namespace {
constexpr int kFirst = 3;
constexpr int kSecond = 4;
constexpr int kThird = 5;
constexpr int kFourth = 6;
constexpr int kNone = 0;
}  // namespace

class Counter : public QObject {
  Q_OBJECT

 public:
  explicit Counter(QObject* parent = nullptr) : QObject(parent) {}
  void emitSyntax(int v) { emit valueChanged(v); }
  void qEmitSyntax(int v) { Q_EMIT valueChanged(v); }
  void fromSyntax(int v) { valueChanged(v).from(this); }
  void noArguments() { emit done(); }
  void fromLambdaThis(int v) {
    auto fire = [this](int x) { emit valueChanged(x); };
    fire(v);
  }
  void fromLambdaRef(int v) {
    auto fire = [&](int x) { emit valueChanged(x); };
    fire(v);
  }
  void realQtSignal() { emit destroyed(nullptr); }  // a Qt signal is a void call: it joins the built-in comma
  void nonStatic(int v) { emit plain(v); }          // so does a non-static rqt::signal

 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
  static inline rqt::static_signal<void()> done{};
  rqt::signal<void(int)> plain;
};

void fire_from_free_function(Counter* c, int v) { Counter::valueChanged(v).from(c); }

struct NotAQObject {
  void fire(Counter* c, int v) const { Counter::valueChanged(v).from(c); }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Counter counter;
  int heard = kNone;
  int done = 0;
  int plain = kNone;
  int destroyed = 0;
  CHECK(QObject::connect(&counter, &Counter::valueChanged, &counter, [&](int v) { heard = v; }));
  CHECK(QObject::connect(&counter, &Counter::done, &counter, [&] { ++done; }));
  CHECK(QObject::connect(&counter, &Counter::plain, &counter, [&](int v) { plain = v; }));
  CHECK(QObject::connect(&counter, &QObject::destroyed, &counter, [&](QObject*) { ++destroyed; }));

  counter.emitSyntax(kFirst);
  CHECK(heard == kFirst);
  counter.qEmitSyntax(kSecond);
  CHECK(heard == kSecond);
  counter.fromSyntax(kThird);
  CHECK(heard == kThird);
  counter.noArguments();
  CHECK(done == 1);
  counter.fromLambdaThis(kFourth);
  CHECK(heard == kFourth);
  heard = kNone;
  counter.fromLambdaRef(kFirst);
  CHECK(heard == kFirst);
  fire_from_free_function(&counter, kSecond);
  CHECK(heard == kSecond);
  NotAQObject{}.fire(&counter, kThird);
  CHECK(heard == kThird);
  counter.realQtSignal();
  CHECK(destroyed == 1);
  counter.nonStatic(kFourth);
  CHECK(plain == kFourth);
  return rqt_test::finish("capability_static_signal_emit_syntax");
}
