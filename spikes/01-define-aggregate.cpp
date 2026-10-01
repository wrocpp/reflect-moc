// Spike 01: define_aggregate.
//
// Question: can reflection synthesize a class (signals_of<T>) from the
// annotated fields of a user class T, and from which contexts can the
// definition be triggered?
//
// Build:  g++-16 -std=c++26 -freflection -Wall -Wextra spikes/01-define-aggregate.cpp
// Probe the rejected variants with -DSPIKE_CRTP_BASE, -DSPIKE_IN_CLASS,
// -DSPIKE_LAZY_ALIAS, -DSPIKE_CONSTEVAL_FN (each is expected to fail).
#include <meta>

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace meta = std::meta;

// --- 1a: the minimal case ---------------------------------------------------
struct S;
consteval { meta::define_aggregate(^^S, {meta::data_member_spec(^^int, {.name = "x"})}); }

// --- 1b: signals_of<T> -------------------------------------------------------
struct property {
  bool notify = false;
  bool read_only = false;
};

template <class T>
struct signal_ref {
  int emits = 0;
  T last{};
  void operator()(T v) {
    ++emits;
    last = v;
  }
};

consteval bool notifies(meta::info member) {
  for (auto a : meta::annotations_of(member))
    if (meta::remove_cvref(meta::type_of(a)) == ^^property) return meta::extract<property>(a).notify;
  return false;
}

// The class every define_* call completes. Incomplete until someone defines it.
template <class T>
struct signals_of_t;

template <class T>
using signals_of = signals_of_t<T>;

consteval meta::info define_signals(meta::info target, meta::info cls) {
  std::vector<meta::info> specs;
  for (auto m : meta::nonstatic_data_members_of(cls, meta::access_context::unchecked())) {
    if (!notifies(m)) continue;
    std::string name{meta::identifier_of(m)};
    name += "Changed";
    specs.push_back(meta::data_member_spec(meta::substitute(^^signal_ref, {meta::type_of(m)}), {.name = name}));
  }
  return meta::define_aggregate(target, specs);
}

struct Model {
  [[= property{.notify = true}]] int width = 0;
  [[= property{.notify = true}]] double ratio = 0;
  [[= property{}]] int silent = 0;
  char const* label = nullptr;
};

// V1: namespace-scope consteval block written after T.
consteval { define_signals(^^signals_of_t<Model>, ^^Model); }

// V2: the target is a NESTED class of a class template, completed by a
// consteval block in that same template. The block and the target share a
// scope, so no class scope intervenes. The alias instantiates the holder on
// first use, which runs the block; T only has to be complete by then.
template <class T>
struct signals_holder {
  struct type;
  consteval { define_signals(^^type, ^^T); }
};
template <class T>
using lazy_signals_of = typename signals_holder<T>::type;

struct Model2 {
  [[= property{.notify = true}]] long count = 0;
};

// V3: a CRTP base that only names lazy_signals_of<D> inside a member function
// body, which is instantiated on first call, after D is complete. The return
// type must be deduced: spelling lazy_signals_of<D>& in the declaration
// instantiates the holder together with the base, while D is incomplete.
template <class D>
struct object_base {
  auto& signals() {
    static lazy_signals_of<D> s{};
    return s;
  }
};

struct Model3 : object_base<Model3> {
  [[= property{.notify = true}]] int a = 0;
  [[= property{.notify = true}]] int b = 0;
  void setA(int v) {
    a = v;
    signals().aChanged(v);
  }
};

#ifdef SPIKE_TEMPLATE_BLOCK_NAMESPACE_TARGET
// A consteval block in a class template that completes a NAMESPACE-scope class.
template <class T>
struct define_signals_for {
  consteval { define_signals(^^signals_of_t<T>, ^^T); }
};
struct Model7 {
  [[= property{.notify = true}]] long count = 0;
};
template struct define_signals_for<Model7>;
#endif

