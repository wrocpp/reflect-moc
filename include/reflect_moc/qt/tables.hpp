// The tables moc would have parsed from the header, read from reflection:
// method list (with clones for default arguments), properties, enums, class
// info and the string pool that the meta-object data indexes into.
#pragma once

#include "annotations.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rqt {

// Marks every rqt::Object<B>, so reflection can tell a base that carries
// meta-object data from one that does not.
class object_tag {};

template <class B>
class Object;

namespace detail {

using info = meta::info;

constexpr auto unchecked = meta::access_context::unchecked();

consteval std::string cat(std::initializer_list<std::string_view> parts) {
  std::string s;
  for (auto p : parts) s.append(p);
  return s;
}

consteval std::string qualified_name(info t) {
  std::string s{meta::identifier_of(t)};
  // The global namespace, and an anonymous one, have no identifier and end the walk.
  for (info p = meta::parent_of(t); (meta::is_namespace(p) || meta::is_class_type(p)) && meta::has_identifier(p);
       p = meta::parent_of(p))
    s = cat({meta::identifier_of(p), "::", s});
  return s;
}

// --- methods -------------------------------------------------------------------------

// Qt's method order: every signal, then every slot, then every invokable.
enum class method_kind { signal_, slot_, invokable_, none };

consteval method_kind kind_of(info m) {
  if (!meta::is_function(m)) return method_kind::none;
  if (has<signal_t>(m)) return method_kind::signal_;
  if (has<slot_t>(m)) return method_kind::slot_;
  if (has<invokable_t>(m)) return method_kind::invokable_;
  return method_kind::none;
}

// One row of the method table. A function with default arguments yields the
// full row and then one cloned row per omitted trailing argument, as moc does.
struct method_entry {
  info fn;
  std::size_t nargs;
  bool cloned;
};

consteval std::size_t default_count(info fn) {
  std::size_t n = 0;
  for (auto p : meta::parameters_of(fn)) n += meta::has_default_argument(p);
  return n;
}

consteval void append_entries(std::vector<method_entry>& out, info fn) {
  std::size_t const full = meta::parameters_of(fn).size();
  out.push_back({.fn = fn, .nargs = full, .cloned = false});
  for (std::size_t omitted = 1; omitted <= default_count(fn); ++omitted)
    out.push_back({.fn = fn, .nargs = full - omitted, .cloned = true});
}

consteval std::vector<method_entry> make_method_entries(info cls) {
  std::vector<method_entry> out;
  for (auto kind : {method_kind::signal_, method_kind::slot_, method_kind::invokable_})
    for (auto m : meta::members_of(cls, unchecked))
      if (kind_of(m) == kind) {
        if (meta::is_static_member(m)) throw meta::exception("rqt: a signal, slot or invokable cannot be static", m);
        append_entries(out, m);
      }
  return out;
}

template <class D>
inline constexpr auto method_entries = std::define_static_array(make_method_entries(^^D));

template <class D>
inline constexpr std::size_t method_total = method_entries<D>.size();

consteval std::size_t count_signals(info cls) {
  std::size_t n = 0;
  for (auto e : make_method_entries(cls)) n += kind_of(e.fn) == method_kind::signal_;
  return n;
}

// The method-table position of the first (uncloned) row for fn.
consteval int entry_index(info cls, info fn) {
  auto const all = make_method_entries(cls);
  for (std::size_t i = 0; i < all.size(); ++i)
    if (all[i].fn == fn && !all[i].cloned) return static_cast<int>(i);
  throw meta::exception("rqt: not a signal, slot or invokable of the class", fn);
}

// --- types and names -----------------------------------------------------------------

// Types that moc writes as a plain QMetaType id. Everything else is written as
// IsUnresolvedType plus the index of its name, and Qt takes the type itself
// from the metaTypes array.
consteval bool resolved_by_id(info t) {
  t = meta::remove_cvref(t);
  return meta::is_void_type(t) || meta::is_arithmetic_type(t);
}

consteval std::string type_name(info t) {
  t = meta::remove_cvref(t);
  if (meta::is_enum_type(t) && meta::has_identifier(t) && meta::is_class_type(meta::parent_of(t)))
    return std::string{meta::identifier_of(t)};
  return std::string{meta::display_string_of(t)};
}

consteval std::string_view param_name(info p) {
  return meta::has_identifier(p) ? meta::identifier_of(p) : std::string_view{};
}

// --- properties ----------------------------------------------------------------------

// A reflection that stands for "none". Neither a null reflection nor the global
// namespace can be stored in a static array (reflect_constant fails), so void is
// the sentinel: no function, member or property type reflects it.
consteval info absent() { return ^^void; }
consteval bool present(info r) { return r != absent(); }

struct prop_desc {
  info anchor = absent();
  info type = absent();
  info member = absent();
  info read = absent();
  info write = absent();
  info notify = absent();
  info reset = absent();
  bool writable = false;
};

template <class Pred>
consteval info find_function(info cls, info anchor, std::string_view name, std::string_view role, Pred ok,
                             std::string_view requirement) {
  bool seen = false;
  for (auto m : meta::members_of(cls, unchecked))
    if (meta::is_function(m) && meta::has_identifier(m) && meta::identifier_of(m) == name) {
      seen = true;
      if (ok(m)) return m;
    }
  throw meta::exception(seen ? cat({"rqt::property ", role, " '", name, "': ", requirement})
                             : cat({"rqt::property ", role, " '", name, "': no member function of that name in ",
                                    meta::identifier_of(cls)}),
                        anchor);
}

consteval std::size_t arity(info fn) { return meta::parameters_of(fn).size(); }

consteval info resolve_notify(info cls, info anchor, std::string_view name) {
  return find_function(
      cls, anchor, name, "NOTIFY", [](info m) { return has<signal_t>(m) && arity(m) <= 1; },
      "is not an rqt::signal with at most one parameter");
}

consteval void resolve_accessors(prop_desc& d, info cls, property const& p) {
  if (std::string_view{p.write}.size()) {
    d.write = find_function(cls, d.anchor, p.write, "WRITE", [](info m) { return arity(m) == 1; },
                            "needs a member function with one parameter");
    d.writable = true;
  }
  if (std::string_view{p.reset}.size())
    d.reset = find_function(cls, d.anchor, p.reset, "RESET", [](info m) { return arity(m) == 0; },
                            "needs a member function without parameters");
  if (std::string_view{p.notify}.size()) d.notify = resolve_notify(cls, d.anchor, p.notify);
}

consteval prop_desc make_prop(info cls, info anchor) {
  auto const p = get<property>(anchor);
  prop_desc d{.anchor = anchor};
  if (meta::is_nonstatic_data_member(anchor)) {
    d.member = anchor;
    d.type = meta::remove_cvref(meta::type_of(anchor));
    d.writable = !meta::is_const(meta::type_of(anchor));
  } else if (meta::is_function(anchor)) {
    d.read = std::string_view{p.read}.empty()
                 ? anchor
                 : find_function(cls, anchor, p.read, "READ", [](info m) { return arity(m) == 0; },
                                 "needs a member function without parameters");
    d.type = meta::remove_cvref(meta::return_type_of(d.read));
  } else {
    throw meta::exception("rqt::property goes on a getter or a non-static data member", anchor);
  }
  resolve_accessors(d, cls, p);
  return d;
}

consteval std::vector<prop_desc> make_props(info cls) {
  std::vector<prop_desc> out;
  for (auto m : meta::members_of(cls, unchecked))
    if (has<property>(m)) out.push_back(make_prop(cls, m));
  return out;
}

// prop_desc holds reflections, which cannot be stored in a static array here
// (reflect_constant fails on the data-member and accessor reflections), so the
// array holds member positions and property_at expands a row back.
struct prop_row {
  std::size_t anchor = 0;
  std::size_t member = 0;
  std::size_t read = 0;
  std::size_t write = 0;
  std::size_t notify = 0;
  std::size_t reset = 0;
  bool writable = false;
};

consteval std::size_t position_of(info cls, info m) {
  auto const all = meta::members_of(cls, unchecked);
  for (std::size_t i = 0; i < all.size(); ++i)
    if (all[i] == m) return i;
  return all.size();  // not a member: stands for absent
}

consteval info member_at(info cls, std::size_t i) {
  auto const all = meta::members_of(cls, unchecked);
  return i < all.size() ? all[i] : absent();
}

consteval std::vector<prop_row> make_prop_rows(info cls) {
  std::vector<prop_row> out;
  for (auto d : make_props(cls))
    out.push_back({.anchor = position_of(cls, d.anchor),
                   .member = position_of(cls, d.member),
                   .read = position_of(cls, d.read),
                   .write = position_of(cls, d.write),
                   .notify = position_of(cls, d.notify),
                   .reset = position_of(cls, d.reset),
                   .writable = d.writable});
  return out;
}

template <class D>
inline constexpr auto property_rows = std::define_static_array(make_prop_rows(^^D));

template <class D>
inline constexpr std::size_t property_count = property_rows<D>.size();

consteval prop_desc expand_prop(info cls, prop_row const& r) {
  prop_desc d{.anchor = member_at(cls, r.anchor),
              .member = member_at(cls, r.member),
              .read = member_at(cls, r.read),
              .write = member_at(cls, r.write),
              .notify = member_at(cls, r.notify),
              .reset = member_at(cls, r.reset),
              .writable = r.writable};
  d.type = present(d.member) ? meta::remove_cvref(meta::type_of(d.member))
                             : meta::remove_cvref(meta::return_type_of(d.read));
  return d;
}

template <class D>
consteval prop_desc property_at(std::size_t i) {
  return expand_prop(^^D, property_rows<D>[i]);
}

consteval int notify_index(info cls, prop_desc const& d) {
  return present(d.notify) ? entry_index(cls, d.notify) : -1;
}

// moc marks a setter named set<Name> as the standard C++ one.
consteval bool is_std_setter(prop_desc const& d) {
  if (!present(d.write) || !meta::has_identifier(d.write)) return false;
  std::string expected{"set"};
  std::string_view const name = meta::identifier_of(d.anchor);
  expected.push_back(static_cast<char>(name[0] >= 'a' && name[0] <= 'z' ? name[0] - 'a' + 'A' : name[0]));
  expected.append(name.substr(1));
  return meta::identifier_of(d.write) == expected;
}

// --- enums and class info ------------------------------------------------------------

consteval std::vector<info> make_enums(info cls) {
  std::vector<info> out;
  for (auto m : meta::members_of(cls, unchecked))
    if (meta::is_enumerable_type(m) && (has<enum_t>(m) || has<flag_t>(m))) out.push_back(m);
  return out;
}

template <class D>
inline constexpr auto enum_list = std::define_static_array(make_enums(^^D));

template <class D>
inline constexpr auto classinfo_list = std::define_static_array(all<classinfo>(^^D));

// --- the string pool -----------------------------------------------------------------

struct string_pool {
  std::vector<std::string> items;

