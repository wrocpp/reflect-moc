// rqt::Object<QWidget>: a widget with reflected slots and properties, on top of
// QWidget's own meta-object (offscreen QPA).
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QMetaProperty>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

namespace {
constexpr int kWidth = 120;
constexpr int kBadge = 4;
}  // namespace

struct Panel : rqt::Object<QWidget> {
  explicit Panel(QWidget* parent = nullptr) : rqt::Object<QWidget>(parent) { bind(); }
  [[= rqt::property{.write = "setBadge", .notify = "badgeChanged"}]] int badge() const { return badge_; }
  [[= rqt::signal]] void badgeChanged(int badge) { rqt::emit{this}(badge); }
  [[= rqt::slot]] void setBadge(int b) {
    if (b == badge_) return;
    badge_ = b;
    badgeChanged(b);
  }
  int badge_ = 0;
};

struct Footer : Panel {
  explicit Footer(QWidget* parent = nullptr) : Panel(parent) { bind(); }
  [[= rqt::slot]] void widen() { resize(kWidth, height()); }
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

  // a reflected property and slot, and a property that belongs to QWidget itself
  CHECK(footer.setProperty("badge", kBadge) && footer.badge_ == kBadge);
  CHECK(footer.property("badge").toInt() == kBadge);
  CHECK(footer.setProperty("windowTitle", QString{"footer"}) && footer.windowTitle() == "footer");
  CHECK(QMetaObject::invokeMethod(&footer, "widen") && footer.width() == kWidth);
  CHECK(QMetaObject::invokeMethod(&footer, "hide"));  // QWidget's own slot, through the base chain

  CHECK(rqt::cast<Panel>(static_cast<QObject*>(&footer)) == &footer);
  CHECK(rqt::cast<Footer>(static_cast<QObject*>(&parent)) == nullptr);
  return rqt_test::finish("capability_qwidget_base");
}
