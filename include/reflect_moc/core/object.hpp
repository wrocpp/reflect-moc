#pragma once

#include <reflect_moc/core/annotations.hpp>
#include <reflect_moc/core/connection.hpp>
#include <reflect_moc/core/event_loop.hpp>
#include <reflect_moc/core/fixed_string.hpp>
#include <reflect_moc/core/introspection.hpp>
#include <reflect_moc/core/slot.hpp>

#include <meta>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>

namespace rqt {

class Object;

namespace detail {

struct access {
  static std::shared_ptr<object_state> const& handle(Object const& o) noexcept;
  static object_state& state(Object const& o) noexcept { return *handle(o); }
};

template <class... A>
std::array<void const*, sizeof...(A)> argument_pointers(A const&... args) {
  return {static_cast<void const*>(std::addressof(args))...};
}

// Direct calls borrow the arguments: `args` point at the caller's values, or
// at temporaries converted to the signal's parameter types.
template <class... P>
void emit_as(Object const& sender, void const* signal, std::type_identity_t<P> const&... args) {
  activate(access::state(sender), signal, argument_pointers(args...));
}

// Named function templates, not generic lambdas: GCC 16.2 does not treat a
// lambda body that splices signal_params(S)[I] as a constant context.
template <std::meta::info S, std::size_t... I>
constexpr auto emitter_for(std::index_sequence<I...>) {
  return &emit_as<typename[:signal_params(S)[I]:]...>;
}

template <std::meta::info S, class... A>
void emit_signal(Object const& sender, A const&... args) {
  static_assert(sizeof...(A) == signal_params(S).size(), "rqt::emit: wrong number of arguments for this signal");
  constexpr void const* key = signal_key<S>();
  constexpr auto emitter = emitter_for<S>(std::make_index_sequence<signal_params(S).size()>{});
  emitter(sender, key, args...);
}

template <std::meta::info S, class F, std::size_t... I>
slot_call bind_for_signal_impl(F f, std::index_sequence<I...>) {
  return make_slot_call<F, typename[:signal_params(S)[I]:]...>(std::move(f));
}

template <std::meta::info S, class F>
slot_call bind_for_signal(F f) {
  return bind_for_signal_impl<S>(std::move(f), std::make_index_sequence<signal_params(S).size()>{});
}

template <std::meta::info S, class F>
connection connect_to(Object const& sender, receiver_token receiver, connection_type type, F f) {
  constexpr void const* key = signal_key<S>();
  // Outside the braced list: see limit_info_template_id_in_designated_initializer.
  slot_call call = bind_for_signal<S>(std::move(f));
  auto const& handle = access::handle(sender);
  return connection{handle, attach(*handle, {.id = next_connection_id(),
                                             .signal = key,
                                             .receiver = std::move(receiver),
                                             .type = type,
                                             .call = std::move(call)})};
}

// Returns false, leaving `member` untouched, when the value is equal.
template <class T, class V>
bool assign_if_changed(T& member, V&& value) {
  if constexpr (std::equality_comparable_with<T const&, V const&>) {
    if (member == value) return false;
  }
  member = std::forward<V>(value);
  return true;
}

}  // namespace detail

// The base of every class with signals, slots or properties.
//
// Not a template, no virtual functions: its member templates take an
// explicit object parameter (`this Self& self`), so reflection sees the
// most-derived class the call is made on.
//
// Every Object has a thread affinity, initially the constructing thread.
// Destroying an Object disconnects everything it sends and everything it
// receives, and drops queued deliveries still addressed to it. Destroy an
// Object on its own thread.
class Object {
 public:
  Object() : state_{std::make_shared<detail::object_state>(std::this_thread::get_id())} {}
  Object(Object const&) = delete;
  Object& operator=(Object const&) = delete;
  ~Object() {
    state_->alive.store(false);
    detail::detach_all(*state_);
  }

  std::thread::id thread() const noexcept { return state_->affinity.load(); }

  // Deliveries resolved after this call target `target`; ones already queued
  // stay on the old thread's loop.
  void move_to_thread(std::thread::id target) noexcept { state_->affinity.store(target); }

  // Connects signal D of this object to `slot` on `receiver`: a member
  // function pointer of the receiver, or any callable, which then runs in the
  // receiver's thread and dies with it.
  template <signal_ref D, class Self, class Receiver, class Slot>
    requires std::derived_from<Receiver, Object>
  connection connect(this Self& self, Receiver& receiver, Slot slot,
                     connection_type type = connection_type::automatic) {
    constexpr std::meta::info S = detail::resolve_signal<Self, D>();
    return detail::connect_to<S>(self, {.state = detail::access::handle(receiver), .bound = true}, type,
                                 detail::bind_receiver(receiver, std::move(slot)));
  }

