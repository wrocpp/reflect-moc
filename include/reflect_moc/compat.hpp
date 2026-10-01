// Source compatibility with Qt's macros: include this AFTER every Qt header, and a class
// written for moc compiles with reflect-moc unchanged where the macros carry the meaning:
//
//   Q_OBJECT              -> RQT_OBJECT
//   Q_PROPERTY(...)       -> RQT_PROPERTY(...)       the exact text
//   Q_INVOKABLE           -> [[=rqt::invokable]]
//   Q_ENUM(E) / Q_FLAG(F) -> RQT_ENUM / RQT_FLAG
//   Q_CLASSINFO(k, v)     -> RQT_CLASSINFO
//
// Not mapped, because a macro cannot do it: a slot or a signal declared only in a `slots:` or
// `signals:` section (add [[=rqt::slot]] to the slot, and write the signal as
// `rqt::signal<void(int)> name;`), Q_GADGET and Q_NAMESPACE, Q_INTERFACES, Q_SLOT and Q_SIGNAL
// markers, and QML_ELEMENT and friends (use rqt::register_qml).
//
// A Qt header included after this one would expand its own Q_OBJECT to RQT_OBJECT and break.
// Include reflect_moc/compat_end.hpp to hand the macros back to Qt.
#pragma once

#include <reflect_moc/qt/qt.hpp>

#pragma push_macro("Q_OBJECT")
#pragma push_macro("Q_PROPERTY")
#pragma push_macro("Q_INVOKABLE")
#pragma push_macro("Q_ENUM")
#pragma push_macro("Q_FLAG")
#pragma push_macro("Q_CLASSINFO")

#undef Q_OBJECT
#undef Q_PROPERTY
#undef Q_INVOKABLE
#undef Q_ENUM
#undef Q_FLAG
#undef Q_CLASSINFO

#define Q_OBJECT RQT_OBJECT
#define Q_PROPERTY(...) RQT_PROPERTY(__VA_ARGS__)
#define Q_INVOKABLE [[= ::rqt::invokable]]
#define Q_ENUM(Type) RQT_ENUM(Type)
#define Q_FLAG(Type) RQT_FLAG(Type)
#define Q_CLASSINFO(key, value) RQT_CLASSINFO(key, value)
