// Q_NAMESPACE, Q_ENUM_NS and Q_FLAG_NS: the meta-object of a namespace and what Qt asks of an enum declared in one.
//
//   namespace Outer::N {
//   enum class [[= rqt::enum_]] E { a, b };
//   RQT_NAMESPACE                          // or the three lines it stands for, in this order:
//   }
//
//   using rqt::qt_getEnumMetaObject;       // 1: what Q_ENUM_NS defines per enum, found by argument-dependent lookup
//   using rqt::qt_getEnumName;
//   inline QMetaObject const& staticMetaObject = rqt::namespace_meta<>;         // 2: Q_NAMESPACE
//   inline bool const rqt_namespace_checked_ = rqt::check_namespace<>();        // 3: an enum declared after line 2
//
// Line 1 comes before line 2: building the meta-object makes Qt decide, once, whether the enum is a Q_ENUM, and it must
// see the functions then. Line 2 sees the enums declared above it; line 3 stops the build for one declared below.
#pragma once

#include "object.hpp"

#include <QtCore/QMetaObject>
#include <QtCore/QtGlobal>

namespace rqt {
namespace detail {

// The namespace an enum, or the QFlags of one (Q_FLAG_NS), is declared in; a null reflection for any other type.
consteval info namespace_of_enum(info t) {
  t = meta::dealias(t);
  if (meta::has_template_arguments(t) && meta::template_of(t) == ^^QFlags) t = meta::dealias(meta::template_arguments_of(t)[0]);
  if (!meta::is_enum_type(t)) return info{};
  info const parent = meta::parent_of(t);
  return meta::is_namespace(parent) ? parent : info{};
}

// An entry answers for its own type and, for a QFlags alias, for the enum behind it: Qt's debug output of flags asks
// qt_getEnumMetaObject(Edge()) where moc declared only the function for the flags, and the implicit conversion took it there.
consteval bool answers_for(enum_entry const& entry, info e) {
  return meta::dealias(entry.type) == meta::dealias(e) || (entry.flag && enum_real(entry) == meta::dealias(e));
}

consteval bool registered_in_namespace(info e) {
  info const ns = namespace_of_enum(e);
  if (ns == info{} || !meta::has_identifier(ns)) return false;
  for (auto const& entry : make_enums(ns))
    if (answers_for(entry, e)) return true;
  return false;
}

// A namespace inside this one that registers enums and has no meta-object of its own: its enums are not in the table of the
// outer namespace (members_of lists the nested namespace, not what is in it), and an inline namespace is one.
consteval bool has_unregistered_nested_enums(info ns) {
  for (auto m : meta::members_of(ns, unchecked))
    if (meta::is_namespace(m) && !has_namespace_meta(m) && (!make_enums(m).empty() || has_unregistered_nested_enums(m))) return true;
  return false;
}

// True for the enums (and QFlags) a namespace's meta-object lists: the table of the namespace is read at the first use, so
// this is false for one declared after rqt::namespace_meta was named.
template <class E>
inline constexpr bool namespace_enum = registered_in_namespace(^^E);

}  // namespace detail

template <class E>
  requires(detail::namespace_enum<E>)
constexpr QMetaObject const* qt_getEnumMetaObject(E const&) noexcept {
  return &namespace_meta<detail::namespace_of_enum(^^E)>;
}

template <class E>
  requires(detail::namespace_enum<E>)
constexpr char const* qt_getEnumName(E const&) noexcept {
  constexpr detail::info ns = detail::namespace_of_enum(^^E);
  for (auto const& entry : detail::enum_list<detail::namespace_scope<ns>>)
    if (detail::answers_for(entry, ^^E)) return std::define_static_string(detail::enum_name(entry));
  return "";
}

// Evaluated where the function body is instantiated, which is the end of the translation unit: there the whole namespace
// is declared, and the table taken at rqt::namespace_meta can be compared with it.
template <meta::info Ns = meta::current_namespace()>
bool check_namespace() {
  static_assert(detail::make_enums(Ns).size() == detail::enum_list<detail::namespace_scope<Ns>>.size(),
                "rqt: an enum annotated rqt::enum_ or rqt::flag is declared after RQT_NAMESPACE (rqt::namespace_meta), "
                "so the meta-object of the namespace does not list it; declare it above");
  static_assert(!detail::has_unregistered_nested_enums(Ns),
                "rqt: an enum is registered in a namespace nested in this one (an inline namespace counts) that has no "
                "RQT_NAMESPACE of its own; the meta-object of this namespace lists only its own enums");
  return true;
}

}  // namespace rqt
