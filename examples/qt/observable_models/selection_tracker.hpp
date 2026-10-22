// The selection state of a list model, kept in the model itself: which rows are selected, in a form that follows the rows
// when they come and go, plus the helpers that tell the host's views what changed in the order the contract asks for.
//
//  - SelectionTracker<Mode, Roles> holds the state. Single holds at most one row, Multi one flag per row. It is a plain
//    value, not a QObject: it has no signals, so a host (a model) owns one and announces what a mutation changed with
//    its own rqt::signal members. Every mutation returns a SelectionDelta, which says what changed: the PRESENCE of a
//    selection (none to some, some to none), the SELECTION itself, and the COUNT. A second row ticked in a Multi tracker
//    changes the count and the selection, not the presence.
//  - The roles the host adds for the selection are a std::array of custom roles (strictly greater than Qt::UserRole, all
//    distinct), checked at compile time by ValidRoleList; the tracker's own role (selectedRole, named "isSelected" for
//    QML) is the largest of them plus one, so it cannot collide with them.
//  - applySelectionValue (the setData branch of the selected role), clearSelectionAndNotify and selectedValue are free
//    functions over a host and a tracker; the host is a QAbstractItemModel with rqt::signal members hasSelectionChanged and
//    selectedRowChanged (Single) or selectedCountChanged (Multi), which the SingleHost and MultiHost concepts check.
//
// The order of what a host announces for a change of the selection: the "selected row changed" or "count changed"
// notification, then dataChanged for the rows whose isSelected changed (named by the selected role alone), then "has selection
// changed" if the presence flipped. A host that follows the rows of a container keeps the tracker in step in the
// model's own signals (rowsInserted, rowsRemoved, modelReset), so the selection already points right when a view hears
// of the change, and announces its notifications afterwards.
#pragma once

#include "container_traits.hpp"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>
#include <QtCore/Qt>
#include <QtCore/QtGlobal>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace observable {

// ---- role lists ---------------------------------------------------------------------------------------------------

namespace detail {

template <class T>
struct is_std_array : std::false_type {};
template <class E, std::size_t N>
struct is_std_array<std::array<E, N>> : std::true_type {};

// The underlying type for an enum, the type itself otherwise; the enum case is chosen through a defaulted bool, so
// std::underlying_type is never instantiated on a non-enum.
template <class T, bool IsEnum = std::is_enum_v<T>>
struct role_underlying {
  using type = T;
};
template <class T>
struct role_underlying<T, true> {
  using type = std::underlying_type_t<T>;
};

template <class R>
consteval long long role_value(R role) {
  return static_cast<long long>(static_cast<typename role_underlying<R>::type>(role));
}

}  // namespace detail

template <class T>
inline constexpr bool is_std_array_v = detail::is_std_array<std::remove_cvref_t<T>>::value;

// STRICTLY greater than Qt::UserRole: UserRole itself (the key role of the container models) and every built-in role are rejected.
template <class R>
consteval bool is_custom_role(R role) {
  return detail::role_value(role) > static_cast<long long>(Qt::UserRole);
}

template <class E, std::size_t N>
consteval bool all_distinct(const std::array<E, N>& roles) {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j)
      if (detail::role_value(roles[i]) == detail::role_value(roles[j])) return false;
  return true;
}

namespace detail {
template <auto Roles>
consteval bool all_custom() {
  if constexpr (is_std_array_v<decltype(Roles)>) {
    for (const auto& r : Roles)
      if (!is_custom_role(r)) return false;
    return true;
  } else {
    return false;
  }
}
}  // namespace detail

// The role list of a host: a non-empty std::array of distinct custom roles, as a constant template argument. The array test comes
// first, so a non-array argument is false, not an error.
template <auto Roles>
concept ValidRoleList = is_std_array_v<decltype(Roles)> && (Roles.size() > 0) && detail::all_custom<Roles>() && all_distinct(Roles);

// ---- the tracker --------------------------------------------------------------------------------------------------

enum class SelectionMode { Single, Multi };

// What a mutation changed. The three differ on purpose: a second ticked row changes the count, not the presence.
struct SelectionDelta {
  bool presenceFlipped = false;
  bool selectionChanged = false;
  bool countChanged = false;
  bool operator==(const SelectionDelta&) const = default;
};

