// A class with static signals defined in a header that two translation units include. The static
// signal members are `static inline`: one object per program, so the address that identifies a
// signal is the same in both units (checked with -flto -Wodr -Wlto-type-mismatch -Werror).
#pragma once

#include <QtCore/QObject>

#include <reflect_moc/compat.hpp>

#include <typeinfo>

class SharedStatic : public QObject {
  Q_OBJECT

 public:
  explicit SharedStatic(QObject* parent = nullptr) : QObject(parent) {}
  void fire(int v) {
    emit changed(v);
    emit done();
  }

 signals:
  [[= rqt::names("value")]] static inline rqt::static_signal<void(int)> changed{};
  static inline rqt::static_signal<void()> done{};
};

// defined in the second translation unit
void second_unit_fire(SharedStatic* object, int value);
char const* second_unit_signal_type_name();
int second_unit_changed_index();
void const* second_unit_changed_address();
