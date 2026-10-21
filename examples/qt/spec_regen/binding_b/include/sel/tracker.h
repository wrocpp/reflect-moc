// The selection value type, its Delta and the host concepts (SPEC 6.3, 10.1 to 10.4).
#pragma once

#include "roles.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QtGlobal>

#include <algorithm>
#include <concepts>
#include <optional>
#include <type_traits>
#include <vector>

namespace sel {

enum class Mode { Single, Multi };

// What a mutation changed. The three differ on purpose: a second ticked row changes the count, not the presence.
struct Delta {
    bool presenceFlipped = false;
    bool selectionChanged = false;
    bool countChanged = false;
};

template <Mode M, auto Roles>
    requires ValidRoleList<Roles>
class Tracker {
public:
    static constexpr Mode mode = M;

    // The largest base role plus one, so it cannot collide with the base model's roles.
    static consteval int selectedRole()
    {
        long long largest = 0;
        for (const auto & r : Roles)
            largest = std::max(largest, detail::roleValue(r));
        return static_cast<int>(largest + 1);
    }

    static constexpr const char * selectedRoleName() { return "isSelected"; }

    static QHash<int, QByteArray> & appendRoleNames(QHash<int, QByteArray> & names)
    {
        Q_ASSERT(!names.contains(selectedRole()));
        Q_ASSERT(!nameTaken(names));
        names.insert(selectedRole(), QByteArray(selectedRoleName()));
        return names;
    }

    bool hasSelection() const
    {
        if constexpr (M == Mode::Single)
            return state_.has_value();
        else
            return selected_ > 0;
    }

    int count() const
    {
        if constexpr (M == Mode::Single)
            return state_.has_value() ? 1 : 0;
        else
            return selected_;
    }

    bool isSelected(int row) const
    {
        if constexpr (M == Mode::Single) {
            return state_.has_value() && *state_ == row;
        } else {
            Q_ASSERT(row >= 0 && row < rowCount());
            return state_[static_cast<std::size_t>(row)] != 0;
        }
    }

    void clear()
    {
        if constexpr (M == Mode::Single) {
            state_.reset();
        } else {
            std::fill(state_.begin(), state_.end(), char{0});
            selected_ = 0;
        }
    }

    void rowInserted(int row)
    {
        if constexpr (M == Mode::Single) {
            if (state_.has_value() && *state_ >= row)
                ++*state_;
        } else {
            Q_ASSERT(row >= 0 && row <= rowCount());
            state_.insert(state_.begin() + row, char{0});
        }
    }

    Delta rowRemoved(int row)
    {
        if constexpr (M == Mode::Single) {
            if (!state_.has_value() || *state_ < row)
                return {};
            if (*state_ == row) {
                state_.reset();
                return {.presenceFlipped = true, .selectionChanged = true, .countChanged = true};
            }
            --*state_;
            return {.presenceFlipped = false, .selectionChanged = true, .countChanged = false};
        } else {
            Q_ASSERT(row >= 0 && row < rowCount());
            const bool had = hasSelection();
            const bool wasSelected = isSelected(row);
            state_.erase(state_.begin() + row);
            if (wasSelected)
                --selected_;
            return {.presenceFlipped = had != hasSelection(), .selectionChanged = wasSelected, .countChanged = wasSelected};
        }
    }

    void reset([[maybe_unused]] int rows)
    {
        if constexpr (M == Mode::Single) {
            state_.reset();
        } else {
            state_.assign(static_cast<std::size_t>(rows), char{0});
            selected_ = 0;
        }
    }

    // Single only
    int selectedRow() const
        requires (M == Mode::Single)
    {
        Q_ASSERT(hasSelection());
        return *state_;
    }

    Delta set(std::optional<int> row)
        requires (M == Mode::Single)
    {
        if (state_ == row)
            return {};
        const bool had = hasSelection();
        state_ = row;
        const bool flipped = had != hasSelection();
        return {.presenceFlipped = flipped, .selectionChanged = true, .countChanged = flipped};
    }

    // Multi only
    std::vector<int> selectedRows() const
        requires (M == Mode::Multi)
    {
        std::vector<int> rows;
        rows.reserve(static_cast<std::size_t>(selected_));
        for (std::size_t i = 0; i < state_.size(); ++i)
            if (state_[i] != 0)
                rows.push_back(static_cast<int>(i));
        return rows;
    }

    int rowCount() const
        requires (M == Mode::Multi)
    {
        return static_cast<int>(state_.size());
    }

    Delta set(int row, bool selected)
        requires (M == Mode::Multi)
    {
        Q_ASSERT(row >= 0 && row < rowCount());
        if (isSelected(row) == selected)
            return {};
        const bool had = hasSelection();
        state_[static_cast<std::size_t>(row)] = selected ? char{1} : char{0};
        selected_ += selected ? 1 : -1;
        return {.presenceFlipped = had != hasSelection(), .selectionChanged = true, .countChanged = true};
    }

private:
    static bool nameTaken(const QHash<int, QByteArray> & names)
    {
        for (const QByteArray & n : names)
            if (n == selectedRoleName())
                return true;
        return false;
    }

    // char, not bool: std::vector<bool> is not a container of bytes
    std::conditional_t<M == Mode::Single, std::optional<int>, std::vector<char>> state_{};
    int selected_ = 0;  // Multi only: the number of set flags
};

namespace detail {
template <class T>
struct is_tracker : std::false_type {};
template <Mode M, auto Roles>
struct is_tracker<Tracker<M, Roles>> : std::true_type {};
}  // namespace detail

template <class T>
concept IsTracker = detail::is_tracker<std::remove_cvref_t<T>>::value;

template <class T>
concept SingleHost = std::derived_from<T, QAbstractItemModel> && requires(T & t) {
    t.hasSelectionChanged();
    t.selectedRowChanged();
};

template <class T>
concept MultiHost = std::derived_from<T, QAbstractItemModel> && requires(T & t) {
    t.hasSelectionChanged();
    t.selectedCountChanged();
};

}  // namespace sel