template <SelectionMode M, auto Roles>
  requires ValidRoleList<Roles>
class SelectionTracker {
 public:
  static constexpr SelectionMode mode = M;

  // The largest base role plus one, so it cannot collide with the host's own roles.
  static consteval int selectedRole() {
    long long largest = 0;
    for (const auto& r : Roles) largest = std::max(largest, detail::role_value(r));
    return static_cast<int>(largest + 1);
  }

  static constexpr const char* selectedRoleName() { return "isSelected"; }

  // Adds the selected role to the host's role names. The role and the name must not be taken.
  static QHash<int, QByteArray>& appendRoleNames(QHash<int, QByteArray>& names) {
    Q_ASSERT(!names.contains(selectedRole()));
    Q_ASSERT(!nameTaken(names));
    names.insert(selectedRole(), QByteArray(selectedRoleName()));
    return names;
  }

  bool hasSelection() const {
    if constexpr (M == SelectionMode::Single)
      return m_state.has_value();
    else
      return m_selected > 0;
  }

  int count() const {
    if constexpr (M == SelectionMode::Single)
      return m_state.has_value() ? 1 : 0;
    else
      return m_selected;
  }

  bool isSelected(int row) const {
    if constexpr (M == SelectionMode::Single) {
      return m_state.has_value() && *m_state == row;
    } else {
      Q_ASSERT(row >= 0 && row < rowCount());
      return row >= 0 && row < rowCount() && m_state[static_cast<std::size_t>(row)] != 0;
    }
  }

  // Drops the selection and keeps the rows.
  void clear() {
    if constexpr (M == SelectionMode::Single) {
      m_state.reset();
    } else {
      std::fill(m_state.begin(), m_state.end(), char{0});
      m_selected = 0;
    }
  }

  // `count` rows inserted at `first`: the selected rows after them move down. True when a selected row moved.
  bool rowsInserted(int first, int count) {
    if constexpr (M == SelectionMode::Single) {
      if (!m_state.has_value() || *m_state < first) return false;
      *m_state += count;
      return true;
    } else {
      Q_ASSERT(first >= 0 && first <= rowCount() && count >= 0);
      m_state.insert(m_state.begin() + std::clamp(first, 0, rowCount()), static_cast<std::size_t>(count), char{0});
      return false;
    }
  }
  void rowInserted(int row) { rowsInserted(row, 1); }

  // Rows `first` to `last` removed. The delta says what that did to the selection.
  SelectionDelta rowsRemoved(int first, int last) {
    if constexpr (M == SelectionMode::Single) {
      if (!m_state.has_value() || *m_state < first) return {};
      if (*m_state <= last) {
        m_state.reset();
        return {.presenceFlipped = true, .selectionChanged = true, .countChanged = true};
      }
      *m_state -= last - first + 1;
      return {.presenceFlipped = false, .selectionChanged = true, .countChanged = false};
    } else {
      Q_ASSERT(first >= 0 && last < rowCount() && first <= last);
      const bool had = hasSelection();
      const auto from = m_state.begin() + first;
      const int removedSelected = static_cast<int>(std::count(from, from + (last - first + 1), char{1}));
      m_state.erase(from, from + (last - first + 1));
      m_selected -= removedSelected;
      return {.presenceFlipped = had != hasSelection(), .selectionChanged = removedSelected > 0, .countChanged = removedSelected > 0};
    }
  }
  SelectionDelta rowRemoved(int row) { return rowsRemoved(row, row); }

  // The host's rows were replaced: nothing is selected, and a Multi tracker has `rows` rows to flag.
  void reset([[maybe_unused]] int rows) {
    if constexpr (M == SelectionMode::Single) {
      m_state.reset();
    } else {
      m_state.assign(static_cast<std::size_t>(rows), char{0});
      m_selected = 0;
    }
  }

  // Single only
  int selectedRow() const
    requires(M == SelectionMode::Single)
  {
    Q_ASSERT(hasSelection());
    return m_state.value_or(-1);
  }

  SelectionDelta set(std::optional<int> row)
    requires(M == SelectionMode::Single)
  {
    if (m_state == row) return {};
    const bool had = hasSelection();
    m_state = row;
    const bool flipped = had != hasSelection();
    return {.presenceFlipped = flipped, .selectionChanged = true, .countChanged = flipped};
  }

