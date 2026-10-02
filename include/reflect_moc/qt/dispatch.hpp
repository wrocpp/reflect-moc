// qt_static_metacall for a reflected class: invoke, index-of-method, property
// read, write and reset, and the metatype registration calls.
#pragma once

#include "metadata.hpp"

#include <QtCore/QMetaObject>
#include <QtCore/QMetaType>
#include <QtCore/QObject>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace rqt {
namespace detail {

// Calls f(std::integral_constant<size_t, I>) for the I that equals id. Empty
// packs are skipped with if constexpr: an empty || fold is a statement with no
// effect, and (void)-casting it makes GCC 16.2 escalate the lambda to consteval.
template <std::size_t N, class F>
void with_index(int id, F&& f) {
  if constexpr (N > 0) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((id == static_cast<int>(I) && (f(std::integral_constant<std::size_t, I>{}), true)) || ...);
    }(std::make_index_sequence<N>{});
  }
}

// --- private members ------------------------------------------------------------------
// A splice is access-checked at the splice site, so library code cannot write
// `self->[:fn:]` for a private slot (GCC 16.2: "'void D::hidden(int)' is private
// within this context"). std::meta::extract of a member FUNCTION is not
// access-checked, so functions are called through the extracted pointer to
// member. extract of a private data member is checked, so a private data member
// is reached through its offset.

template <info Fn>
constexpr auto member_pointer() {
  using Cls = typename[:meta::parent_of(Fn):];
  using F = typename[:meta::type_of(Fn):];
  return meta::extract<F Cls::*>(Fn);
}

template <info M, class C>
decltype(auto) member_ref(C* self) {
  using T = typename[:meta::type_of(M):];
  if constexpr (meta::is_public(M)) {
    return (self->[:M:]);
  } else {
    constexpr std::size_t offset = meta::offset_of(M).bytes;
    return (*reinterpret_cast<T*>(reinterpret_cast<std::byte*>(self) + offset));
  }
}

// Calls a slot, an invokable or a signal; calling a signal data member activates it.
template <info Fn, class C, class... A>
decltype(auto) call(C* self, A&&... a) {
  if constexpr (is_static_signal(Fn)) {
    return [:Fn:](std::forward<A>(a)...).from(self);
  } else if constexpr (is_data_signal(Fn)) {
    return member_ref<Fn>(self)(std::forward<A>(a)...);
  } else if constexpr (meta::is_public(Fn)) {
    return self->[:Fn:](std::forward<A>(a)...);  // default arguments apply, which a pointer to member cannot give
  } else {
    constexpr auto pointer = member_pointer<Fn>();
    return (self->*pointer)(std::forward<A>(a)...);
  }
}

template <info Fn, std::size_t P>
using param_t = std::remove_cvref_t<entry_param_t<Fn, P>>;

template <info Fn, std::size_t P>
decltype(auto) arg_at(void** args) {
  return *static_cast<param_t<Fn, P>*>(args[P + 1]);
}

// args[0] is the return slot, args[1..] point at the arguments.
template <info Fn, std::size_t N, class C>
void call_with_args(C* self, void** args) {
  [&]<std::size_t... P>(std::index_sequence<P...>) {
    using R = entry_return_t<Fn>;
    if constexpr (std::is_void_v<R>) {
      call<Fn>(self, arg_at<Fn, P>(args)...);
    } else {
      std::remove_cvref_t<R> r = call<Fn>(self, arg_at<Fn, P>(args)...);
      if (args[0]) *static_cast<std::remove_cvref_t<R>*>(args[0]) = std::move(r);
    }
  }(std::make_index_sequence<N>{});
}

template <class D>
void invoke_method(D* t, int id, void** a) {
  with_index<method_total<D>>(id, [&](auto ic) {
    constexpr method_entry e = method_entries<D>[decltype(ic)::value];
    call_with_args<e.fn, e.nargs>(t, a);
  });
}

// --- IndexOfMethod -----------------------------------------------------------------------

template <class D, std::size_t I>
bool match_signal(void** a) {
  constexpr method_entry e = method_entries<D>[I];
  if constexpr (e.cloned || kind_of(e.fn) != method_kind::signal_)
    return false;
  else if constexpr (is_data_signal(e.fn))  // a private signal member has no pointer a caller could name
    return meta::is_public(e.fn) && QtMocHelpers::indexOfMethod<decltype(&[:e.fn:])>(a, &[:e.fn:], static_cast<int>(I));
  else
    return QtMocHelpers::indexOfMethod<decltype(member_pointer<e.fn>())>(a, member_pointer<e.fn>(),
                                                                         static_cast<int>(I));
}

