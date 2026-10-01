// The annotations a class uses to describe its Qt meta-object, and the
// consteval helpers that read them back.
#pragma once

// QT_NO_KEYWORDS is optional: the library never spells `emit`, `signals` or
// `slots` (rqt::activate replaces rqt::emit). With QT_NO_KEYWORDS rqt::emit stays
// as an alias, for code written before the Qt-like syntax.

#include <meta>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rqt {
namespace meta = std::meta;

// A signal declared as a function with a body: `[[=rqt::signal_function]] void f(int v) { rqt::activate{this}(v); }`.
// (rqt::signal is the class template of the bodyless data-member form, declared below.)
struct signal_t {};
inline constexpr signal_t signal_function{};
struct slot_t {};
inline constexpr slot_t slot{};
struct invokable_t {};
inline constexpr invokable_t invokable{};
struct enum_t {};
inline constexpr enum_t enum_{};
struct flag_t {};
inline constexpr flag_t flag{};

// Text inside an annotation. A `char const*` that points at a string literal
// is not a structural value, so reading the annotation back (extract) fails
// with `reflect_constant failed`; the characters are stored inline instead.
// A string literal converts implicitly: `.write = "setValue"`.
inline constexpr std::size_t max_name_length = 64;
inline constexpr std::size_t max_text_length = 256;

template <std::size_t Capacity>
struct text {
  char data[Capacity]{};

  constexpr text() = default;
  template <std::size_t N>
  constexpr text(char const (&literal)[N]) {
    static_assert(N <= Capacity, "rqt: annotation text is longer than the capacity (max_name_length, max_text_length)");
    for (std::size_t i = 0; i < N; ++i) data[i] = literal[i];
  }
  constexpr explicit text(std::string_view s) {
    if (s.size() >= Capacity) throw "rqt: annotation text is longer than the capacity";
    for (std::size_t i = 0; i < s.size(); ++i) data[i] = s[i];
  }
  constexpr std::string_view view() const { return data; }
};

using short_text = text<max_name_length>;

// On a getter (READ is the getter) or on a data member (MEMBER). Every name
// is optional; an empty name means "none". The fields are in this order, and
// a designated initializer must keep it.
struct property {
  short_text read{};    // READ function; empty: the annotated getter
  short_text write{};   // WRITE function
  short_text notify{};  // NOTIFY signal
  short_text reset{};   // RESET function
  short_text name{};    // the property name; empty: the name of the annotated getter or member
  bool final = false;
  bool constant = false;
  bool required = false;
  bool user = false;
  bool designable = true;
  bool scriptable = true;
  bool stored = true;
  int index = -1;  // explicit position in the property table; -1: declaration order, after the indexed ones
};

struct classinfo {
  short_text key;
  text<max_text_length> value;
};

// RQT_ENUM(Mode) / RQT_FLAG(Opts): a static constexpr member that holds the type.
// For a flag, Opts may be a QFlags<Enum> alias (Q_FLAG(Opts)) or an enum.
struct enum_decl {
  meta::info type;
  bool flag;
};

// Parameter names of a signal data member: `[[=rqt::names("value, count")]] rqt::signal<void(int, int)> changed;`
// (a function type has no parameter names, and QML reads them).
struct names {
  text<max_text_length> list;
  template <std::size_t N>
  constexpr names(char const (&literal)[N]) : list(literal) {}
};

// `rqt::signal<void(int)> valueChanged;` as a bodyless data member of a class with a meta-object.
// Owner is the class being defined; the closure in Tag gives every declaration its own type, so
// two members of the same signature stay distinct. Defined in signal.hpp.
template <class Sig, meta::info Owner = meta::current_class(), auto Tag = [] {}>
struct signal;

// --- RQT_PROPERTY: the exact Q_PROPERTY text, parsed at compile time ------------------------

inline constexpr std::size_t max_type_length = 128;

struct prop_decl {
  text<max_type_length> type{};
  short_text name{};
  short_text read{};
  short_text write{};
  short_text notify{};
  short_text reset{};
  short_text member{};
  bool final = false;
  bool constant = false;
  bool required = false;
  bool user = false;
  bool designable = true;
  bool scriptable = true;
  bool stored = true;
  int revision = 0;
};

