// An enum that lives in a namespace (Qt::Orientation) as a slot parameter, a
// signal argument and a property type: written qualified, as moc writes it.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>

struct Slider : rqt::Object<> {
  Slider() { bind(); }
  Qt::Orientation orientation_ = Qt::Horizontal;
  [[= rqt::property{.write = "setOrientation", .notify = "orientationChanged"}]] Qt::Orientation orientation() const {
    return orientation_;
  }
  [[= rqt::signal]] void orientationChanged(Qt::Orientation o) { rqt::emit{this}(o); }
  [[= rqt::slot]] void setOrientation(Qt::Orientation o) {
    if (o == orientation_) return;
    orientation_ = o;
    orientationChanged(o);
  }
};

struct Listener : rqt::Object<> {
  Listener() { bind(); }
  Qt::Orientation last = Qt::Horizontal;
  [[= rqt::slot]] void onOrientation(Qt::Orientation o) { last = o; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Slider slider;
  Listener listener;
  QMetaObject const* mo = slider.metaObject();

  CHECK(mo->indexOfSlot("setOrientation(Qt::Orientation)") >= 0);
  CHECK(QObject::connect(&slider, SIGNAL(orientationChanged(Qt::Orientation)), &listener,
                         SLOT(onOrientation(Qt::Orientation))));
  CHECK(QMetaObject::invokeMethod(&slider, "setOrientation", Q_ARG(Qt::Orientation, Qt::Vertical)));
  CHECK(slider.orientation_ == Qt::Vertical && listener.last == Qt::Vertical);

  QMetaProperty const prop = mo->property(mo->indexOfProperty("orientation"));
  CHECK(prop.isValid() && prop.metaType() == QMetaType::fromType<Qt::Orientation>());
  CHECK(prop.read(&slider).value<Qt::Orientation>() == Qt::Vertical);
  return rqt_test::finish("capability_namespace_enum_parameter");
}