  // Connects signal D to a callable with no receiver; it is always called
  // directly on the emitting thread.
  template <signal_ref D, class Self, class F>
  connection connect(this Self& self, F f) {
    constexpr std::meta::info S = detail::resolve_signal<Self, D>();
    return detail::connect_to<S>(self, {.state = {}, .bound = false}, connection_type::direct, std::move(f));
  }

  // Emits signal D. The arguments convert to the signal's parameter types.
  template <signal_ref D, class Self, class... A>
  void emit(this Self const& self, A const&... args) {
    constexpr std::meta::info S = detail::resolve_signal<Self, D>();
    detail::emit_signal<S>(self, args...);
  }

  // How many connections of signal D have a live receiver.
  template <signal_ref D, class Self>
  std::size_t receivers(this Self const& self) {
    constexpr std::meta::info S = detail::resolve_signal<Self, D>();
    constexpr void const* key = detail::signal_key<S>();
    return detail::live_receivers(detail::access::state(self), key);
  }

  // Assigns the [[=rqt::property]] member N. For a NOTIFY property, emits
  // N##Changed with the new value, unless the value compares equal.
  template <fixed_string N, class Self, class V>
  void set(this Self& self, V&& value) {
    constexpr std::meta::info m = detail::property_named(^^Self, N.view());
    if (!detail::assign_if_changed(self.[:m:], std::forward<V>(value))) return;
    if constexpr (detail::is_notify_property(m)) detail::emit_signal<m>(self, self.[:m:]);
  }

  template <fixed_string N, class Self>
  auto const& get(this Self const& self) {
    constexpr std::meta::info m = detail::property_named(^^Self, N.view());
    return self.[:m:];
  }

 private:
  friend struct detail::access;
  std::shared_ptr<detail::object_state> state_;
};

namespace detail {

inline std::shared_ptr<object_state> const& access::handle(Object const& o) noexcept { return o.state_; }

consteval void const* function_signal_key(std::meta::info caller) {
  if (!is_function_signal(caller))
    throw std::meta::exception("rqt::emit{this} used outside a [[=rqt::signal]] member function", caller);
  return anchored_key(caller);
}

// The identity of the form (a) signal whose body constructs it, taken from
// std::meta::current_function() at the call site.
struct signal_id {
  void const* key;
  char const* name;
  // Static storage from define_static_array, so the view never dangles.
  std::span<void const* const> params;

  consteval signal_id(std::meta::info caller = std::meta::current_function())
      : key{function_signal_key(caller)},
        name{std::define_static_string(std::meta::identifier_of(caller))},
        params{param_type_keys(caller)} {}
};

template <class... A>
bool arguments_match(signal_id const& id) {
  std::array<void const*, sizeof...(A)> const given{type_key<std::remove_cvref_t<A>>...};
  return std::ranges::equal(id.params, given);
}

[[noreturn]] inline void emit_arguments_mismatch(char const* signal) {
  std::fprintf(stderr, "rqt::emit: the arguments do not match the parameters of signal '%s'\n", signal);
  std::abort();
}

}  // namespace detail

// Form (a): the body of a [[=rqt::signal]] member function,
//   [[=rqt::signal]] void clicked(int x) { rqt::emit{this}(x); }
// The arguments must have exactly the signal's parameter types (after
// remove_cvref). That is checked at run time, and a mismatch aborts: see
// limit_function_form_argument_types_checked_at_run_time.
template <class C>
struct emit {
  C* sender;
  detail::signal_id id;

  emit(C* s, detail::signal_id i = {}) : sender{s}, id{i} {}

  template <class... A>
  void operator()(A const&... args) const {
    static_assert(std::derived_from<std::remove_const_t<C>, Object>, "rqt::emit needs a class derived from rqt::Object");
    if (!detail::arguments_match<A...>(id)) detail::emit_arguments_mismatch(id.name);
    detail::activate(detail::access::state(*sender), id.key, detail::argument_pointers(args...));
  }
};

// Form (b): a static descriptor,
//   static constexpr rqt::signal_of<int> valueChanged{};
//   void setValue(int v) { valueChanged.emit(this, v); }
template <class... A>
struct signal_of {
  void emit(Object const* sender, std::type_identity_t<A> const&... args) const {
    detail::activate(detail::access::state(*sender), this, detail::argument_pointers(args...));
  }
};

// `rqt::connect<"progressChanged">(worker, ui, &Ui::onProgress)`
template <signal_ref D, class Sender, class... Rest>
connection connect(Sender& sender, Rest&&... rest) {
  return sender.template connect<D>(std::forward<Rest>(rest)...);
}

}  // namespace rqt
