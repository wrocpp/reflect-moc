// A class defined in a header that two translation units include: the
// in-class static meta-object is one inline variable, so both TUs see the same.
#pragma once
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Shared : rqt::Object<> {
  RQT_META_OBJECT(Shared);
  Shared() { bind(); }
  [[= rqt::slot]] void poke() { ++pokes; }
  int pokes = 0;
};

// defined in the second translation unit
QMetaObject const* second_unit_meta_object();
Shared* second_unit_cast(QObject* o);
char const* meta_name_seen_by_static_initializer();
