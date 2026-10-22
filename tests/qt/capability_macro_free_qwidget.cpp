// rqt::ObjectOf<D, QWidget>: a widget and a derived widget with no macro, on top of QWidget's own meta-object
// (offscreen QPA), with a data-member signal in each class.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QMetaProperty>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

namespace {
constexpr int kWidth = 120;
constexpr int kBadge = 4;
}  // namespace

class Panel : public rqt::ObjectOf<Panel, QWidget> {
 public:
  explicit Panel(QWidget* parent = nullptr) : rqt::ObjectOf<Panel, QWidget>(parent) {}
  [[= rqt::property{.write = "setBadge", .notify = "badgeChanged"}]] int badge() const { return badge_; }
  rqt::signal<void(int)> badgeChanged;
  [[= rqt::slot]] void setBadge(int b) {
    if (b == badge_) return;
    badge_ = b;
    badgeChanged(b);
  }

 private:
  int badge_ = 0;
};

class Footer : public rqt::ObjectOf<Footer, Panel> {
 public:
  explicit Footer(QWidget* parent = nullptr) : rqt::ObjectOf<Footer, Panel>(parent) {}
  [[= rqt::slot]] void widen() { resize(kWidth, height()); }
  rqt::signal<void()> widened;
  void widenAndTell() {
    widen();
    widened();
  }
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  Panel parent;
  Footer footer{&parent};
  QMetaObject const* mo = footer.metaObject();

  CHECK(QByteArray{mo->className()} == "Footer");
  CHECK(QByteArray{mo->superClass()->superClass()->className()} == "QWidget");
  CHECK(footer.inherits("QWidget") && footer.inherits("Panel"));
  CHECK(footer.parentWidget() == &parent);

  int heard = 0;
  QObject::connect(&footer, &Panel::badgeChanged, &footer, [&](int) { ++heard; });
  int told = 0;
  QObject::connect(&footer, &Footer::widened, &footer, [&] { ++told; });
  CHECK(footer.setProperty("badge", kBadge) && footer.badge() == kBadge && heard == 1);
  CHECK(footer.setProperty("windowTitle", QString{"footer"}) && footer.windowTitle() == "footer");
  CHECK(QMetaObject::invokeMethod(&footer, "widen") && footer.width() == kWidth);
  CHECK(QMetaObject::invokeMethod(&footer, "hide"));  // QWidget's own slot, through the base chain
  footer.widenAndTell();
  CHECK(told == 1);

  CHECK(qobject_cast<Panel*>(static_cast<QObject*>(&footer)) == &footer);
  CHECK(qobject_cast<Footer*>(static_cast<QObject*>(&parent)) == nullptr);
  CHECK(qobject_cast<QWidget*>(static_cast<QObject*>(&footer)) == &footer);
  return rqt_test::finish("capability_macro_free_qwidget");
}
