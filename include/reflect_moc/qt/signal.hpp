// rqt::signal<void(A...)>: a signal declared as a bodyless data member,
//   rqt::signal<void(int)> valueChanged;
// The type is the same for every member of one signature and has external linkage, so a class
// defined in a header is one type in every translation unit. The member finds its owner and its
// identity at run time, from two things RQT_OBJECT provides:
//   - a hidden first data member, rqt_anchor_, whose constructor computes the owner pointer
//     (its own address minus its offset, the offset from reflection) and publishes it;
//   - the member's own address: its offset in the owner picks the signal from a table built by
//     reflection, so two members of one signature stay different signals.
// Each signal's default constructor runs after the anchor, in member order, and keeps the owner.
//
// rqt::static_signal<void(A...)> (below) is the opt-in form that takes no space in the object: a static
// data member whose address is its identity, fired with `emit sig(args);` or `sig(args).from(obj);`.
#pragma once

#include "object.hpp"

#include <QtCore/QObject>
#include <QtCore/qobjectdefs.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <tuple>

namespace rqt {
namespace detail {

// The owner under construction. A class's anchor publishes it; the signals declared after the
// anchor read it. The tag names the class, so a signal declared where no anchor ran is caught.
struct published_owner_slot {
  QObject* object = nullptr;
  void const* tag = nullptr;
};
inline thread_local published_owner_slot published_owner;

template <info Owner>
inline constexpr char owner_tag = 0;  // its address identifies the class

template <info Owner, class Self>
consteval info member_of_type() {
  for (auto m : meta::nonstatic_data_members_of(Owner, unchecked))
    if (meta::remove_cvref(meta::type_of(m)) == ^^Self) return m;
  throw meta::exception("rqt: member not found in its owner", Owner);
}

[[noreturn]] inline void signal_without_owner(char const* what) {
  std::fprintf(stderr,
               "rqt::signal: %s. Put RQT_OBJECT first in the class, before any signal, and do not declare an "
               "rqt class as a member between RQT_OBJECT and a signal.\n",
               what);
  std::abort();
}

template <info Owner>
QObject* take_owner() {
  if (published_owner.tag != &owner_tag<Owner>) signal_without_owner("a signal was constructed with no owner");
  return published_owner.object;
}

struct signal_offset {
  std::size_t offset;
  int index;
};

consteval std::vector<signal_offset> make_signal_offsets(info cls) {
  std::vector<signal_offset> out;
  auto const entries = make_method_entries(cls);
  for (std::size_t i = 0; i < entries.size(); ++i)
    if (is_data_signal(entries[i].fn) && !is_static_signal(entries[i].fn))  // a static signal has no offset
      out.push_back({.offset = static_cast<std::size_t>(meta::offset_of(entries[i].fn).bytes),
                     .index = static_cast<int>(i)});
  return out;
}

template <info Owner>
inline constexpr auto signal_offsets = std::define_static_array(make_signal_offsets(Owner));

// The method-table index of the signal member that lives `offset` bytes into the owner.
template <info Owner>
int signal_index_at(std::size_t offset) {
  for (auto const& e : signal_offsets<Owner>)
    if (e.offset == offset) return e.index;
  signal_without_owner("a signal is not a member of its owner");
}

consteval std::vector<info> make_static_signals(info owner) {
  std::vector<info> out;
  for (auto m : meta::members_of(owner, unchecked))
    if (is_static_signal(m)) out.push_back(m);
  return out;
}
template <info Owner>
inline constexpr auto static_signals = std::define_static_array(make_static_signals(Owner));

// The method-table index of the static signal I of Owner, if the object at `p` is that signal.
template <info Owner, std::size_t I>
int static_index_if(void const* p) {
  constexpr info m = static_signals<Owner>[I];
  return p == static_cast<void const*>(&[:m:]) ? entry_index(Owner, m) : -1;
}

// The method-table index of the static signal object at `p`: its address picks it from Owner's static members.
template <info Owner>
int static_signal_index(void const* p) {
  return [&]<std::size_t... I>(std::index_sequence<I...>) {
    int found = -1;
    ((found = found >= 0 ? found : static_index_if<Owner, I>(p)), ...);
    return found;
  }(std::make_index_sequence<static_signals<Owner>.size()>{});
}

}  // namespace detail

// RQT_OBJECT declares one of these first in the class.
template <meta::info Owner = meta::current_class()>
struct owner_anchor {
  owner_anchor() {
    using O = typename[:Owner:];
    constexpr meta::info self = detail::member_of_type<Owner, owner_anchor>();
    constexpr std::size_t offset = meta::offset_of(self).bytes;
    O* const owner = reinterpret_cast<O*>(const_cast<char*>(reinterpret_cast<char const*>(this)) - offset);
    detail::published_owner = {static_cast<QObject*>(owner), &detail::owner_tag<Owner>};
  }
  owner_anchor(owner_anchor const&) = delete;
  owner_anchor& operator=(owner_anchor const&) = delete;
};

template <class... A, meta::info Owner>
struct signal<void(A...), Owner> {
  using signature = void(A...);
  using args_tuple = std::tuple<A...>;

  signal() : owner_(detail::take_owner<Owner>()) {
    static_assert(detail::declares_member(Owner, "rqt_anchor_"),
                  "rqt::signal needs RQT_OBJECT, first in the class, before any signal");
  }
  signal(signal const&) = delete;
  signal& operator=(signal const&) = delete;