  consteval void add(std::string_view s) {
    for (auto const& e : items)
      if (e == s) return;
    items.emplace_back(s);
  }
  consteval void add_type(info t) {
    if (!resolved_by_id(t)) add(type_name(t));
  }
};

consteval void pool_methods(string_pool& pool, info cls) {
  for (auto e : make_method_entries(cls)) {
    pool.add(meta::identifier_of(e.fn));
    pool.add_type(meta::return_type_of(e.fn));
    for (auto p : meta::parameters_of(e.fn)) {
      pool.add(param_name(p));
      pool.add_type(meta::type_of(p));
    }
  }
}

consteval void pool_properties(string_pool& pool, info cls) {
  for (auto d : make_props(cls)) {
    pool.add(meta::identifier_of(d.anchor));
    pool.add_type(d.type);
  }
}

consteval void pool_enums(string_pool& pool, info cls) {
  for (auto e : make_enums(cls)) {
    pool.add(meta::identifier_of(e));
    for (auto v : meta::enumerators_of(e)) pool.add(meta::identifier_of(v));
  }
}

// Class name first (Qt requires index 0), then "" (the empty tag).
consteval std::vector<std::string> make_strings(info cls) {
  string_pool pool;
  pool.add(qualified_name(cls));
  pool.add("");
  for (auto ci : all<classinfo>(cls)) {
    pool.add(ci.key);
    pool.add(ci.value);
  }
  pool_methods(pool, cls);
  pool_properties(pool, cls);
  pool_enums(pool, cls);
  return pool.items;
}

consteval std::span<char const* const> string_list(info cls) {
  std::vector<char const*> v;
  for (auto const& s : make_strings(cls)) v.push_back(std::define_static_string(s));
  return std::define_static_array(v);
}

template <class D>
inline constexpr std::span<char const* const> strings = string_list(^^D);

template <class D>
consteval unsigned string_index(std::string_view s) {
  for (std::size_t i = 0; i < strings<D>.size(); ++i)
    if (std::string_view{strings<D>[i]} == s) return static_cast<unsigned>(i);
  throw meta::exception("rqt: internal error, string missing from the pool", ^^D);
}

// --- inheritance -----------------------------------------------------------------------

// rqt::Object<B> is the only class that has object_tag as a direct base.
consteval bool is_object_instance(info t) {
  for (auto b : meta::bases_of(t, unchecked))
    if (meta::type_of(b) == ^^object_tag) return true;
  return false;
}

// The direct base of cls that carries meta-object data: rqt::Object<B> itself
// or another class derived from it.
consteval info object_base_of(info cls) {
  for (auto b : meta::bases_of(cls, unchecked))
    if (meta::type_of(b) != (^^object_tag) && meta::is_base_of_type(^^object_tag, meta::type_of(b)))
      return meta::type_of(b);
  throw meta::exception("rqt: the class does not derive from rqt::Object", cls);
}

consteval bool declares_static_meta_object(info cls) {
  for (auto m : meta::members_of(cls, unchecked))
    if (meta::has_identifier(m) && meta::identifier_of(m) == "staticMetaObject") return true;
  return false;
}

}  // namespace detail
}  // namespace rqt
