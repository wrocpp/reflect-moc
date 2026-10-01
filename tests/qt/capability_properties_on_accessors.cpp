// [[=rqt::property]] on a getter (READ implied, WRITE/RESET resolved by name)
// and on a data member (MEMBER style), read back through QMetaProperty.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>

namespace {
constexpr int kDefaultTarget = 20;
constexpr int kNewTarget = 25;
constexpr int kFixed = 7;
}  // namespace

struct Thermostat : rqt::Object<> {
  Thermostat() { bind(); }

  [[= rqt::property{.write = "setTarget", .reset = "resetTarget"}]] int target() const { return target_; }
  [[= rqt::property{}]] int fixed() const { return kFixed; }
  [[= rqt::property{}]] QString label = "idle";

  [[= rqt::slot]] void setTarget(int t) { target_ = t; }
  [[= rqt::slot]] void resetTarget() { target_ = kDefaultTarget; }

  int target_ = kDefaultTarget;
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Thermostat t;
  QMetaObject const* mo = t.metaObject();
  int const first = mo->propertyOffset();
  CHECK(mo->propertyCount() - first == 3);

  QMetaProperty const target = mo->property(mo->indexOfProperty("target"));
  CHECK(target.isValid() && target.isReadable() && target.isWritable() && target.isResettable());
  CHECK(target.hasStdCppSet());
  CHECK(QString{target.typeName()} == "int");
  CHECK(target.write(&t, kNewTarget) && t.target_ == kNewTarget);
  CHECK(target.read(&t).toInt() == kNewTarget);
  CHECK(target.reset(&t) && t.target_ == kDefaultTarget);

  // getter only: readable, not writable
  QMetaProperty const fixed = mo->property(mo->indexOfProperty("fixed"));
  CHECK(fixed.isReadable() && !fixed.isWritable() && !fixed.isResettable());
  CHECK(fixed.read(&t).toInt() == kFixed);

  // MEMBER style: read and write go straight to the data member
  QMetaProperty const label = mo->property(mo->indexOfProperty("label"));
  CHECK(label.isReadable() && label.isWritable());
  CHECK(QString{label.typeName()} == "QString");
  CHECK(label.write(&t, QString{"busy"}) && t.label == "busy");
  CHECK(t.property("label").toString() == "busy");
  CHECK(t.setProperty("target", kNewTarget) && t.target() == kNewTarget);
  return rqt_test::finish("capability_properties_on_accessors");
}
