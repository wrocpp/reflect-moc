// EXPERIMENT (spike/namespace-meta, not part of the library): a namespace meta-object built from the library's consteval
// helpers (rqt::detail::make_enums, string_list, enum_real ...) and QtMocHelpers::metaObjectData<void, tag>. See ../namespace-spike.md.
#pragma once
#include <reflect_moc/qt/qt.hpp>
#include <QtCore/QMetaEnum>

namespace nsx {
namespace d = rqt::detail;
namespace meta = std::meta;
using info = meta::info;
namespace QMC = QtMocConstants;

template <info Ns> inline constexpr auto strings = d::string_list(Ns);
template <info Ns> consteval unsigned index(std::string_view s) {
  for (std::size_t i = 0; i < strings<Ns>.size(); ++i) if (std::string_view{strings<Ns>[i]} == s) return unsigned(i);
  throw meta::exception("nsx: string missing", Ns);
}
template <info Ns> struct string_table {
  static constexpr int StringCount = int(strings<Ns>.size());
  static constexpr std::size_t StringSize = [] { std::size_t n = 0; for (auto s : strings<Ns>) n += std::string_view{s}.size() + 1; return n; }();
  constexpr void writeTo(unsigned (&offsets)[2 * StringCount], char (&data)[StringSize]) const {
    unsigned offset = 0;
    for (int i = 0; i < StringCount; ++i) {
      std::string_view const s{strings<Ns>[i]};
      for (std::size_t j = 0; j <= s.size(); ++j) data[offset + j] = strings<Ns>[i][j];
      offsets[2 * i] = offset + sizeof(offsets);
      offsets[2 * i + 1] = unsigned(s.size());
      offset += unsigned(s.size() + 1);
    }
  }
};
template <info Ns> inline constexpr auto enum_list = std::define_static_array(d::make_enums(Ns));

template <info Ns, d::enum_entry En, std::size_t... V>
constexpr auto enum_data_for(std::index_sequence<V...>) {
  using Enum = typename[:meta::dealias(En.type):];
  constexpr info real = d::enum_real(En);
  constexpr unsigned name = index<Ns>(d::enum_name(En));
  constexpr unsigned alias = index<Ns>(d::enum_alias(En));
  constexpr unsigned flags = En.flag ? QMC::EnumIsFlag : 0u;
  if constexpr (sizeof...(V) == 0) return QtMocHelpers::EnumData<Enum>(name, alias, flags);
  else return QtMocHelpers::EnumData<Enum>(name, alias, flags).add({{int(index<Ns>(meta::identifier_of(meta::enumerators_of(real)[V]))), [:meta::enumerators_of(real)[V]:]}...});
}
template <info Ns, std::size_t I> constexpr auto enum_data() {
  constexpr d::enum_entry e = enum_list<Ns>[I];
  return enum_data_for<Ns, e>(std::make_index_sequence<meta::enumerators_of(d::enum_real(e)).size()>{});
}
template <info Ns> struct ns_tag {};
template <info Ns> constexpr auto content = [] {
  auto enums = []<std::size_t... I>(std::index_sequence<I...>) { return QtMocHelpers::UintData{enum_data<Ns, I>()...}; }(std::make_index_sequence<enum_list<Ns>.size()>{});
  QtMocHelpers::UintData none{};
  return QtMocHelpers::metaObjectData<void, ns_tag<Ns>>(QMC::PropertyAccessInStaticMetaCall, string_table<Ns>{}, none, none, enums);
}();

// (b) a variable template: named by a reference, so instantiated at the end of the translation unit
template <info Ns = meta::current_namespace()>
inline constexpr QMetaObject ns_meta = {{
nullptr, content<Ns>.staticData.stringdata, content<Ns>.staticData.data, nullptr, nullptr,
                                         content<Ns>.relocatingData.metaTypes, nullptr}};

// (a) a function: constexpr reference needs the body at once
template <info Ns = meta::current_namespace()>
constexpr QMetaObject const& ns_meta_fn() { return ns_meta<Ns>; }

// The ADL pair, found through a using-declaration in the namespace.
template <class E>
  requires std::is_enum_v<E> && (meta::is_namespace(meta::parent_of(^^E)))
constexpr QMetaObject const* qt_getEnumMetaObject(E const&) noexcept { return &ns_meta<meta::parent_of(^^E)>; }
template <class E>
  requires std::is_enum_v<E> && (meta::is_namespace(meta::parent_of(^^E)))
constexpr char const* qt_getEnumName(E const&) noexcept {
  for (auto const& e : enum_list<meta::parent_of(^^E)>) if (meta::dealias(e.type) == ^^E) return std::define_static_string(d::enum_name(e));
  return "";
}
// Evaluated when the function body is instantiated, which GCC does at the end of the translation unit: then members_of sees
// the whole namespace, and the snapshot taken at the namespace line can be compared with it.
template <info Ns = meta::current_namespace()>
bool verify() {
  static_assert(d::make_enums(Ns).size() == enum_list<Ns>.size(),
                "rqt: an enum annotated rqt::enum_ or rqt::flag is declared after the rqt namespace line, so the meta-object misses it");
  return true;
}
}  // namespace nsx
