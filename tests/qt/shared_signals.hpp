// A class with data-member signals defined in a header that two translation units include.
// The closure in the default template argument of rqt::signal must give the same member type
// in both (ODR); the test compares the mangled type names.
#pragma once

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>

#include <typeinfo>

class SharedSignals : public QObject {
  RQT_OBJECT

 public:
  explicit SharedSignals(QObject* parent = nullptr) : QObject(parent) {}
  void fire(int v) {
    emit changed(v);
    emit done();
  }

 signals:
  [[= rqt::names("value")]] rqt::signal<void(int)> changed;
  rqt::signal<void()> done;
};

// defined in the second translation unit
void second_unit_fire(SharedSignals* object, int value);
char const* second_unit_signal_type_name();
int second_unit_changed_index();
