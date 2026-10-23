// rqt::Gadget<D, B>: the base of a Q_GADGET with no macro, a value class that has a meta-object and no QObject in it.
//
//   struct Point : rqt::Gadget<> { [[= rqt::property{}]] int x = 0; [[= rqt::invokable]] int manhattan() const; };
//   struct Point3 : rqt::Gadget<Point> { ... };               // a gadget as the base
// The class is not named: the default argument of the base is evaluated in the base clause, where current_class() is it.
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

template <class B = void, meta::info D = deduced_class<B>()>
class Gadget : public detail::gadget_base_t<B> {
 public:
  using rqt_gadget_ = typename[:D:];
  static inline QMetaObject const& staticMetaObject = static_meta_object<typename[:D:]>;
  void qt_check_for_QGADGET_macro() {}
  using QtGadgetHelper = void;
};

}  // namespace rqt
