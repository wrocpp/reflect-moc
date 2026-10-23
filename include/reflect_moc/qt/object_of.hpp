// rqt::ObjectOf<D, B>: the base of a class that has no macro. It supplies what Q_OBJECT declares (the
// meta-object, qt_metacast, qt_metacall, tr) as overrides that name D, so nothing in the class repeats
// the class name, and it publishes the owner for the `rqt::signal` data members declared in D.
//
//   class Counter : public rqt::ObjectOf<> { ... };                         // a QObject
//   class Scaled  : public rqt::ObjectOf<Counter> { ... };                  // a reflected class as its base
//   class Panel   : public rqt::ObjectOf<QWidget> { ... };                  // a Qt class as its base
// The class is not named: see detail::deduced_class.
#pragma once

#include "object.hpp"
#include "signal.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <concepts>
#include <type_traits>
#include <utility>

namespace rqt {

template <class B = QObject, meta::info D = deduced_class<B>()>
class ObjectOf : public B {
  using Self = typename[:D:];  // the class that derives from this one, incomplete until its body is closed

 public:
  using qt_base = B;
  using rqt_crtp_ = Self;  // what HasQ_OBJECT_Macro and reflection look for

  // The most derived class is not known to the constructors that forward, so they are templates. A copy or move of a
  // derived class must not match them (see not_copy_or_move for the other spelling of the same trap).
  ObjectOf() { publish_owner(); }
  template <class... A>
    requires(sizeof...(A) > 0 && std::constructible_from<B, A...> &&
             !(sizeof...(A) == 1 && (std::is_base_of_v<ObjectOf, std::remove_cvref_t<A>> && ...)))
  explicit(sizeof...(A) == 1) ObjectOf(A&&... a) : B(std::forward<A>(a)...) {
    publish_owner();
  }

  static inline QMetaObject const& staticMetaObject = static_meta_object<Self>;

  const QMetaObject* metaObject() const override {
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &static_meta_object<Self>;
  }
  void* qt_metacast(const char* name) override { return metacast_impl<D>(static_cast<Self*>(this), name); }
  int qt_metacall(QMetaObject::Call c, int id, void** a) override {
    return metacall_impl<D>(static_cast<Self*>(this), c, id, a);
  }
  static QString tr(const char* source, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate(static_meta_object<Self>.className(), source, disambiguation, n);
  }

 private:
  // The signals of D are constructed after this base, and take the owner from here. The base is a QObject, so the
  // conversion is an upcast: nothing about D is needed yet.
  void publish_owner() {
    detail::published_owner = {static_cast<QObject*>(this), &detail::owner_tag<D>};
  }
};

}  // namespace rqt

namespace QtPrivate {
template <class D>
  requires(requires { typename D::rqt_crtp_; })
struct HasQ_OBJECT_Macro<D> {
  enum { Value = 1 };
};
}  // namespace QtPrivate
