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
    if (is_data_signal(entries[i].fn))
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
  template <typename SignalArgs, typename R>
  static void call(Function, Obj*, void**) {}
};
}  // namespace QtPrivate
