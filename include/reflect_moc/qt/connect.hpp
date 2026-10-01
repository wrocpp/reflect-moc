// rqt::cast and rqt::connect: the replacements for qobject_cast and the
// pointer-to-member QObject::connect for classes that skip the tier B opt-in.
#pragma once

#include "object.hpp"

#include <QtCore/QObject>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rqt {

template <class T>
T* cast(QObject* o) {
  return static_cast<T*>(static_meta_object<T>.cast(o));
}

template <class T>
T const* cast(QObject const* o) {
  return static_cast<T const*>(static_meta_object<T>.cast(o));
}

namespace detail {

// The method index of a pointer to member, relative to C's meta-object chain.
template <class C, class F>
int method_index(F C::* pmf) {
  return [pmf]<std::size_t... I>(std::index_sequence<I...>) {
    int found = -1;
    (
        [&] {
          constexpr method_entry e = method_entries<C>[I];
          if constexpr (!e.cloned && std::is_same_v<decltype(&[:e.fn:]), F C::*>)
            if (found < 0 && &[:e.fn:] == pmf) found = static_cast<int>(I);
        }(),
        ...);
    return found < 0 ? -1 : static_meta_object<C>.methodOffset() + found;
  }(std::make_index_sequence<method_total<C>>{});
}

// The slot's parameters must be a prefix of the signal's. Index-based
// QMetaObject::connect does not check this, so the check is ours.
template <class SigF, class SlotF>
struct args_prefix : std::false_type {};

template <class SRet, class... S, class RRet, class... R>
struct args_prefix<SRet(S...), RRet(R...)> {
  static constexpr bool value = [] {
    if constexpr (sizeof...(R) > sizeof...(S)) {
      return false;
    } else {
      return []<std::size_t... I>(std::index_sequence<I...>) {
        using SL = std::tuple<std::remove_cvref_t<S>...>;
        return (std::is_same_v<std::tuple_element_t<I, SL>, std::remove_cvref_t<R>> && ...);
      }(std::index_sequence_for<R...>{});
    }
  }();
};

template <class F>
struct strip_const_fn {
  using type = F;
};
template <class R, class... A>
struct strip_const_fn<R(A...) const> {
  using type = R(A...);
};

}  // namespace detail

// Signal to slot by index (QMetaObject::connect is public API). Functor slots
// have no public index-based entry point; they need the tier B opt-in.
template <class S, class SF, class R, class RF>
QMetaObject::Connection connect(QObject const* sender, SF S::* signal, QObject const* receiver, RF R::* slot,
                                Qt::ConnectionType type = Qt::AutoConnection) {
  static_assert(detail::args_prefix<SF, typename detail::strip_const_fn<RF>::type>::value,
                "rqt::connect: the slot's parameters must be a prefix of the signal's");
  return QMetaObject::connect(sender, detail::method_index(signal), receiver, detail::method_index(slot), type);
}

}  // namespace rqt
