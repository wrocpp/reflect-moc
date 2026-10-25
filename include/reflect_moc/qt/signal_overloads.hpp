// A signal data member that carries several signatures:
//   [[=rqt::names("value", "text")]] rqt::signal<void(int), void(const QString&)> valueChanged;
// one member and one set of signatures, where function overloads need one function each (a data member cannot be overloaded:
// two members cannot share a name). The meta-object gets one row per signature, in declaration order, as moc orders
// overloads; the call operator is overloaded, so `valueChanged(7)` and `valueChanged(QString{"x"})` each emit their own row.
// Up to four signatures. The first is `args_tuple`, the others `args_tuple_1` to `args_tuple_3`, which the tables read.
//
// A pointer to such a member names no single signal, so Qt's connect cannot take it, as it cannot take a pointer to an overloaded
// function; rqt::overload picks the row, the way qOverload does:
//   QObject::connect(&o, rqt::overload<void(const QString&)>(&C::valueChanged), &receiver, &Receiver::slot);
//   rqt::connect(&o, rqt::overload<void(int)>(&C::valueChanged), &receiver, &Receiver::setValue);
// Strings, QMetaMethod::indexOfSignal and QSignalSpy by signature work as for any signal. Not supported: a static_signal with
// several signatures (its identity is its address, one object per signal).
#pragma once

#include "signal.hpp"
#include "connect.hpp"

