// Spike 03: member functions and their parameters.
//
// Question: can reflection enumerate a class's member functions, classify
// them, read return and parameter types, recover parameter NAMES (QML needs
// them for signal handler arguments), and call them qt_metacall style
// through void** args?
//
// Build:  g++-16 -std=c++26 -freflection -Wall -Wextra spikes/03-member-functions.cpp
#include <meta>

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace meta = std::meta;

#if defined(__clang__)
// clang-p2996 has no std::meta::exception. Calling a non-constexpr function
// also stops constant evaluation, with a less readable diagnostic.
inline void reflection_error(std::string_view) {}
#define SPIKE_THROW(msg, r) (reflection_error(msg), throw 0)
#else
#define SPIKE_THROW(msg, r) throw meta::exception(msg, r)
#endif

struct Counter {
  int value_ = 0;

  Counter() = default;
  Counter(Counter const&) = default;
  ~Counter() = default;

  int value() const { return value_; }
  void setValue(int v);  // the definition below names it newValue
  void rename(int);      // unnamed here, named in the definition
  double scale(double factor, int times) { return value_ * factor * times; }
  void bump() { ++value_; }
  void bump(int by) { value_ += by; }
  void step(int by = 1) { value_ += by; }
  static int instances() { return 1; }

 private:
  void hidden() {}
};

consteval meta::info fn(std::string_view name) {
  for (auto m : meta::members_of(^^Counter, meta::access_context::unchecked()))
    if (meta::is_function(m) && meta::has_identifier(m) && meta::identifier_of(m) == name) return m;
  SPIKE_THROW("no function", ^^Counter);
}

// Parameter names seen BEFORE the out-of-class definitions are reachable.
constexpr bool set_value_named_before = meta::has_identifier(meta::parameters_of(fn("setValue"))[0]);
constexpr bool rename_named_before = meta::has_identifier(meta::parameters_of(fn("rename"))[0]);

void Counter::setValue(int newValue) { value_ = newValue; }
void Counter::rename(int to) { value_ = to; }

// And AFTER.
constexpr bool set_value_named_after = meta::has_identifier(meta::parameters_of(fn("setValue"))[0]);
constexpr bool rename_named_after = meta::has_identifier(meta::parameters_of(fn("rename"))[0]);

consteval int count_functions(bool (*pred)(meta::info)) {
  int n = 0;
  for (auto m : meta::members_of(^^Counter, meta::access_context::unchecked()))
    if (meta::is_function(m) && pred(m)) ++n;
  return n;
}

// Methods moc would register: non-special, non-static member functions.
consteval bool is_method(meta::info m) {
  return !meta::is_special_member_function(m) && !meta::is_static_member(m) && !meta::is_constructor(m) &&
         !meta::is_destructor(m) && !meta::is_operator_function(m);
}

consteval std::string_view param_name(meta::info f, std::size_t i) {
  auto p = meta::parameters_of(f)[i];
  return meta::has_identifier(p) ? meta::identifier_of(p) : std::string_view{"<none>"};
}

// qt_metacall style: args[0] is the return slot, args[1..] point to the params.
template <meta::info Fn>
void call_with_args(void* object, void** args) {
  using C = [:meta::parent_of(Fn):];
  auto& self = *static_cast<C*>(object);
  constexpr std::size_t n = meta::parameters_of(Fn).size();
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    using R = [:meta::return_type_of(Fn):];
    auto pmf = &[:Fn:];
    if constexpr (std::is_void_v<R>) {
      (self.*pmf)(*static_cast<std::remove_cvref_t<typename[:meta::type_of(meta::parameters_of(Fn)[I]):]>*>(
          args[I + 1])...);
    } else {
      R r = (self.*pmf)(*static_cast<std::remove_cvref_t<typename[:meta::type_of(meta::parameters_of(Fn)[I]):]>*>(
          args[I + 1])...);
      if (args[0]) *static_cast<R*>(args[0]) = std::move(r);
    }
  }(std::make_index_sequence<n>{});
}

// The pointer-to-member itself as a non-type template argument.
template <auto Pmf>
struct pmf_holder {
  static constexpr auto value = Pmf;
};

int main() {
  // classification
  static_assert(meta::is_const(fn("value")));
  static_assert(!meta::is_const(fn("scale")));
  static_assert(meta::is_static_member(fn("instances")));
  static_assert(meta::return_type_of(fn("value")) == ^^int);
  static_assert(meta::return_type_of(fn("scale")) == ^^double);
  static_assert(meta::return_type_of(fn("setValue")) == ^^void);
  static_assert(meta::is_private(fn("hidden")));
  constexpr int specials = count_functions([](meta::info m) { return meta::is_special_member_function(m); });
  constexpr int methods = count_functions(is_method);
  std::printf("special member functions seen: %d, methods seen: %d\n", specials, methods);
  // value setValue rename scale bump bump step hidden
  static_assert(methods == 8);

  // parameter names and types
  static_assert(meta::parameters_of(fn("scale")).size() == 2);
  static_assert(param_name(fn("scale"), 0) == "factor");
  static_assert(param_name(fn("scale"), 1) == "times");
  static_assert(meta::type_of(meta::parameters_of(fn("scale"))[1]) == ^^int);
  static_assert(meta::has_default_argument(meta::parameters_of(fn("step"))[0]));
  static_assert(!meta::has_default_argument(meta::parameters_of(fn("scale"))[0]));

  // A name survives only while every reachable declaration agrees on it.
  static_assert(set_value_named_before && !set_value_named_after);
  static_assert(!rename_named_before && rename_named_after);
  static_assert(specials == 4);  // default ctor, copy ctor, dtor, implicit copy assignment
  std::printf("setValue(int v) / (int newValue): named before definition=%d after=%d\n", set_value_named_before,
              set_value_named_after);
  std::printf("rename(int) / (int to):           named before definition=%d after=%d\n", rename_named_before,
              rename_named_after);
  std::printf("param_name(setValue, 0) after definition: %s\n", std::string{param_name(fn("setValue"), 0)}.c_str());

  // call through void** args
  Counter c;
  int v = 41;
  void* set_args[] = {nullptr, &v};
  call_with_args<fn("setValue")>(&c, set_args);
  assert(c.value_ == 41);

  int result = 0;
  void* get_args[] = {&result};
  call_with_args<fn("value")>(&c, get_args);
  assert(result == 41);

  double f = 0.5, out = 0;
  int times = 4;
  void* scale_args[] = {&out, &f, &times};
  call_with_args<fn("scale")>(&c, scale_args);
  assert(out == 82.0);

  // &[:fn:] as an NTTP
  constexpr auto pmf = pmf_holder<&[:fn("value"):]>::value;
  static_assert(std::is_same_v<decltype(pmf), int (Counter::* const)() const>);
  assert((c.*pmf)() == 41);

  std::puts("03 member functions: classification, parameter names, void** call, PMF NTTP OK");
}
