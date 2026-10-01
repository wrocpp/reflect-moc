// QMetaObject data for a reflected class, laid out by Qt's own
// QtMocHelpers::metaObjectData from the tables in tables.hpp.
#pragma once

#include "tables.hpp"

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/qtmochelpers.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace rqt {
namespace detail {

namespace QMC = QtMocConstants;

// QtMocHelpers::StringRefStorage wants char arrays of known extent. metaObjectData
// only needs StringCount, StringSize and writeTo, so this duck-types them over the pool.
template <class D>
struct string_table {
  static constexpr int StringCount = static_cast<int>(strings<D>.size());
  static constexpr std::size_t StringSize = [] {
    std::size_t n = 0;
    for (auto s : strings<D>) n += std::string_view{s}.size() + 1;
    return n;
  }();

  constexpr void writeTo(unsigned (&offsets)[2 * StringCount], char (&data)[StringSize]) const {
    unsigned offset = 0;
    for (int i = 0; i < StringCount; ++i) {
      std::string_view const s{strings<D>[i]};
      for (std::size_t j = 0; j <= s.size(); ++j) data[offset + j] = strings<D>[i][j];
      offsets[2 * i] = offset + sizeof(offsets);
      offsets[2 * i + 1] = static_cast<unsigned>(s.size());
      offset += static_cast<unsigned>(s.size() + 1);
    }
  }
};

// The type of a table entry: an id for the built-in ones, else IsUnresolvedType | name.
template <class D, class T>
consteval unsigned type_id() {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::is_void_v<U>) {
    return QMetaType::Void;
  } else if constexpr (std::is_arithmetic_v<U>) {
    static_assert(QMetaTypeId2<U>::IsBuiltIn, "rqt: this arithmetic type has no QMetaType id");
    return QMetaTypeId2<U>::MetaType;
  } else {
    return QMC::IsUnresolvedType | string_index<D>(type_name(^^U));
  }
}

template <class T>
inline constexpr bool named_type_v = !(std::is_void_v<std::remove_cvref_t<T>> || std::is_arithmetic_v<std::remove_cvref_t<T>>);

// --- method rows -----------------------------------------------------------------------

template <class R, bool Const, class... A>
struct signature_of {
  using type = R(A...);
};
template <class R, class... A>
struct signature_of<R, true, A...> {
  using type = R(A...) const;
};

// The parameter and return types of a table entry, whether it is a function or a signal data member.
template <info M, std::size_t P>
using entry_param_t = typename[:entry_param_type(M, P):];

template <info M>
using entry_return_t = typename[:entry_return_type(M):];

template <info M, std::size_t... P>
auto signature_for(std::index_sequence<P...>)
    -> std::type_identity<typename signature_of<entry_return_t<M>, entry_is_const(M), entry_param_t<M, P>...>::type>;

// The function type of M cut to its first N parameters.
template <info M, std::size_t N>
using signature_t = typename decltype(signature_for<M>(std::make_index_sequence<N>{}))::type;

consteval unsigned access_of(info m) {
  return meta::is_private(m) ? QMC::AccessPrivate : meta::is_protected(m) ? QMC::AccessProtected : QMC::AccessPublic;
}

// Each FunctionData specialization nests its own FunctionParameter, so the array
// is built for the Data type that is constructed.
template <class Data, class D, info M, std::size_t N>
constexpr Data function_data(unsigned flags) {
  using Params = typename Data::ParametersArray;
  constexpr Params params = []<std::size_t... P>(std::index_sequence<P...>) {
    return Params{{{type_id<D, entry_param_t<M, P>>(), string_index<D>(entry_param_name(M, P))}...}};
  }(std::make_index_sequence<N>{});
  return Data(string_index<D>(meta::identifier_of(M)), string_index<D>(""), flags, type_id<D, entry_return_t<M>>(),
              params);
}

template <class D, std::size_t I>
constexpr auto method_data() {
  constexpr method_entry e = method_entries<D>[I];
  using F = signature_t<e.fn, e.nargs>;
  constexpr unsigned flags = access_of(e.fn) | (e.cloned ? QMC::MethodCloned : 0u);
  if constexpr (kind_of(e.fn) == method_kind::signal_)
    return function_data<QtMocHelpers::SignalData<F>, D, e.fn, e.nargs>(flags);
  else if constexpr (kind_of(e.fn) == method_kind::slot_)
    return function_data<QtMocHelpers::SlotData<F>, D, e.fn, e.nargs>(flags);
  else
    return function_data<QtMocHelpers::MethodData<F>, D, e.fn, e.nargs>(flags);
}

// --- property rows ---------------------------------------------------------------------

template <class D, std::size_t I>
constexpr auto property_data() {
  constexpr prop_desc d = property_at<D>(I);
  using T = typename[:d.type:];
  return QtMocHelpers::PropertyData<T>(string_index<D>(d.name.view()), type_id<D, T>(), d.flags,
                                       static_cast<unsigned>(notify_index(^^D, d)));
}

// --- enum rows -------------------------------------------------------------------------

template <class D, info E, std::size_t... V>
constexpr auto enum_data_for(std::index_sequence<V...>) {
  using Enum = typename[:E:];
  constexpr unsigned name = string_index<D>(meta::identifier_of(E));
  constexpr unsigned flags = has<flag_t>(E) ? QMC::EnumIsFlag : 0u;
  if constexpr (sizeof...(V) == 0) {
    return QtMocHelpers::EnumData<Enum>(name, name, flags);
  } else {
    return QtMocHelpers::EnumData<Enum>(name, name, flags)
        .add({{static_cast<int>(string_index<D>(meta::identifier_of(meta::enumerators_of(E)[V]))),
               [:meta::enumerators_of(E)[V]:]}...});
  }
}

template <class D, std::size_t I>
constexpr auto enum_data() {
  constexpr info e = enum_list<D>[I];
  return enum_data_for<D, e>(std::make_index_sequence<meta::enumerators_of(e).size()>{});
}

// --- class info ------------------------------------------------------------------------

template <class D>
constexpr auto classinfo_block() {
  constexpr std::size_t n = classinfo_list<D>.size();
  if constexpr (n == 0) {
    return QtMocHelpers::detail::UintDataBlock<0, 0>{};
  } else {
    return []<std::size_t... I>(std::index_sequence<I...>) {
      std::array<unsigned, 2> const rows[n] = {
          {string_index<D>(classinfo_list<D>[I].key.view()), string_index<D>(classinfo_list<D>[I].value.view())}...};
      return QtMocHelpers::ClassInfos<static_cast<int>(n)>(rows);
    }(std::make_index_sequence<n>{});
  }
}

template <class D>
struct meta_tag {};

template <class D, template <class, std::size_t> class Row, std::size_t N>
constexpr auto make_uint_data() {
  return []<std::size_t... I>(std::index_sequence<I...>) {
    return QtMocHelpers::UintData{Row<D, I>::make()...};
  }(std::make_index_sequence<N>{});
}

template <class D, std::size_t I>
struct method_row {
  static constexpr auto make() { return method_data<D, I>(); }
};
template <class D, std::size_t I>
struct property_row {
  static constexpr auto make() { return property_data<D, I>(); }
};
template <class D, std::size_t I>
struct enum_row {
  static constexpr auto make() { return enum_data<D, I>(); }
};

template <class D>
constexpr auto meta_content = [] {
  auto methods = make_uint_data<D, method_row, method_total<D>>();
  auto properties = make_uint_data<D, property_row, property_count<D>>();
  auto enums = make_uint_data<D, enum_row, enum_list<D>.size()>();
  QtMocHelpers::UintData constructors{};
  return QtMocHelpers::metaObjectData<D, meta_tag<D>>(QMC::MetaObjectFlag{}, string_table<D>{}, methods, properties,
                                                      enums, constructors, classinfo_block<D>());
}();

}  // namespace detail
}  // namespace rqt