#include <QtCore/QObject>
#include <QtCore/qobjectdefs.h>

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace rqt {
namespace detail {

// The call operator of one row; the set derives from one of these per signature. A row nobody declared (void) has an operator that
// cannot be called, so the using-declarations of the set stay well formed.
struct unused_row_argument {
  unused_row_argument() = delete;
};

template <class Set, std::size_t Row, class Sig>
struct signal_row;

template <class Set, std::size_t Row, class... A>
struct signal_row<Set, Row, void(A...)> {
  void operator()(A... a) const { static_cast<Set const*>(this)->template emit_row<Row>(a...); }
};

template <class Set, std::size_t Row>
struct signal_row<Set, Row, void> {
  void operator()(unused_row_argument) const = delete;
};

template <class S>
struct row_args {
  using type = void;
};
template <class... A>
struct row_args<void(A...)> {
  using type = std::tuple<A...>;
};

template <class... S>
inline constexpr bool all_distinct_signatures = true;
template <class S, class... More>
inline constexpr bool all_distinct_signatures<S, More...> =
    (std::is_void_v<S> || (!std::is_same_v<S, More> && ...)) && all_distinct_signatures<More...>;

}  // namespace detail

template <class S0, class S1, class S2, class S3, meta::info Owner>
  requires(!std::is_void_v<S1>)
struct signal<S0, S1, S2, S3, Owner> : detail::signal_row<signal<S0, S1, S2, S3, Owner>, 0, S0>,
                                       detail::signal_row<signal<S0, S1, S2, S3, Owner>, 1, S1>,
                                       detail::signal_row<signal<S0, S1, S2, S3, Owner>, 2, S2>,
                                       detail::signal_row<signal<S0, S1, S2, S3, Owner>, 3, S3> {
  using signature = S0;
  using args_tuple = typename detail::row_args<S0>::type;
  using args_tuple_1 = typename detail::row_args<S1>::type;
  using args_tuple_2 = typename detail::row_args<S2>::type;
  using args_tuple_3 = typename detail::row_args<S3>::type;

  signal() : owner_(detail::take_owner<Owner>()) {
    static_assert(detail::declares_member(Owner, "rqt_anchor_"),
                  "rqt::signal needs RQT_OBJECT, first in the class, before any signal");
    static_assert(detail::all_distinct_signatures<S0, S1, S2, S3>, "rqt::signal: two signatures of one member are the same type");
  }
  signal(signal const&) = delete;
  signal& operator=(signal const&) = delete;

  using detail::signal_row<signal, 0, S0>::operator();
  using detail::signal_row<signal, 1, S1>::operator();
  using detail::signal_row<signal, 2, S2>::operator();
  using detail::signal_row<signal, 3, S3>::operator();

  // Emits row `Row`: the member's first row index in the table plus Row (the rows of one member are consecutive).
  template <std::size_t Row, class... A>
  void emit_row(A&... a) const {
    using O = typename[:Owner:];
    O* const owner = static_cast<O*>(owner_);
    int const first = detail::signal_index_at<Owner>(
        static_cast<std::size_t>(reinterpret_cast<char const*>(this) - reinterpret_cast<char const*>(owner)));
    void* args[] = {nullptr, const_cast<void*>(static_cast<void const*>(std::addressof(a)))...};
    QMetaObject::activate(owner_, &static_meta_object<O>, first + static_cast<int>(Row), args);
  }

 private:
  QObject* owner_;
};

// A member and the row of one of its signatures.
template <class Obj, class SetT, class... A, std::size_t Row>
struct signal_selection<Obj, SetT, void(A...), Row> {
  SetT Obj::* member;
  std::size_t row;  // the same as Row, kept as a value so that Qt can compare two selections of one layout
  friend bool operator==(signal_selection const&, signal_selection const&) = default;
};

namespace detail {

template <class Sig, class S0, class S1, class S2, class S3>
consteval std::size_t row_of_signature() {
  constexpr bool is[] = {std::is_same_v<Sig, S0>, std::is_same_v<Sig, S1>, std::is_same_v<Sig, S2>, std::is_same_v<Sig, S3>};
  std::size_t found = 4;
  for (std::size_t i = 0; i < 4; ++i)
    if (is[i] && found == 4) found = i;
  return found;
}

template <class Set>
struct set_signatures;
template <class S0, class S1, class S2, class S3, meta::info Owner>
struct set_signatures<signal<S0, S1, S2, S3, Owner>> {
  template <class Sig>
  static constexpr std::size_t row = row_of_signature<Sig, S0, S1, S2, S3>();
};

// The table index of the row a selection names, relative to the class's own methods.
template <class C, class Sel>
int selection_index(Sel const& selection) {
  return [&]<std::size_t... I>(std::index_sequence<I...>) {
    int found = -1;
    (
        [&] {
          constexpr method_entry e = method_entries<C>[I];
          if constexpr (!e.cloned && is_data_signal(e.fn) && !is_static_signal(e.fn))
            if constexpr (std::is_same_v<decltype(entry_pointer<e.fn>()), decltype(selection.member)>)
              if (found < 0 && e.sig == selection.row && entry_pointer<e.fn>() == selection.member) found = static_cast<int>(I);
        }(),
        ...);
    return found < 0 ? -1 : static_meta_object<C>.methodOffset() + found;
  }(std::make_index_sequence<method_total<C>>{});
}

}  // namespace detail

// rqt::overload<void(const QString&)>(&C::valueChanged): the pointer to a multi-signature member with the signature chosen.
template <class Sig, class Obj, class SetT>
constexpr auto overload(SetT Obj::* member) {
  constexpr std::size_t row = detail::set_signatures<SetT>::template row<Sig>;
  static_assert(row < 4, "rqt::overload: the member has no signal of that signature");
  return signal_selection<Obj, SetT, Sig, row>{member, row};
}

// Signal to slot by index, for a selected signal (the signal is given by rqt::overload).
template <class Obj, class SetT, class Sig, std::size_t Row, class R, class RF>
QMetaObject::Connection connect(QObject const* sender, signal_selection<Obj, SetT, Sig, Row> signal, QObject const* receiver,
                                RF R::* slot, Qt::ConnectionType type = Qt::AutoConnection) {
  static_assert(detail::args_prefix<Sig, typename detail::strip_const_fn<RF>::type>::value,
                "rqt::connect: the slot's parameters must be a prefix of the signal's");
  return QMetaObject::connect(sender, detail::selection_index<Obj>(signal), receiver, detail::method_index(slot), type);
}

}  // namespace rqt

namespace QtPrivate {
// A selected signal as Qt's connect, QMetaMethod::fromSignal and QSignalSpy take it, like a pointer to member function.
template <class Obj, class SetT, class... A, std::size_t Row>
struct FunctionPointer<rqt::signal_selection<Obj, SetT, void(A...), Row>> {
  using Object = Obj;
  using Arguments = List<A...>;
  using ReturnType = void;
  using Function = rqt::signal_selection<Obj, SetT, void(A...), Row>;
  enum { ArgumentCount = sizeof...(A), IsPointerToMemberFunction = true };
  // used as a slot: the receiver's own signal is emitted, the overload of these argument types
  template <typename SignalArgs, typename R>
  static void call(Function f, QObject* receiver, void** arg) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (static_cast<Obj*>(receiver)->*f.member)(
          *reinterpret_cast<std::remove_reference_t<std::tuple_element_t<I, std::tuple<A...>>>*>(arg[I + 1])...);
    }(std::index_sequence_for<A...>{});
  }
};
}  // namespace QtPrivate