  // Multi only
  std::vector<int> selectedRows() const
    requires(M == SelectionMode::Multi)
  {
    std::vector<int> rows;
    rows.reserve(static_cast<std::size_t>(m_selected));
    for (std::size_t i = 0; i < m_state.size(); ++i)
      if (m_state[i] != 0) rows.push_back(static_cast<int>(i));
    return rows;
  }

  int rowCount() const
    requires(M == SelectionMode::Multi)
  {
    return to_row(static_cast<qsizetype>(m_state.size()));
  }

  SelectionDelta set(int row, bool selected)
    requires(M == SelectionMode::Multi)
  {
    Q_ASSERT(row >= 0 && row < rowCount());
    if (row < 0 || row >= rowCount() || isSelected(row) == selected) return {};
    const bool had = hasSelection();
    m_state[static_cast<std::size_t>(row)] = selected ? char{1} : char{0};
    m_selected += selected ? 1 : -1;
    return {.presenceFlipped = had != hasSelection(), .selectionChanged = true, .countChanged = true};
  }

 private:
  static bool nameTaken(const QHash<int, QByteArray>& names) {
    for (const QByteArray& n : names)
      if (n == selectedRoleName()) return true;
    return false;
  }

  // char, not bool: std::vector<bool> is not a container of bytes
  std::conditional_t<M == SelectionMode::Single, std::optional<int>, std::vector<char>> m_state{};
  int m_selected = 0;  // Multi only: the number of set flags
};

namespace detail {
template <class T>
struct is_selection_tracker : std::false_type {};
template <SelectionMode M, auto Roles>
struct is_selection_tracker<SelectionTracker<M, Roles>> : std::true_type {};
}  // namespace detail

template <class T>
concept IsSelectionTracker = detail::is_selection_tracker<std::remove_cvref_t<T>>::value;

// A host that can announce a Single tracker's changes, or a Multi tracker's: a model with these notifications as members.
template <class T>
concept SingleHost = std::derived_from<T, QAbstractItemModel> && requires(T& t) {
  t.hasSelectionChanged();
  t.selectedRowChanged();
};

template <class T>
concept MultiHost = std::derived_from<T, QAbstractItemModel> && requires(T& t) {
  t.hasSelectionChanged();
  t.selectedCountChanged();
};

// ---- the helpers ----------------------------------------------------------------------------------------------------

// Inclusive runs of an ascending, duplicate-free range of rows.
inline std::vector<std::pair<int, int>> row_runs(const std::vector<int>& ascending) {
  std::vector<std::pair<int, int>> runs;
  for (const int row : ascending) {
    if (!runs.empty() && runs.back().second + 1 == row)
      runs.back().second = row;
    else
      runs.emplace_back(row, row);
  }
  return runs;
}

// The setData branch for the selected role. Precondition: the caller validated the index and the role. Always true: a request
// that changes nothing (the row is selected already, or another row is deselected) says nothing.
template <class Host, class T>
  requires SingleHost<Host> && IsSelectionTracker<T> && (T::mode == SelectionMode::Single)
bool applySelectionValue(Host* host, T& tracker, const QModelIndex& index, const QVariant& value) {
  const int row = index.row();
  const bool requested = value.toBool();
  const bool isThatRow = tracker.hasSelection() && tracker.selectedRow() == row;
  if (requested == isThatRow) return true;  // false on another row, or true on the selected row

  const std::optional<int> old = tracker.hasSelection() ? std::optional<int>{tracker.selectedRow()} : std::nullopt;
  const SelectionDelta delta = tracker.set(requested ? std::optional<int>{row} : std::nullopt);
  const QList<int> roles{T::selectedRole()};

  host->selectedRowChanged();
  if (old.has_value()) host->dataChanged(host->index(*old, 0), host->index(*old, 0), roles);
  if (requested) host->dataChanged(host->index(row, 0), host->index(row, 0), roles);
  if (delta.presenceFlipped) host->hasSelectionChanged();
  return true;
}

template <class Host, class T>
  requires MultiHost<Host> && IsSelectionTracker<T> && (T::mode == SelectionMode::Multi)
