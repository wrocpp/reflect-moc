// qt_static_metacall for a reflected class: invoke, index-of-method, property
// read, write and reset, and the metatype registration calls.
#pragma once

#include "metadata.hpp"

#include <QtCore/QMetaObject>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QProperty>

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
  if constexpr (has_private_tag(Fn) && sizeof...(A) == entry_arity(Fn)) {
    // moc passes QPrivateSignal() too: the class's own tag, which only the class (and so this call) can make
    return call<Fn>(self, std::forward<A>(a)..., entry_param_t<Fn, entry_arity(Fn)>{});
  } else if constexpr (is_static_signal(Fn)) {
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

template <info Fn, std::size_t P, std::size_t S = 0>
using param_t = std::remove_cvref_t<entry_param_t<Fn, P, S>>;

template <info Fn, std::size_t P, std::size_t S = 0>
decltype(auto) arg_at(void** args) {
  return *static_cast<param_t<Fn, P, S>*>(args[P + 1]);
}

// args[0] is the return slot, args[1..] point at the arguments. S is the signature of a signal member that carries several.
template <info Fn, std::size_t N, class C, std::size_t S = 0>
void call_with_args(C* self, void** args) {
  [&]<std::size_t... P>(std::index_sequence<P...>) {
    using R = entry_return_t<Fn>;
    if constexpr (std::is_void_v<R>) {
      call<Fn>(self, arg_at<Fn, P, S>(args)...);
    } else {
      std::remove_cvref_t<R> r = call<Fn>(self, arg_at<Fn, P, S>(args)...);
      if (args[0]) *static_cast<std::remove_cvref_t<R>*>(args[0]) = std::move(r);
    }
  }(std::make_index_sequence<N>{});
}

template <class D>
void invoke_method(D* t, int id, void** a) {
  with_index<method_total<D>>(id, [&](auto ic) {
    constexpr method_entry e = method_entries<D>[decltype(ic)::value];
    call_with_args<e.fn, e.nargs, D, e.sig>(t, a);
  });
}

// --- IndexOfMethod -----------------------------------------------------------------------

template <class D, std::size_t I>
bool match_signal(void** a) {
  constexpr method_entry e = method_entries<D>[I];
  if constexpr (e.cloned || kind_of(e.fn) != method_kind::signal_)
    return false;
  else if constexpr (is_data_signal(e.fn) && signal_signature_count(e.fn) > 1) {
    // a member with several signatures is named through rqt::overload, which carries the row (signal_overloads.hpp)
    using Set = std::remove_cvref_t<typename[:meta::type_of(e.fn):]>;
    using Selection = signal_selection<typename[:meta::parent_of(e.fn):], Set, signature_t<e.fn, e.nargs, e.sig>, e.sig>;
    return meta::is_public(e.fn) && QtMocHelpers::indexOfMethod<Selection>(a, Selection{&[:e.fn:], e.sig}, static_cast<int>(I));
  } else if constexpr (is_data_signal(e.fn))  // a private signal member has no pointer a caller could name
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

template <info Fn, std::size_t N, std::size_t S = 0>
void register_argument(void** a) {
  auto* out = static_cast<QMetaType*>(a[0]);
  int const wanted = *static_cast<int*>(a[1]);
  [&]<std::size_t... P>(std::index_sequence<P...>) {
    (((wanted == static_cast<int>(P) && named_type_v<param_t<Fn, P, S>>)
          ? (*out = QMetaType::fromType<param_t<Fn, P, S>>(), true)
          : false) ||
     ...);
  }(std::make_index_sequence<N>{});
}

template <class D>
void register_method_argument(int id, void** a) {
  *static_cast<QMetaType*>(a[0]) = QMetaType();
  with_index<method_total<D>>(id, [&](auto ic) {
    constexpr method_entry e = method_entries<D>[decltype(ic)::value];
    if constexpr (e.nargs > 0) register_argument<e.fn, e.nargs, e.sig>(a);
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
    if constexpr (entry_arity(d.notify, d.notify_sig) == 0)
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

// moc: `*static_cast<QUntypedBindable*>(_a[0]) = _t->bindableX();`
template <prop_desc d, class D>
void bind_property(D* t, void* v) {
  if constexpr (present(d.bindable)) *static_cast<QUntypedBindable*>(v) = call<d.bindable>(t);
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
      case QMetaObject::BindableProperty: bind_property<d>(t, a[0]); break;
      case QMetaObject::RegisterPropertyMetaType: *static_cast<int*>(a[0]) = register_property_type<T>(); break;
      default: break;
    }
  });
}

template <class D>
void static_metacall(QObject* o, QMetaObject::Call c, int id, void** a) {
  // Qt hands a gadget's address over as a QObject*: it is not one, so the pointer is reinterpreted.
  auto* t = [o] {
    if constexpr (std::is_base_of_v<QObject, D>)
      return static_cast<D*>(o);
    else
      return reinterpret_cast<D*>(o);
  }();
  switch (c) {
    case QMetaObject::InvokeMetaMethod: invoke_method<D>(t, id, a); break;
    case QMetaObject::IndexOfMethod: index_of_method<D>(a); break;
    case QMetaObject::RegisterMethodArgumentMetaType: register_method_argument<D>(id, a); break;
    case QMetaObject::ReadProperty:
    case QMetaObject::WriteProperty:
    case QMetaObject::ResetProperty:
    case QMetaObject::BindableProperty:
    case QMetaObject::RegisterPropertyMetaType: property_call<D>(t, c, id, a); break;
    default: break;
  }
}

}  // namespace detail
}  // namespace rqt
