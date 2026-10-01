// Multi-level inheritance (A : rqt::Object, B : A, C : B): inherited slots,
// signals and properties are reachable by name, by index and through Qt's
// per-level offsets.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>

namespace {
constexpr int kSpeed = 12;
constexpr int kGear = 3;
constexpr int kHeight = 9;
constexpr int kLevels = 3;
}  // namespace

struct Vehicle : rqt::Object<> {
  Vehicle() { bind(); }
  [[= rqt::property{.write = "setSpeed", .notify = "speedChanged"}]] int speed() const { return speed_; }
  [[= rqt::signal]] void speedChanged(int speed) { rqt::emit{this}(speed); }
  [[= rqt::slot]] void setSpeed(int s) {
    if (s == speed_) return;
    speed_ = s;
    speedChanged(s);
  }
  int speed_ = 0;
};

struct Car : Vehicle {
  Car() { bind(); }
  [[= rqt::property{.write = "setGear"}]] int gear() const { return gear_; }
  [[= rqt::slot]] void setGear(int g) { gear_ = g; }
  int gear_ = 0;
};

struct Plane : Car {
  Plane() { bind(); }
  [[= rqt::property{}]] int height = 0;
  [[= rqt::signal]] void landed() { rqt::emit{this}(); }
  [[= rqt::invokable]] int altitudeTwice() const { return 2 * height; }
};

struct Spy : rqt::Object<> {
  Spy() { bind(); }
  int speeds = 0;
  int landings = 0;
  [[= rqt::slot]] void onSpeed(int) { ++speeds; }
  [[= rqt::slot]] void onLanded() { ++landings; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Plane plane;
  Spy spy;
  QMetaObject const* mo = plane.metaObject();

  CHECK(QByteArray{mo->className()} == "Plane");
  CHECK(QByteArray{mo->superClass()->className()} == "Car");
  CHECK(QByteArray{mo->superClass()->superClass()->className()} == "Vehicle");
  CHECK(mo->superClass()->superClass()->superClass() == &QObject::staticMetaObject);
  CHECK(plane.inherits("Vehicle") && plane.inherits("Car") && plane.inherits("QObject"));

  // properties of every level, with offsets that grow level by level
  CHECK(mo->propertyCount() - QObject::staticMetaObject.propertyCount() == kLevels);
  CHECK(mo->property(mo->indexOfProperty("speed")).write(&plane, kSpeed) && plane.speed_ == kSpeed);
  CHECK(mo->property(mo->indexOfProperty("gear")).write(&plane, kGear) && plane.gear_ == kGear);
  CHECK(mo->property(mo->indexOfProperty("height")).write(&plane, kHeight) && plane.height == kHeight);
  CHECK(plane.property("speed").toInt() == kSpeed);

  // an inherited slot and an own invokable, by name
  CHECK(QMetaObject::invokeMethod(&plane, "setGear", Q_ARG(int, kGear + 1)) && plane.gear_ == kGear + 1);
  int twice = 0;
  CHECK(QMetaObject::invokeMethod(&plane, "altitudeTwice", Q_RETURN_ARG(int, twice)) && twice == 2 * kHeight);

  // an inherited signal reaches a string connection, and the notify signal fires through Qt
  CHECK(QObject::connect(&plane, SIGNAL(speedChanged(int)), &spy, SLOT(onSpeed(int))));
  CHECK(QObject::connect(&plane, SIGNAL(landed()), &spy, SLOT(onLanded())));
  plane.setSpeed(kSpeed + 1);
  CHECK(spy.speeds == 1);
  CHECK(plane.setProperty("speed", kSpeed + 2) && spy.speeds == 2);
  plane.landed();
  CHECK(spy.landings == 1);

  // rqt::connect takes the inherited signal through a pointer to the derived class
  Spy other;
  CHECK(rqt::connect(&plane, &Plane::speedChanged, &other, &Spy::onSpeed));
  plane.setSpeed(kSpeed + 3);
  CHECK(other.speeds == 1);

  // an object bound at one level still reports that level
  Vehicle vehicle;
  CHECK(QByteArray{vehicle.metaObject()->className()} == "Vehicle");
  CHECK(rqt::cast<Vehicle>(static_cast<QObject*>(&plane)) == &plane);
  CHECK(rqt::cast<Plane>(static_cast<QObject*>(&vehicle)) == nullptr);
  return rqt_test::finish("capability_inherited_members");
}
