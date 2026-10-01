// The Qt-like syntax: RQT_OBJECT in place of Q_OBJECT, RQT_PROPERTY taking the exact
// Q_PROPERTY text. Slots, invokables and signals stay annotations ([[=rqt::slot]]).
#pragma once

#include "object.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QString>

#define RQT_CAT_(a, b) a##b
#define RQT_CAT(a, b) RQT_CAT_(a, b)
#define RQT_UNIQUE(prefix) RQT_CAT(prefix, __LINE__)

// The class being defined, from inside one of its own member functions.
#define RQT_SELF (::std::meta::parent_of(::std::meta::current_function()))

// Q_OBJECT without a class name, for `class C : public QObject`. The functions are defined in
// the class body, a complete-class context, so reflection sees every member. The overrides
// are the ones moc declares, so Qt's own checks (qobject_cast, HasQ_OBJECT_Macro, PMF connect,
// qmlRegisterType) pass natively.
#define RQT_OBJECT                                                                                         \
 public:                                                                                                   \
  static constexpr bool rqt_object_ = true;                                                                \
  static ::QMetaObject const& rqt_meta_() { return ::rqt::meta_of_class<RQT_SELF>(); }                     \
  static inline ::QMetaObject const& staticMetaObject = rqt_meta_();                                       \
  const ::QMetaObject* metaObject() const override {                                                       \
    return ::QObject::d_ptr->metaObject ? ::QObject::d_ptr->dynamicMetaObject() : &rqt_meta_();            \
  }                                                                                                        \
  void* qt_metacast(const char* rqt_name) override { return ::rqt::metacast_impl<RQT_SELF>(this, rqt_name); } \
  int qt_metacall(::QMetaObject::Call rqt_call, int rqt_id, void** rqt_args) override {                    \
    return ::rqt::metacall_impl<RQT_SELF>(this, rqt_call, rqt_id, rqt_args);                               \
  }                                                                                                        \
  static ::QString tr(const char* rqt_source, const char* rqt_disambiguation = nullptr, int rqt_n = -1) {  \
    return ::QCoreApplication::translate(rqt_meta_().className(), rqt_source, rqt_disambiguation, rqt_n);  \
  }                                                                                                        \
                                                                                                           \
 private:                                                                                                  \
  static void qt_static_metacall(::QObject* rqt_o, ::QMetaObject::Call rqt_call, int rqt_id, void** rqt_args) { \
    ::rqt::static_call<RQT_SELF>(rqt_o, rqt_call, rqt_id, rqt_args);                                       \
  }

// Q_ENUM(Mode), Q_FLAG(Opts) and Q_CLASSINFO("key", "value"): static constexpr holders the library finds
// by reflection. The enum (and for Q_FLAG the QFlags alias) must already be declared.
#define RQT_ENUM(Type) static constexpr ::rqt::enum_decl RQT_UNIQUE(rqt_enum_){^^Type, false};
#define RQT_FLAG(Type) static constexpr ::rqt::enum_decl RQT_UNIQUE(rqt_flag_){^^Type, true};
#define RQT_CLASSINFO(key, value) static constexpr ::rqt::classinfo RQT_UNIQUE(rqt_classinfo_){key, value};

// Q_PROPERTY(type name READ r WRITE w NOTIFY n ...): the argument text is parsed at compile time.
#define RQT_PROPERTY(...) \
  static constexpr ::rqt::prop_decl RQT_UNIQUE(rqt_property_) = ::rqt::parse_property(#__VA_ARGS__);
