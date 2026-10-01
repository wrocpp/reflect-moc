// Tier B: the two-line opt-in makes Qt's own pointer-to-member and functor
// QObject::connect work, and rqt::connect works with or without it.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>

namespace {
constexpr int kFirst = 4;
constexpr int kSecond = 8;
}  // namespace

struct Gadget : rqt::Object<> {
  static QMetaObject const& staticMetaObject;
  Gadget() { bind(); }
  int value_ = 0;
  [[= rqt::signal_function]] void valueChanged(int value) { rqt::emit{this}(value); }
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    valueChanged(v);
  }
};
RQT_STATIC_META_OBJECT(Gadget);

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Gadget a, b;

  int seen = 0;
  auto functor = QObject::connect(&a, &Gadget::valueChanged, &a, [&](int v) { seen = v; });
  CHECK(functor);
  a.setValue(kFirst);
  CHECK(seen == kFirst);

  CHECK(QObject::connect(&a, &Gadget::valueChanged, &b, &Gadget::setValue));
  a.setValue(kSecond);
  CHECK(b.value_ == kSecond);

  Gadget c;
  CHECK(rqt::connect(&a, &Gadget::valueChanged, &c, &Gadget::setValue));
  a.setValue(kFirst);
  CHECK(c.value_ == kFirst);

  CHECK(&Gadget::staticMetaObject == a.metaObject());
  return rqt_test::finish("capability_pmf_connect_with_static_metaobject");
}
