// Spike 02: annotations (P3394) on every entity kind moc cares about.
//
// Question: can annotations mark member functions, enumerators, classes and
// static constexpr data members; can they carry an aggregate value and a
// char const* from define_static_string; and does the portable lookup
// (annotations_of + remove_cvref(type_of(a)) == ^^Tag + extract<Tag>) work
// for all of them?
//
// Build:  g++-16 -std=c++26 -freflection -Wall -Wextra spikes/02-annotations.cpp
// Probe:  -DSPIKE_STRING_LITERAL (a string literal pointer inside an annotation)
#include <meta>

#include <cassert>
#include <cstdio>
#include <string_view>

namespace meta = std::meta;

#if defined(__clang__)
// clang-p2996 has no std::meta::exception. Calling a non-constexpr function
// also stops constant evaluation, with a less readable diagnostic.
inline void reflection_error(std::string_view) {}
#define SPIKE_THROW(msg, r) (reflection_error(msg), throw 0)
#else
#define SPIKE_THROW(msg, r) throw meta::exception(msg, r)
#endif

struct property {
  bool notify = false;
  bool read_only = false;
};
struct slot_t {};
inline constexpr slot_t slot{};
struct invokable_t {};
inline constexpr invokable_t invokable{};
struct doc {
  char const* text;
};
struct classinfo {
  char const* key;
  char const* value;
};

template <class Tag>
consteval bool has_annotation(meta::info r) {
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) return true;
  return false;
}

template <class Tag>
consteval Tag annotation_of(meta::info r) {
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) return meta::extract<Tag>(a);
  SPIKE_THROW("no such annotation", r);
}

consteval bool same(char const* a, std::string_view b) { return std::string_view{a} == b; }

struct [[= doc{std::define_static_string("A button with a counter")}]]
[[= classinfo{std::define_static_string("DefaultProperty"), std::define_static_string("count")}]] Button {
  [[= property{.notify = true, .read_only = false}]] int count = 0;
  [[= property{.read_only = true}]] int generation = 0;

  [[= slot]] void reset() { count = 0; }
  [[= invokable]] [[= doc{std::define_static_string("Adds n")}]] int add(int n) { return count += n; }
  void plain() {}

  [[= doc{std::define_static_string("schema")}]] static constexpr int version = 3;

  enum class Mode { Fast [[= doc{std::define_static_string("fast path")}]], Slow };

#ifdef SPIKE_STRING_LITERAL
  [[= doc{"literal"}]] void literal_doc() {}
#endif
};

consteval meta::info member(std::string_view name) {
  for (auto m : meta::members_of(^^Button, meta::access_context::unchecked()))
    if (meta::has_identifier(m) && meta::identifier_of(m) == name) return m;
  SPIKE_THROW("no member", ^^Button);
}

consteval meta::info enumerator(std::string_view name) {
  for (auto e : meta::enumerators_of(^^Button::Mode))
    if (meta::identifier_of(e) == name) return e;
  SPIKE_THROW("no enumerator", ^^Button::Mode);
}

consteval int count_annotations(meta::info r) { return static_cast<int>(meta::annotations_of(r).size()); }

int main() {
  // class
  static_assert(has_annotation<doc>(^^Button));
  static_assert(same(annotation_of<doc>(^^Button).text, "A button with a counter"));
  static_assert(same(annotation_of<classinfo>(^^Button).key, "DefaultProperty"));
  static_assert(count_annotations(^^Button) == 2);

  // aggregate-valued annotation on data members
  static_assert(annotation_of<property>(member("count")).notify);
  static_assert(!annotation_of<property>(member("count")).read_only);
  static_assert(annotation_of<property>(member("generation")).read_only);
  static_assert(!annotation_of<property>(member("generation")).notify);

  // member functions
  static_assert(has_annotation<slot_t>(member("reset")));
  static_assert(has_annotation<invokable_t>(member("add")));
  static_assert(same(annotation_of<doc>(member("add")).text, "Adds n"));
  static_assert(!has_annotation<slot_t>(member("plain")));
  static_assert(count_annotations(member("plain")) == 0);

  // static constexpr data member
  static_assert(meta::is_variable(member("version")));
  static_assert(same(annotation_of<doc>(member("version")).text, "schema"));

  // enumerators
  static_assert(same(annotation_of<doc>(enumerator("Fast")).text, "fast path"));
  static_assert(!has_annotation<doc>(enumerator("Slow")));

  // the annotation value is also usable at run time
  constexpr char const* text = annotation_of<doc>(^^Button).text;
  assert(std::string_view{text} == "A button with a counter");

  std::puts("02 annotations: class, member fn, enumerator, static constexpr, aggregate, define_static_string OK");
}