template <class D>
void index_of_method(void** a) {
  if constexpr (method_total<D> > 0)
    [&]<std::size_t... I>(std::index_sequence<I...>) { (match_signal<D, I>(a) || ...); }(
        std::make_index_sequence<method_total<D>>{});
}

// --- registering argument metatypes --------------------------------------------------------

template <info Fn, std::size_t N>
void register_argument(void** a) {
  auto* out = static_cast<QMetaType*>(a[0]);
  int const wanted = *static_cast<int*>(a[1]);
  [&]<std::size_t... P>(std::index_sequence<P...>) {
    (((wanted == static_cast<int>(P) && named_type_v<param_t<Fn, P>>)
          ? (*out = QMetaType::fromType<param_t<Fn, P>>(), true)
          : false) ||
     ...);
  }(std::make_index_sequence<N>{});
}

template <class D>
void register_method_argument(int id, void** a) {
  *static_cast<QMetaType*>(a[0]) = QMetaType();
  with_index<method_total<D>>(id, [&](auto ic) {
    constexpr method_entry e = method_entries<D>[decltype(ic)::value];
    if constexpr (e.nargs > 0) register_argument<e.fn, e.nargs>(a);
  });
}

// --- properties --------------------------------------------------------------------------

template <class T, class D, prop_desc d>
void read_property(D* t, void* v) {
  if constexpr (present(d.member))
    *static_cast<T*>(v) = member_ref<d.member>(t);
  else
    *static_cast<T*>(v) = call<d.read>(t);
}

template <prop_desc d, class D>
void emit_notify(D* t) {
  if constexpr (present(d.notify)) {
    if constexpr (entry_arity(d.notify) == 0)
      call<d.notify>(t);
    else
      call<d.notify>(t, member_ref<d.member>(t));
  }
}

template <class T, prop_desc d, class D>
void write_property(D* t, void* v) {
  auto& value = *static_cast<T*>(v);
  if constexpr (present(d.write)) {
    call<d.write>(t, value);
  } else if constexpr (present(d.member) && d.writable) {
    if (QtMocHelpers::setProperty(member_ref<d.member>(t), value)) emit_notify<d>(t);
  }
}

template <prop_desc d, class D>
void reset_property(D* t) {
  if constexpr (present(d.reset)) call<d.reset>(t);
}

template <class T>
int register_property_type() {
  if constexpr (named_type_v<T>)
    return qRegisterMetaType<T>();
  else
    return -1;
}

template <class D>
void property_call(D* t, QMetaObject::Call c, int id, void** a) {
  if (c == QMetaObject::RegisterPropertyMetaType) *static_cast<int*>(a[0]) = -1;
  with_index<property_count<D>>(id, [&](auto ic) {
    constexpr prop_desc d = property_at<D>(decltype(ic)::value);
    using T = typename[:d.type:];
    switch (c) {
      case QMetaObject::ReadProperty: read_property<T, D, d>(t, a[0]); break;
      case QMetaObject::WriteProperty: write_property<T, d>(t, a[0]); break;
      case QMetaObject::ResetProperty: reset_property<d>(t); break;
      case QMetaObject::RegisterPropertyMetaType: *static_cast<int*>(a[0]) = register_property_type<T>(); break;
      default: break;
    }
  });
}

template <class D>
void static_metacall(QObject* o, QMetaObject::Call c, int id, void** a) {
  auto* t = static_cast<D*>(o);
  switch (c) {
    case QMetaObject::InvokeMetaMethod: invoke_method<D>(t, id, a); break;
    case QMetaObject::IndexOfMethod: index_of_method<D>(a); break;
    case QMetaObject::RegisterMethodArgumentMetaType: register_method_argument<D>(id, a); break;
    case QMetaObject::ReadProperty:
    case QMetaObject::WriteProperty:
    case QMetaObject::ResetProperty:
    case QMetaObject::RegisterPropertyMetaType: property_call<D>(t, c, id, a); break;
    default: break;
  }
}

}  // namespace detail
}  // namespace rqt
