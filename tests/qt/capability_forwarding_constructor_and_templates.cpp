// The constructor pattern for a class that inherits its base's constructors:
// a forwarding constructor template that calls bind() (`using Base::Base;`
// would not run it). Member templates and constructor templates are not
// part of the meta-object and must not break it.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QMetaProperty>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <utility>

namespace {
constexpr int kBadge = 5;
}  // namespace

struct Base : rqt::Object<QWidget> {
  template <class... A>
    requires rqt::forwardable<Base, A...>
  explicit Base(A&&... a) : rqt::Object<QWidget>(std::forward<A>(a)...) {
    bind();
  }
  [[= rqt::property{.write = "setBadge"}]] int badge() const { return badge_; }
  [[= rqt::slot]] void setBadge(int b) { badge_ = b; }

  // member templates carry no annotation and are skipped
  template <class T>
  T twice(T v) const {
    return v + v;
  }
  template <class T>
  [[= rqt::slot]] void ignored(T) {}

  int badge_ = 0;
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  Base parent;
  Base child{&parent};

  CHECK(QByteArray{child.metaObject()->className()} == "Base");
  CHECK(child.parentWidget() == &parent);
  CHECK(child.setProperty("badge", kBadge) && child.badge_ == kBadge);
  CHECK(child.twice(kBadge) == 2 * kBadge);
  CHECK(child.metaObject()->indexOfSlot("ignored(int)") < 0);
  return rqt_test::finish("capability_forwarding_constructor_and_templates");
}
