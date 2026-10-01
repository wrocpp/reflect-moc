// Spike 05: two ways to declare a signal.
//
// (a) an annotated member function whose body emits without naming itself:
//       [[=rqt_a::signal]] void clicked(int x) { rqt_a::emit{this}(x); }
//     The signal's identity comes from std::meta::current_function(), read in
//     the default argument of a consteval constructor.
// (b) a static descriptor object:
//       static constexpr rqt_b::signal<int> valueChanged{};
//     found by members_of + is_variable + template_of(type) == ^^rqt_b::signal.
//     Its identity comes from its address, or from reflect_object.
//
// For both: a compile-time signal table in declaration order.
//
// The two forms live in separate namespaces because one name cannot be both
// the annotation object (form a) and the descriptor class template (form b).
//
// Build:  g++-16 -std=c++26 -freflection -Wall -Wextra spikes/05-signal-forms.cpp
// Probe:  -DSPIKE_EMIT_OUTSIDE (emit from a function that is not a signal)
#include <meta>

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <map>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace meta = std::meta;

#if defined(__clang__)
// clang-p2996 has no std::meta::exception. Calling a non-constexpr function
// also stops constant evaluation, with a less readable diagnostic.
inline void reflection_error(std::string_view) {}
#define SPIKE_THROW(msg, r) (reflection_error(msg), throw 0)
// Nor std::meta::current_function(), so form (a) cannot be built there.
#define SPIKE_FORM_A 0
#else
#define SPIKE_THROW(msg, r) throw meta::exception(msg, r)
#define SPIKE_FORM_A 1
#endif

// --- a toy runtime: (object, signal index) -> connected slots ---------------
namespace rt {
using slot_fn = std::function<void(void**)>;
inline std::map<std::pair<void const*, int>, std::vector<slot_fn>> connections;
inline void activate(void const* object, int index, void** args) {
  if (auto it = connections.find({object, index}); it != connections.end())
    for (auto& f : it->second) f(args);
}
}  // namespace rt

struct signal_record {
  char const* name;
  std::size_t arity;
};

// --- form (a) ----------------------------------------------------------------
#if SPIKE_FORM_A
namespace rqt_a {
struct signal_t {};
inline constexpr signal_t signal{};

consteval bool is_signal(meta::info f) {
  for (auto a : meta::annotations_of(f))
    if (meta::remove_cvref(meta::type_of(a)) == ^^signal_t) return true;
  return false;
}

consteval std::vector<meta::info> signals_in(meta::info cls) {
  std::vector<meta::info> out;
  for (auto m : meta::members_of(cls, meta::access_context::unchecked()))
    if (meta::is_function(m) && is_signal(m)) out.push_back(m);
  return out;
}

consteval int signal_index(meta::info f) {
  if (meta::is_function(f) && meta::is_class_member(f)) {
    auto all = signals_in(meta::parent_of(f));
    for (std::size_t i = 0; i < all.size(); ++i)
      if (all[i] == f) return static_cast<int>(i);
  }
  SPIKE_THROW("rqt::emit used outside a [[=rqt::signal]] member function", f);
}

// Built at compile time from the caller's reflection; holds only runtime-safe data.
struct signal_id {
  int index;
  char const* name;
  consteval signal_id(meta::info caller = meta::current_function())
      : index(signal_index(caller)), name(std::define_static_string(meta::identifier_of(caller))) {}
};

// The name the last emit resolved to, so main can check current_function()
// saw the signal and not emit's constructor.
inline char const* last_emit_name = nullptr;

template <class C>
struct emit {
  C* self;
  signal_id id;
  emit(C* s, signal_id i = {}) : self(s), id(i) {}
  template <class... A>
  void operator()(A const&... a) const {
    last_emit_name = id.name;
    void* args[] = {nullptr, const_cast<void*>(static_cast<void const*>(&a))...};
    rt::activate(self, id.index, args);
  }
};

template <meta::info Sig, class C, class F>
void connect(C* object, F f) {
  rt::connections[{object, signal_index(Sig)}].push_back([f](void** a) mutable {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      f(*static_cast<typename[:meta::remove_cvref(meta::type_of(meta::parameters_of(Sig)[I])):]*>(a[I + 1])...);
    }(std::make_index_sequence<meta::parameters_of(Sig).size()>{});
  });
}

template <class C>
consteval std::span<signal_record const> signal_table() {
  std::vector<signal_record> v;
  for (auto s : signals_in(^^C))
    v.push_back({std::define_static_string(meta::identifier_of(s)), meta::parameters_of(s).size()});
  return std::define_static_array(v);
}
}  // namespace rqt_a

struct Button {
  [[= rqt_a::signal]] void clicked(int x) { rqt_a::emit{this}(x); }
  void press() { clicked(3); }
  [[= rqt_a::signal]] void released() { rqt_a::emit{this}(); }
  [[= rqt_a::signal]] void renamed(char const* to) { rqt_a::emit{this}(to); }
#ifdef SPIKE_EMIT_OUTSIDE
  void not_a_signal() { rqt_a::emit{this}(1); }
#endif
};
#endif  // SPIKE_FORM_A