namespace detail {

consteval bool is_property_keyword(std::string_view t) {
  for (auto k : {"READ", "WRITE", "MEMBER", "RESET", "NOTIFY", "REVISION", "DESIGNABLE", "SCRIPTABLE", "STORED", "USER",
                 "CONSTANT", "FINAL", "REQUIRED", "BINDABLE"})
    if (t == k) return true;
  return false;
}

consteval std::vector<std::string_view> split_words(std::string_view s) {
  std::vector<std::string_view> words;
  for (std::size_t i = 0; i < s.size();) {
    while (i < s.size() && s[i] == ' ') ++i;
    std::size_t j = i;
    while (j < s.size() && s[j] != ' ') ++j;
    if (j > i) words.push_back(s.substr(i, j - i));
    i = j;
  }
  return words;
}

[[noreturn]] consteval void bad_property(std::string_view why) {
  throw meta::exception(std::string{"RQT_PROPERTY: "}.append(why), ^^bad_property);
}

// `true` or `false` after DESIGNABLE, SCRIPTABLE, STORED and USER. A function name
// there (moc evaluates it at run time) is not supported.
consteval bool parse_bool_value(std::string_view keyword, std::vector<std::string_view> const& w, std::size_t& i) {
  if (i + 1 < w.size() && (w[i + 1] == "true" || w[i + 1] == "false")) return w[++i] == "true";
  if (i + 1 < w.size() && !is_property_keyword(w[i + 1]))
    bad_property(std::string{keyword}.append(" takes true or false; a function name is not supported"));
  return true;  // a bare USER
}

consteval std::string_view word_after(std::string_view keyword, std::vector<std::string_view> const& w, std::size_t& i) {
  if (i + 1 >= w.size()) bad_property(std::string{keyword}.append(" needs a name"));
  return w[++i];
}

consteval int parse_number(std::string_view s) {
  int n = 0;
  for (char c : s) {
    if (c < '0' || c > '9') bad_property("REVISION needs a number");
    n = n * 10 + (c - '0');
  }
  return n;
}

consteval void parse_keywords(prop_decl& p, std::vector<std::string_view> const& w, std::size_t first_keyword) {
  for (std::size_t i = first_keyword; i < w.size(); ++i) {
    std::string_view const k = w[i];
    if (k == "READ") p.read = short_text{word_after(k, w, i)};
    else if (k == "WRITE") p.write = short_text{word_after(k, w, i)};
    else if (k == "MEMBER") p.member = short_text{word_after(k, w, i)};
    else if (k == "RESET") p.reset = short_text{word_after(k, w, i)};
    else if (k == "NOTIFY") p.notify = short_text{word_after(k, w, i)};
    else if (k == "REVISION") p.revision = parse_number(word_after(k, w, i));
    else if (k == "DESIGNABLE") p.designable = parse_bool_value(k, w, i);
    else if (k == "SCRIPTABLE") p.scriptable = parse_bool_value(k, w, i);
    else if (k == "STORED") p.stored = parse_bool_value(k, w, i);
    else if (k == "USER") p.user = parse_bool_value(k, w, i);
    else if (k == "CONSTANT") p.constant = true;
    else if (k == "FINAL") p.final = true;
    else if (k == "REQUIRED") p.required = true;
    else bad_property(std::string{k}.append(" is not supported"));
  }
}

}  // namespace detail

// `RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)` stringifies its
// argument and lands here. The type may be several words or a template (`QList<int>`); the
// name is the word before the first keyword.
consteval prop_decl parse_property(std::string_view text_of_property) {
  auto const words = detail::split_words(text_of_property);
  std::size_t first_keyword = words.size();
  for (std::size_t i = 0; i < words.size(); ++i)
    if (detail::is_property_keyword(words[i])) {
      first_keyword = i;
      break;
    }
  if (first_keyword < 2) detail::bad_property("expected `type name KEYWORD ...`");

  prop_decl p;
  std::string_view name = words[first_keyword - 1];
  std::string type;
  for (std::size_t i = 0; i + 1 < first_keyword; ++i) type.append(i ? " " : "").append(words[i]);
  // `Person *host` stringifies as two words, the star glued to the name
  while (!name.empty() && (name.front() == '*' || name.front() == '&')) {
    type.push_back(name.front());
    name.remove_prefix(1);
  }
  p.type = text<max_type_length>{type};
  p.name = short_text{name};
  detail::parse_keywords(p, words, first_keyword);
  if (p.read.view().empty() && p.member.view().empty()) detail::bad_property("needs READ or MEMBER");
  return p;
}

namespace detail {

// annotations_of throws for a template (member function templates, constructor
// templates); a template carries no rqt annotation.
template <class Tag>
consteval bool has(meta::info r) {
  if (meta::is_template(r)) return false;
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) return true;
  return false;
}

template <class Tag>
consteval Tag get(meta::info r) {
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) return meta::extract<Tag>(a);
  throw meta::exception("missing annotation", r);
}

template <class Tag>
consteval std::vector<Tag> all(meta::info r) {
  std::vector<Tag> out;
  if (meta::is_template(r)) return out;
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) out.push_back(meta::extract<Tag>(a));
  return out;
}

}  // namespace detail
}  // namespace rqt