bool applySelectionValue(Host* host, T& tracker, const QModelIndex& index, const QVariant& value) {
  const SelectionDelta delta = tracker.set(index.row(), value.toBool());
  const QList<int> roles{T::selectedRole()};

  if (delta.selectionChanged) host->dataChanged(index, index, roles);
  if (delta.countChanged) host->selectedCountChanged();
  if (delta.presenceFlipped) host->hasSelectionChanged();
  return true;
}

template <class Host, class T>
  requires SingleHost<Host> && IsSelectionTracker<T> && (T::mode == SelectionMode::Single)
void clearSelectionAndNotify(Host* host, T& tracker) {
  if (!tracker.hasSelection()) return;
  const int row = tracker.selectedRow();
  tracker.clear();

  host->dataChanged(host->index(row, 0), host->index(row, 0), QList<int>{T::selectedRole()});
  host->hasSelectionChanged();
  host->selectedRowChanged();
}

template <class Host, class T>
  requires MultiHost<Host> && IsSelectionTracker<T> && (T::mode == SelectionMode::Multi)
void clearSelectionAndNotify(Host* host, T& tracker) {
  if (!tracker.hasSelection()) return;
  // the rows must be read BEFORE clear(): afterwards nothing says which rows were selected
  const std::vector<int> rows = tracker.selectedRows();
  tracker.clear();

  const QList<int> roles{T::selectedRole()};
  for (const auto& [first, last] : row_runs(rows)) host->dataChanged(host->index(first, 0), host->index(last, 0), roles);
  host->hasSelectionChanged();
  host->selectedCountChanged();
}

namespace detail {

template <class R>
struct unwrap_optional {
  using type = std::remove_cvref_t<R>;
};
template <class X>
struct unwrap_optional<std::optional<X>> {
  using type = X;
};

// The getter's result with cv-ref removed and an std::optional unwrapped.
template <auto Getter, class Container>
using SelectedResult = typename unwrap_optional<std::remove_cvref_t<std::invoke_result_t<decltype(Getter), const typename Container::value_type&>>>::type;

template <class T>
inline constexpr bool is_optional = false;
template <class X>
inline constexpr bool is_optional<std::optional<X>> = true;

}  // namespace detail

// What the selected row of a Single tracker is, read from a random-access sequential container (a QList, a std::vector) that is
// borrowed by reference and never copied: `Getter` is a member pointer or a callable of the element. Nothing selected gives a
// default value (an empty std::optional too), and the container is not read.
template <auto Getter, class T, class Container>
  requires(!std::is_pointer_v<Container>)
auto selectedValue(const T& tracker, const Container& container) {
  static_assert(IsSelectionTracker<T>, "selectedValue needs an observable::SelectionTracker");
  static_assert(T::mode == SelectionMode::Single, "selectedValue: a Multi tracker holds multiple selection, so there is no single value to return");
  using Result = detail::SelectedResult<Getter, Container>;
  if constexpr (T::mode == SelectionMode::Single) {
    if (!tracker.hasSelection()) return Result{};
    const int row = tracker.selectedRow();
    Q_ASSERT(row >= 0 && static_cast<std::size_t>(row) < static_cast<std::size_t>(container.size()));
    const auto& element = container[static_cast<typename Container::size_type>(row)];
    if constexpr (detail::is_optional<std::remove_cvref_t<std::invoke_result_t<decltype(Getter), decltype(element)>>>) {
      auto&& got = std::invoke(Getter, element);
      return got.has_value() ? Result(*got) : Result{};
    } else {
      return Result(std::invoke(Getter, element));
    }
  }
}

// The same through a pointer to the container, which may be null while nothing is selected.
template <auto Getter, class T, class Container>
auto selectedValue(const T& tracker, const Container* container) {
  static_assert(IsSelectionTracker<T>, "selectedValue needs an observable::SelectionTracker");
  static_assert(T::mode == SelectionMode::Single, "selectedValue: a Multi tracker holds multiple selection, so there is no single value to return");
  using Result = detail::SelectedResult<Getter, Container>;
  if constexpr (T::mode == SelectionMode::Single) {
    if (!tracker.hasSelection()) return Result{};  // the container is not read: the pointer may be null
    Q_ASSERT(container != nullptr);
    return selectedValue<Getter>(tracker, *container);
  }
}

}  // namespace observable