#ifdef SPIKE_FN_BLOCK
// A block-scope consteval block inside a function template.
template <class T>
void define_in_function() {
  consteval { define_signals(^^signals_of_t<T>, ^^T); }
}
struct Model8 {
  [[= property{.notify = true}]] long count = 0;
};
template void define_in_function<Model8>();
#endif

#ifdef SPIKE_RETURN_TYPE
// V3 with the holder named in the declaration of a CRTP base member.
template <class D>
struct object_base_named {
  lazy_signals_of<D>& signals();
};
struct Model9 : object_base_named<Model9> {
  [[= property{.notify = true}]] int a = 0;
};
#endif

#ifdef SPIKE_CRTP_BASE
// A consteval block in a CRTP base. D is incomplete while its base is instantiated.
template <class D>
struct with_signals {
  consteval { define_signals(^^signals_of_t<D>, ^^D); }
};
struct Model3b : with_signals<Model3b> {
  [[= property{.notify = true}]] int a = 0;
};
signals_of<Model3b> probe3{};
#endif

#ifdef SPIKE_IN_CLASS
// V4: consteval block inside T itself, after the members.
struct Model4 {
  [[= property{.notify = true}]] int a = 0;
  consteval { define_signals(^^signals_of_t<Model4>, ^^Model4); }
};
signals_of<Model4> probe4{};
#endif

#ifdef SPIKE_LAZY_ALIAS
// V5: an alias template that defines on first use.
consteval meta::info ensure_signals(meta::info cls) {
  auto target = meta::substitute(^^signals_of_t, {cls});
  if (!meta::is_complete_type(target)) define_signals(target, cls);
  return target;
}
template <class T>
using splice_signals_of = [:ensure_signals(^^T):];
struct Model5 {
  [[= property{.notify = true}]] int a = 0;
};
splice_signals_of<Model5> probe5{};
#endif

#ifdef SPIKE_CONSTEVAL_FN
// V6: define_aggregate from a constexpr variable initializer (not a consteval block).
struct Model6 {
  [[= property{.notify = true}]] int a = 0;
};
constexpr auto defined6 = define_signals(^^signals_of_t<Model6>, ^^Model6);
signals_of<Model6> probe6{};
#endif

template <class T>
consteval std::size_t member_count() {
  return meta::nonstatic_data_members_of(^^T, meta::access_context::unchecked()).size();
}

template <class T, std::size_t I>
consteval std::string_view member_name() {
  return meta::identifier_of(meta::nonstatic_data_members_of(^^T, meta::access_context::unchecked())[I]);
}

int main() {
  // 1a
  S s{.x = 42};
  assert(s.x == 42);
  static_assert(member_count<S>() == 1);

  // 1b, V1
  static_assert(member_count<signals_of<Model>>() == 2);
  static_assert(member_name<signals_of<Model>, 0>() == "widthChanged");
  static_assert(member_name<signals_of<Model>, 1>() == "ratioChanged");
  static_assert(std::is_same_v<decltype(signals_of<Model>::widthChanged), signal_ref<int>>);
  static_assert(std::is_same_v<decltype(signals_of<Model>::ratioChanged), signal_ref<double>>);
  signals_of<Model> sig{};
  sig.widthChanged(7);
  sig.ratioChanged(0.5);
  sig.widthChanged(8);
  assert(sig.widthChanged.emits == 2 && sig.widthChanged.last == 8);
  assert(sig.ratioChanged.emits == 1);

  // V2
  static_assert(member_count<lazy_signals_of<Model2>>() == 1);
  lazy_signals_of<Model2> sig2{};
  sig2.countChanged(3L);
  assert(sig2.countChanged.last == 3L);

  // V3
  Model3 m3;
  m3.setA(5);
  assert(m3.signals().aChanged.emits == 1 && m3.signals().aChanged.last == 5);
  static_assert(member_count<lazy_signals_of<Model3>>() == 2);

  std::puts("01 define_aggregate: namespace block, nested-holder alias, CRTP member-body use OK");
}
