// rqt::Gadget<D, B>: the base of a Q_GADGET with no macro, a value class that has a meta-object and no QObject in it.
//
//   struct Point : rqt::Gadget<Point> { [[= rqt::property{}]] int x = 0; [[= rqt::invokable]] int manhattan() const; };
//   struct Point3 : rqt::Gadget<Point3, Point> { ... };       // a gadget as the base
//
// The property code runs in the static meta-call, as moc's gadgets do (PropertyAccessInStaticMetaCall), so
// QMetaProperty::readOnGadget and QVariant::fromValue work with stock Qt.
#pragma once

#include "object.hpp"

#include <type_traits>

namespace rqt {

namespace detail {
struct no_gadget_base {};
template <class B>
using gadget_base_t = std::conditional_t<std::is_void_v<B>, no_gadget_base, B>;
}  // namespace detail

template <class D, class B = void>
class Gadget : public detail::gadget_base_t<B> {
 public:
  static inline QMetaObject const& staticMetaObject = static_meta_object<D>;
  void qt_check_for_QGADGET_macro() {}
  using QtGadgetHelper = void;
};

}  // namespace rqt