  void operator()(A... a) const {
    using O = typename[:Owner:];
    O* const owner = static_cast<O*>(owner_);
    int const index = detail::signal_index_at<Owner>(
        static_cast<std::size_t>(reinterpret_cast<char const*>(this) - reinterpret_cast<char const*>(owner)));
    void* args[] = {nullptr, const_cast<void*>(static_cast<void const*>(std::addressof(a)))...};
    QMetaObject::activate(owner_, &static_meta_object<O>, index, args);
  }

 private:
  QObject* owner_;
};

// What `sig(args)` returns: the signal and its arguments, with no side effect. Fire it with
// `emit sig(args);` (the compat macro) or `sig(args).from(object);`. [[nodiscard]]: a bare `sig(args);`
// is a warning, so forgetting `emit` does not compile under -Werror.
template <class... A, meta::info Owner>
struct [[nodiscard("a signal call only builds the emission: write `emit sig(args);` or `sig(args).from(obj);`")]]
    pending<void(A...), Owner> {
  static_signal<void(A...), Owner> const* sig;
  std::tuple<A...> args;

  template <class O>
  void from(O* object) && {
    static_assert(!std::is_const_v<O>,
                  "rqt: a signal is a non-const member: it cannot be emitted from a const member function or "
                  "through a const pointer");
    using Ow = typename[:Owner:];
    static_assert(std::is_base_of_v<Ow, O>, "rqt: this signal belongs to another class than the object that emits it");
    int const index = detail::static_signal_index<Owner>(sig);
    std::apply(
        [&](auto&... a) {
          void* argv[] = {nullptr, const_cast<void*>(static_cast<void const*>(std::addressof(a)))...};
          QMetaObject::activate(static_cast<QObject*>(static_cast<Ow*>(object)), &static_meta_object<Ow>, index, argv);
        },
        args);
  }
};

// `emit` (compat.hpp) expands to `rqt::emitter{this},`: the comma operator fires a pending emission,
// and a real Qt signal (a void call) still works through the built-in comma.
template <class O>
struct emitter {
  O* object;
  constexpr explicit emitter(O* o) : object(o) {}
};
template <class O>
emitter(O*) -> emitter<O>;
template <class O, class... A, meta::info Owner>
void operator,(emitter<O> e, pending<void(A...), Owner>&& p) {
  std::move(p).from(e.object);
}

// A signal that takes no space in the object: `static inline rqt::static_signal<void(int)> s{};` (it must
// be `static inline` and non-const: Qt reads its address through a non-const pointer).
template <class... A, meta::info Owner>
struct static_signal<void(A...), Owner> {
  using signature = void(A...);
  using args_tuple = std::tuple<A...>;
  constexpr static_signal() = default;

  pending<void(A...), Owner> operator()(A... a) const { return {this, {std::move(a)...}}; }
};

}  // namespace rqt

// Teach Qt's own connect, QMetaMethod::fromSignal and QSignalSpy about signal data members. QtPrivate is
// Qt's internal namespace: this specialization follows FunctionPointer as it is in Qt 6.10.
namespace QtPrivate {
template <class Obj, class... A, std::meta::info Owner>
struct FunctionPointer<rqt::signal<void(A...), Owner> Obj::*> {
  using Object = Obj;
  using Arguments = List<A...>;
  using ReturnType = void;
  using Function = rqt::signal<void(A...), Owner> Obj::*;
  enum { ArgumentCount = sizeof...(A), IsPointerToMemberFunction = true };
  // A signal used as a slot: (o->*f)(args...) on a pointer to a callable data member calls
  // signal::operator(), which emits. Qt passes the receiver as a plain QObject* for anything that is
  // not a pointer to member function (qobjectdefs_impl.h, QCallableObject::impl), so take QObject*
  // and cast. The arguments are read with this signal's own types, a prefix of the sender's:
  // arg[0] is the return slot, the arguments start at arg[1].
  template <typename SignalArgs, typename R>
  static void call(Function f, QObject* receiver, void** arg) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (static_cast<Obj*>(receiver)->*f)(
          *reinterpret_cast<std::remove_reference_t<std::tuple_element_t<I, std::tuple<A...>>>*>(arg[I + 1])...);
    }(std::index_sequence_for<A...>{});
  }
};
}  // namespace QtPrivate

namespace QtPrivate {
// A pointer to a STATIC signal object: the class is not in the pointer type, so the signal type carries it.
template <class... A, std::meta::info Owner>
struct FunctionPointer<rqt::static_signal<void(A...), Owner>*> {
  using Object = typename[:Owner:];
  using Arguments = List<A...>;
  using ReturnType = void;
  using Function = rqt::static_signal<void(A...), Owner>*;
  enum { ArgumentCount = sizeof...(A), IsPointerToMemberFunction = true };
  // used as a slot: emit the receiver's signal
  template <typename SignalArgs, typename R>
  static void call(Function f, QObject* receiver, void** arg) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (*f)(*reinterpret_cast<std::remove_reference_t<std::tuple_element_t<I, std::tuple<A...>>>*>(arg[I + 1])...)
          .from(static_cast<Object*>(receiver));
    }(std::index_sequence_for<A...>{});
  }
};
}  // namespace QtPrivate
