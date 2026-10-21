// The helpers that emit the host's notifications in the contract order (SPEC 10.5).
#pragma once

#include "tracker.h"

#include "../obs/traits.h"

#include <QtCore/QList>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>

#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>

namespace sel {
namespace detail {

template <class R>
struct unwrapOptional {
    using type = std::remove_cvref_t<R>;
};
template <class X>
struct unwrapOptional<std::optional<X>> {
    using type = X;
};

// The getter's result with cv-ref removed and an std::optional unwrapped.
template <auto Getter, class Container>
using SelectedResult = typename unwrapOptional<
    std::remove_cvref_t<std::invoke_result_t<decltype(Getter), const typename Container::value_type &>>>::type;

template <class T>
inline constexpr bool isOptional = false;
template <class X>
inline constexpr bool isOptional<std::optional<X>> = true;

}  // namespace detail

// The setData branch for selectedRole(). Precondition: the caller validated the index and the role.
template <class Host, class T>
    requires SingleHost<Host> && IsTracker<T> && (T::mode == Mode::Single)
bool applySelectionValue(Host * host, T & tracker, const QModelIndex & index, const QVariant & value)
{
    const int row = index.row();
    const bool requested = value.toBool();
    const bool isThatRow = tracker.hasSelection() && tracker.selectedRow() == row;
    if (requested == isThatRow)  // false on another row, or true on the selected row
        return true;

    const std::optional<int> old = tracker.hasSelection() ? std::optional<int>{tracker.selectedRow()} : std::nullopt;
    const Delta delta = tracker.set(requested ? std::optional<int>{row} : std::nullopt);
    const QList<int> roles{T::selectedRole()};

    host->selectedRowChanged();
    if (old.has_value())
        host->dataChanged(host->index(*old, 0), host->index(*old, 0), roles);
    if (requested)
        host->dataChanged(host->index(row, 0), host->index(row, 0), roles);
    if (delta.presenceFlipped)
        host->hasSelectionChanged();
    return true;
}

template <class Host, class T>
    requires MultiHost<Host> && IsTracker<T> && (T::mode == Mode::Multi)
bool applySelectionValue(Host * host, T & tracker, const QModelIndex & index, const QVariant & value)
{
    const Delta delta = tracker.set(index.row(), value.toBool());
    const QList<int> roles{T::selectedRole()};

    if (delta.selectionChanged)
        host->dataChanged(index, index, roles);
    if (delta.countChanged)
        host->selectedCountChanged();
    if (delta.presenceFlipped)
        host->hasSelectionChanged();
    return true;
}

template <class Host, class T>
    requires SingleHost<Host> && IsTracker<T> && (T::mode == Mode::Single)
void clearSelectionAndNotify(Host * host, T & tracker)
{
    if (!tracker.hasSelection())
        return;
    const int row = tracker.selectedRow();
    tracker.clear();

    host->dataChanged(host->index(row, 0), host->index(row, 0), QList<int>{T::selectedRole()});
    host->hasSelectionChanged();
    host->selectedRowChanged();
}

template <class Host, class T>
    requires MultiHost<Host> && IsTracker<T> && (T::mode == Mode::Multi)
void clearSelectionAndNotify(Host * host, T & tracker)
{
    if (!tracker.hasSelection())
        return;
    // the rows must be read BEFORE clear(): afterwards nothing says which rows were selected
    const std::vector<int> rows = tracker.selectedRows();
    tracker.clear();

    const QList<int> roles{T::selectedRole()};
    for (const auto & [first, last] : obs::contiguousRuns(rows.begin(), rows.end()))
        host->dataChanged(host->index(first, 0), host->index(last, 0), roles);
    host->hasSelectionChanged();
    host->selectedCountChanged();
}

// The container is a random-access sequential container, borrowed by reference and never copied.
template <auto Getter, class T, class Container>
    requires (!std::is_pointer_v<Container>)
auto selectedValue(const T & tracker, const Container & container)
{
    static_assert(IsTracker<T>, "selectedValue needs a sel::Tracker");
    static_assert(T::mode == Mode::Single, "selectedValue: a Multi tracker holds multiple selection, so there is no single value to return");
    using Result = detail::SelectedResult<Getter, Container>;
    if constexpr (T::mode == Mode::Single) {
        if (!tracker.hasSelection())
            return Result{};
        const int row = tracker.selectedRow();
        Q_ASSERT(row >= 0 && static_cast<std::size_t>(row) < static_cast<std::size_t>(container.size()));
        const auto & element = container[static_cast<typename Container::size_type>(row)];
        if constexpr (detail::isOptional<std::remove_cvref_t<std::invoke_result_t<decltype(Getter), decltype(element)>>>) {
            auto && got = std::invoke(Getter, element);
            return got.has_value() ? Result(*got) : Result{};
        } else {
            return Result(std::invoke(Getter, element));
        }
    }
}

template <auto Getter, class T, class Container>
auto selectedValue(const T & tracker, const Container * container)
{
    static_assert(IsTracker<T>, "selectedValue needs a sel::Tracker");
    static_assert(T::mode == Mode::Single, "selectedValue: a Multi tracker holds multiple selection, so there is no single value to return");
    using Result = detail::SelectedResult<Getter, Container>;
    if constexpr (T::mode == Mode::Single) {
        if (!tracker.hasSelection())
            return Result{};  // the container is not read: the pointer may be null
        Q_ASSERT(container != nullptr);
        return selectedValue<Getter>(tracker, *container);
    }
}

}  // namespace sel
