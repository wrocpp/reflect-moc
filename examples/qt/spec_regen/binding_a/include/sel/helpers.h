#pragma once

#include "obs/traits.h"
#include "sel/tracker.h"

#include <QList>
#include <QModelIndex>
#include <QVariant>

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace sel {

namespace detail {

template <class Host>
void notifyRow(Host * host, int first, int last, int role)
{
    host->dataChanged(host->index(first, 0), host->index(last, 0), QList<int>{role});
}

// X is the getter's result with cv-ref removed; an optional<Y> is unwrapped to Y.
template <class X>
struct Unwrapped {
    using type = X;
    template <class R>
    static type get(R && r)
    {
        return static_cast<type>(std::forward<R>(r));
    }
};

template <class Y>
struct Unwrapped<std::optional<Y>> {
    using type = Y;
    template <class R>
    static type get(R && r)
    {
        return r.has_value() ? Y(*std::forward<R>(r)) : Y{};
    }
};

template <auto Getter, class Container>
struct SelectedValueOf {
    using Element = typename std::remove_cvref_t<Container>::value_type;
    using Raw = std::invoke_result_t<decltype(Getter), const Element &>;
    using Wrapper = Unwrapped<std::remove_cvref_t<Raw>>;
    using type = typename Wrapper::type;
};

}  // namespace detail

template <class Host, class T>
    requires SingleHost<Host> && IsTracker<T> && (T::mode == Mode::Single)
bool applySelectionValue(Host * host, T & tracker, const QModelIndex & index, const QVariant & value)
{
    const int row = index.row();
    const bool requested = value.toBool();
    if (tracker.isSelected(row) == requested)
        return true;  // deselecting another row, or selecting the selected one
    const std::optional<int> old = tracker.hasSelection() ? std::optional<int>{tracker.selectedRow()} : std::nullopt;
    const Delta delta = tracker.set(requested ? std::optional<int>{row} : std::nullopt);
    host->selectedRowChanged();
    if (old.has_value())
        detail::notifyRow(host, *old, *old, T::selectedRole());
    if (requested)
        detail::notifyRow(host, row, row, T::selectedRole());
    if (delta.presenceFlipped)
        host->hasSelectionChanged();
    return true;
}

template <class Host, class T>
    requires MultiHost<Host> && IsTracker<T> && (T::mode == Mode::Multi)
bool applySelectionValue(Host * host, T & tracker, const QModelIndex & index, const QVariant & value)
{
    const int row = index.row();
    const Delta delta = tracker.set(row, value.toBool());
    if (delta.selectionChanged)
        detail::notifyRow(host, row, row, T::selectedRole());
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
    const int selected = tracker.selectedRow();
    tracker.clear();
    detail::notifyRow(host, selected, selected, T::selectedRole());
    host->hasSelectionChanged();
    host->selectedRowChanged();
}

template <class Host, class T>
    requires MultiHost<Host> && IsTracker<T> && (T::mode == Mode::Multi)
void clearSelectionAndNotify(Host * host, T & tracker)
{
    if (!tracker.hasSelection())
        return;
    const std::vector<int> rows = tracker.selectedRows();  // before clearing
    tracker.clear();
    for (const auto & [first, last] : obs::contiguousRuns(rows.begin(), rows.end()))
        detail::notifyRow(host, first, last, T::selectedRole());
    host->hasSelectionChanged();
    host->selectedCountChanged();
}

// The container is read through a const reference and never copied.
template <auto Getter, class T, class Container>
    requires (!std::is_pointer_v<Container>)
auto selectedValue(const T & tracker, const Container & container)
{
    static_assert(T::mode == Mode::Single, "selectedValue: not available with multiple selection");
    using Result = typename detail::SelectedValueOf<Getter, Container>;
    if constexpr (T::mode == Mode::Single) {
        if (!tracker.hasSelection())
            return typename Result::type{};
        Q_ASSERT(obs::validIndex(container, tracker.selectedRow()));
        const auto & element = container[static_cast<typename Container::size_type>(tracker.selectedRow())];
        return Result::Wrapper::get(std::invoke(Getter, element));
    }
}

template <auto Getter, class T, class Container>
auto selectedValue(const T & tracker, const Container * container)
{
    static_assert(T::mode == Mode::Single, "selectedValue: not available with multiple selection");
    using Result = typename detail::SelectedValueOf<Getter, Container>;
    if constexpr (T::mode == Mode::Single) {
        if (!tracker.hasSelection())
            return typename Result::type{};
        Q_ASSERT(container != nullptr);
        return selectedValue<Getter>(tracker, *container);
    }
}

}  // namespace sel
