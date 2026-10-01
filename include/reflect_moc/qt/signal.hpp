// rqt::signal<void(A...)>: a signal declared as a bodyless data member,
//   rqt::signal<void(int)> valueChanged;
// The member finds its owner and its own identity at compile time. Owner is the class being
// defined (the default template argument std::meta::current_class()); the closure in Tag gives
// every member declaration its own type. operator() is instantiated late, finds the member of
// Owner whose type is its own, and recovers the owner as `this` minus the member's offset.
#pragma once

#include "object.hpp"

#include <QtCore/QObject>
#include <QtCore/qobjectdefs.h>

#include <cstddef>
#include <memory>
#include <tuple>

namespace rqt {
namespace detail {

template <info Owner, class Self>
consteval info signal_member() {
  for (auto m : meta::nonstatic_data_members_of(Owner, unchecked))
    if (meta::remove_cvref(meta::type_of(m)) == ^^Self) return m;
  throw meta::exception("rqt::signal member not found in its owner", Owner);
}

}  // namespace detail

template <class... A, meta::info Owner, auto Tag>
struct signal<void(A...), Owner, Tag> {
  using signature = void(A...);
  using args_tuple = std::tuple<A...>;

  void operator()(A... a) const {
    constexpr meta::info member = detail::signal_member<Owner, signal>();
    using O = typename[:Owner:];
    constexpr std::size_t offset = meta::offset_of(member).bytes;
    O* owner = reinterpret_cast<O*>(const_cast<char*>(reinterpret_cast<char const*>(this)) - offset);
    constexpr int index = detail::entry_index(Owner, member);
    void* args[] = {nullptr, const_cast<void*>(static_cast<void const*>(std::addressof(a)))...};
    QMetaObject::activate(owner, &static_meta_object<O>, index, args);
  }
};

}  // namespace rqt

// Teach Qt's own connect, QMetaMethod::fromSignal and QSignalSpy about signal data members. QtPrivate is
// Qt's internal namespace: this specialization follows FunctionPointer as it is in Qt 6.10.
namespace QtPrivate {
template <class Obj, class... A, std::meta::info Owner, auto Tag>
struct FunctionPointer<rqt::signal<void(A...), Owner, Tag> Obj::*> {
  using Object = Obj;
  using Arguments = List<A...>;
  using ReturnType = void;
  using Function = rqt::signal<void(A...), Owner, Tag> Obj::*;
  enum { ArgumentCount = sizeof...(A), IsPointerToMemberFunction = true };
  template <typename SignalArgs, typename R>
  static void call(Function, Obj*, void**) {}
};
}  // namespace QtPrivate
