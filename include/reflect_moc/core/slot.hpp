#pragma once

#include <reflect_moc/core/connection.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rqt::detail {

template <class F, class Params, std::size_t... I>
consteval bool invocable_with_prefix(std::index_sequence<I...>) {
  return std::is_invocable_v<F const&, std::tuple_element_t<I, Params> const&...>;
}

// A slot may take a prefix of the signal's arguments, as in Qt. The largest
// prefix it accepts wins; nullopt if it accepts none.
template <class F, class... P>
consteval std::optional<std::size_t> slot_arity() {
  using params = std::tuple<P...>;
  return []<std::size_t... K>(std::index_sequence<K...>) {
    std::optional<std::size_t> best;
    ((invocable_with_prefix<F, params>(std::make_index_sequence<K>{}) ? (best = K, true) : false), ...);
    return best;
  }(std::make_index_sequence<sizeof...(P) + 1>{});
}

template <class F, class Params, std::size_t... I>
void invoke_slot(F const& f, [[maybe_unused]] arg_span args, std::index_sequence<I...>) {
  f(*static_cast<std::tuple_element_t<I, Params> const*>(args[I])...);
}

// The copy into the task is the named ownership boundary: the arguments are
// handed to another thread and must outlive the emit.
template <class F, class Params, std::size_t... I>
task package_slot(std::shared_ptr<void const> const& target, [[maybe_unused]] arg_span args,
                  std::index_sequence<I...>) {
  return [target, copies = std::tuple<std::tuple_element_t<I, Params>...>{
                      *static_cast<std::tuple_element_t<I, Params> const*>(args[I])...}] {
    std::apply(*static_cast<F const*>(target.get()), copies);
  };
}

template <class F, class... P>
struct slot_thunks {
  using params = std::tuple<P...>;
  using prefix = std::make_index_sequence<*slot_arity<F, P...>()>;

  static void invoke(void const* target, arg_span args) {
    invoke_slot<F, params>(*static_cast<F const*>(target), args, prefix{});
  }
  static task package(std::shared_ptr<void const> const& target, arg_span args) {
    return package_slot<F, params>(target, args, prefix{});
  }
};

template <class F, class... P>
slot_call make_slot_call(F f) {
  static_assert(slot_arity<F, P...>().has_value(),
                "rqt::connect: the slot cannot be called with the signal's arguments (or a prefix of them)");
  return {.target = std::make_shared<F const>(std::move(f)),
          .invoke = &slot_thunks<F, P...>::invoke,
          .package = &slot_thunks<F, P...>::package};
}

// A member function slot bound to its receiver. Its liveness is guarded by
// the connection's receiver token, not by this pointer.
template <class Receiver, class Pmf>
struct member_slot {
  Receiver* receiver;
  Pmf pmf;

  template <class... A>
    requires std::is_invocable_v<Pmf const&, Receiver&, A const&...>
  void operator()(A const&... a) const {
    std::invoke(pmf, *receiver, a...);
  }
};

template <class Receiver, class Slot>
auto bind_receiver(Receiver& receiver, Slot slot) {
  if constexpr (std::is_member_function_pointer_v<Slot>)
    return member_slot<Receiver, Slot>{.receiver = &receiver, .pmf = slot};
  else
    return slot;
}

}  // namespace rqt::detail