// --- form (b) ----------------------------------------------------------------
namespace rqt_b {
template <class... A>
struct signal;

consteval bool is_signal(meta::info m) {
  if (!meta::is_variable(m)) return false;
  auto t = meta::remove_cvref(meta::type_of(m));
  return meta::has_template_arguments(t) && meta::template_of(t) == ^^signal;
}

consteval std::vector<meta::info> signals_in(meta::info cls) {
  std::vector<meta::info> out;
  for (auto m : meta::members_of(cls, meta::access_context::unchecked()))
    if (is_signal(m)) out.push_back(m);
  return out;
}

// Identity by reflection of the object.
template <class C>
consteval int index_of_object(meta::info object) {
  auto all = signals_in(^^C);
  for (std::size_t i = 0; i < all.size(); ++i)
    if (meta::object_of(all[i]) == object) return static_cast<int>(i);
  SPIKE_THROW("not a signal of this class", object);
}

// Identity by address, resolved at run time.
template <class C>
int index_of_address(void const* p) {
  constexpr std::size_t n = signals_in(^^C).size();
  return [p]<std::size_t... I>(std::index_sequence<I...>) {
    int r = -1;
    ((static_cast<void const*>(&[:signals_in(^^C)[I]:]) == p ? (r = static_cast<int>(I), true) : false) || ...);
    return r;
  }(std::make_index_sequence<n>{});
}

template <class... A>
struct signal {
  template <class C>
  void emit(C* self, A const&... a) const {
    void* args[] = {nullptr, const_cast<void*>(static_cast<void const*>(&a))...};
    rt::activate(self, index_of_address<C>(this), args);
  }
};

template <auto& Sig, class C, class... A>
void emit(C* self, A const&... a) {
  constexpr int index = index_of_object<C>(meta::reflect_object(Sig));
  void* args[] = {nullptr, const_cast<void*>(static_cast<void const*>(&a))...};
  rt::activate(self, index, args);
}

template <class... A, class F>
void invoke_unpacked(F& f, void** a, signal<A...> const*) {
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    f(*static_cast<A*>(a[I + 1])...);
  }(std::index_sequence_for<A...>{});
}

template <auto& Sig, class C, class F>
void connect(C* object, F f) {
  rt::connections[{object, index_of_object<C>(meta::reflect_object(Sig))}].push_back(
      [f](void** a) mutable { invoke_unpacked(f, a, &Sig); });
}

template <class C>
consteval std::span<signal_record const> signal_table() {
  std::vector<signal_record> v;
  for (auto s : signals_in(^^C))
    v.push_back({std::define_static_string(meta::identifier_of(s)),
                 meta::template_arguments_of(meta::remove_cvref(meta::type_of(s))).size()});
  return std::define_static_array(v);
}
}  // namespace rqt_b

struct Slider {
  static constexpr rqt_b::signal<int> valueChanged{};
  static constexpr int not_a_signal = 3;
  static constexpr rqt_b::signal<> released{};
  static constexpr rqt_b::signal<int, double> moved{};
  int value_ = 0;
  void setValue(int v) {
    value_ = v;
    valueChanged.emit(this, v);
  }
  void release() { rqt_b::emit<released>(this); }
  void move(int x, double y) { rqt_b::emit<moved>(this, x, y); }
};

int main() {
#if SPIKE_FORM_A
  // form (a)
  constexpr auto a_table = rqt_a::signal_table<Button>();
  static_assert(a_table.size() == 3);
  static_assert(std::string_view{a_table[0].name} == "clicked" && a_table[0].arity == 1);
  static_assert(std::string_view{a_table[1].name} == "released" && a_table[1].arity == 0);
  static_assert(std::string_view{a_table[2].name} == "renamed");

  Button b;
  int clicked_with = 0, released_count = 0;
  char const* renamed_to = nullptr;
  rqt_a::connect<^^Button::clicked>(&b, [&](int x) { clicked_with = x; });
  rqt_a::connect<^^Button::released>(&b, [&] { ++released_count; });
  rqt_a::connect<^^Button::renamed>(&b, [&](char const* s) { renamed_to = s; });
  b.press();
  assert(std::string_view{rqt_a::last_emit_name} == "clicked");
  b.released();
  assert(std::string_view{rqt_a::last_emit_name} == "released");
  b.renamed("ok");
  assert(clicked_with == 3);
  assert(released_count == 1);
  assert(std::string_view{renamed_to} == "ok");
#else
  std::puts("form (a) skipped: this compiler has no std::meta::current_function()");
#endif

  // form (b)
  constexpr auto b_table = rqt_b::signal_table<Slider>();
  static_assert(b_table.size() == 3);
  static_assert(std::string_view{b_table[0].name} == "valueChanged" && b_table[0].arity == 1);
  static_assert(std::string_view{b_table[1].name} == "released" && b_table[1].arity == 0);
  static_assert(std::string_view{b_table[2].name} == "moved" && b_table[2].arity == 2);
  static_assert(rqt_b::index_of_object<Slider>(meta::reflect_object(Slider::moved)) == 2);

  Slider s;
  int value_seen = 0, slider_released = 0;
  double moved_y = 0;
  rqt_b::connect<Slider::valueChanged>(&s, [&](int v) { value_seen = v; });
  rqt_b::connect<Slider::released>(&s, [&] { ++slider_released; });
  rqt_b::connect<Slider::moved>(&s, [&](int, double y) { moved_y = y; });
  s.setValue(12);
  s.release();
  s.move(1, 2.5);
  assert(value_seen == 12);
  assert(slider_released == 1);
  assert(moved_y == 2.5);
  assert(rqt_b::index_of_address<Slider>(&Slider::moved) == 2);

  std::puts("05 signals: (a) current_function emit and (b) static descriptor, both with ordered tables OK");
}
