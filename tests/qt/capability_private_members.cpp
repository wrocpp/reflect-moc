// `private slots:` and `private:` data behind a property work with no extra
// line in the class: a splice is access-checked, so functions are called through
// std::meta::extract (not access-checked for functions) and private data members
// through their offset.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>

namespace {
constexpr int kStep = 4;
constexpr int kStart = 10;
}  // namespace

class Window : public rqt::Object<> {
 public:
  Window() { bind(); }
  int loads() const { return loads_; }
  int count() const { return count_; }

 private:
  [[= rqt::slot]] void loadImage() { ++loads_; }
  [[= rqt::slot]] void advance(int by) {
    count_ += by;
    countChanged(count_);
  }
  [[= rqt::invokable]] int secret() const { return count_ * 2; }
  [[= rqt::signal_function]] void countChanged(int count) { rqt::emit{this}(count); }

  [[= rqt::property{.notify = "countChanged"}]] int count_ = kStart;
  int loads_ = 0;
};

class Spy : public rqt::Object<> {
 public:
  Spy() { bind(); }
  int last = 0;
  [[= rqt::slot]] void onCount(int c) { last = c; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Window window;
  Spy spy;
  QMetaObject const* mo = window.metaObject();

  CHECK(QMetaObject::invokeMethod(&window, "loadImage") && window.loads() == 1);
  CHECK(QMetaObject::invokeMethod(&window, "advance", Q_ARG(int, kStep)) && window.count() == kStart + kStep);
  int secret = 0;
  CHECK(QMetaObject::invokeMethod(&window, "secret", Q_RETURN_ARG(int, secret)) && secret == 2 * (kStart + kStep));

  // a private data member as a MEMBER property, with its private NOTIFY signal
  CHECK(QObject::connect(&window, SIGNAL(countChanged(int)), &spy, SLOT(onCount(int))));
  QMetaProperty const count = mo->property(mo->indexOfProperty("count_"));
  CHECK(count.isValid() && count.read(&window).toInt() == kStart + kStep);
  CHECK(count.write(&window, kStart) && window.count() == kStart && spy.last == kStart);

  CHECK(mo->indexOfSignal("countChanged(int)") >= 0 && mo->method(mo->indexOfSlot("advance(int)")).access() ==
                                                           QMetaMethod::Private);
  return rqt_test::finish("capability_private_members");
}
