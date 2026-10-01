// FINAL, CONSTANT, REQUIRED, USER, DESIGNABLE/SCRIPTABLE/STORED and a property
// NAME that differs from the accessor, written into the PropertyData flags as
// moc writes them.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaProperty>

struct Sample : rqt::Object<> {
  Sample() { bind(); }

  [[= rqt::property{.final = true}]] int plain() const { return 1; }
  [[= rqt::property{.final = true, .constant = true}]] int fixed() const { return 2; }
  [[= rqt::property{.required = true, .user = true}]] int needed() const { return 3; }
  [[= rqt::property{.designable = false, .scriptable = false, .stored = false}]] int hidden() const { return 4; }
  [[= rqt::property{.name = "renamed"}]] int original() const { return 5; }
  [[= rqt::property{.name = "shown", .final = true}]] int stored_value = 6;
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Sample sample;
  QMetaObject const* mo = sample.metaObject();
  auto property = [&](char const* name) { return mo->property(mo->indexOfProperty(name)); };

  CHECK(property("plain").isFinal() && !property("plain").isConstant());
  CHECK(property("fixed").isConstant() && property("fixed").isFinal());
  CHECK(property("needed").isRequired() && property("needed").isUser());
  CHECK(!property("plain").isRequired() && !property("plain").isUser());
  CHECK(property("plain").isDesignable() && property("plain").isScriptable() && property("plain").isStored());
  CHECK(!property("hidden").isDesignable() && !property("hidden").isScriptable() && !property("hidden").isStored());

  // NAME replaces the accessor's name, for a getter and for a data member
  CHECK(!property("original").isValid());
  CHECK(property("renamed").isValid() && property("renamed").read(&sample).toInt() == 5);
  CHECK(property("shown").isValid() && property("shown").isFinal());
  CHECK(sample.property("renamed").toInt() == 5);
  return rqt_test::finish("capability_property_flags");
}
