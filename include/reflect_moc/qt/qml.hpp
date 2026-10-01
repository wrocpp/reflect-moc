// QML registration of a reflected class through its own meta-object.
#pragma once

#include "object.hpp"

#include <QtQml/qqml.h>

namespace rqt {

// Needs the tier B opt-in: qmlRegisterType<T> reads T::staticMetaObject.
template <class T>
int register_qml(char const* uri, int major, int minor, char const* name) {
  static_assert(detail::declares_static_meta_object(^^T),
                "rqt::register_qml: the class needs the tier B opt-in: `static QMetaObject const& staticMetaObject;` "
                "in the class and RQT_STATIC_META_OBJECT(T); after it");
  return qmlRegisterType<T>(uri, major, minor, name);
}

}  // namespace rqt
