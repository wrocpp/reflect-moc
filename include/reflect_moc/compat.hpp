// Source compatibility with Qt's macros: include this AFTER every Qt header, and a class
// written for moc compiles with reflect-moc unchanged where the macros carry the meaning:
//
//   Q_OBJECT              -> RQT_OBJECT
//   Q_PROPERTY(...)       -> RQT_PROPERTY(...)       the exact text
//   Q_INVOKABLE           -> [[=rqt::invokable]]
//   Q_ENUM(E) / Q_FLAG(F) -> RQT_ENUM / RQT_FLAG
//   Q_CLASSINFO(k, v)     -> RQT_CLASSINFO
//   emit / Q_EMIT         -> ::rqt::emitter{this},   (fires an rqt::static_signal call)
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
#pragma push_macro("emit")
#pragma push_macro("Q_EMIT")

#undef Q_OBJECT
#undef Q_PROPERTY
#undef Q_INVOKABLE
#undef Q_ENUM
#undef Q_FLAG
#undef Q_CLASSINFO
#undef emit
#undef Q_EMIT

#define Q_OBJECT RQT_OBJECT
#define Q_PROPERTY(...) RQT_PROPERTY(__VA_ARGS__)
#define Q_INVOKABLE [[= ::rqt::invokable]]
#define Q_ENUM(Type) RQT_ENUM(Type)
#define Q_FLAG(Type) RQT_FLAG(Type)
#define Q_CLASSINFO(key, value) RQT_CLASSINFO(key, value)
// `emit sig(args);` for an rqt::static_signal: a static member cannot know which object emits it, so
// `emit` hands `this` along. Where `this` does not exist (a static member function, a free function, a
// lambda that does not capture it) write `sig(args).from(object);`. A real Qt signal still works: a void
// call joins the built-in comma. `emit other->sig(v)` on another object of the SAME class fires on `this`
// (the static member discards `other`): write `sig(v).from(other)`.
#define emit ::rqt::emitter{this},
#define Q_EMIT ::rqt::emitter{this},
