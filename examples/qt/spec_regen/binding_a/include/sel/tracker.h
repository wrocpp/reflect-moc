#pragma once

#include "sel/roles.h"

#include <QAbstractItemModel>
#include <QByteArray>
#include <QHash>

#include <algorithm>
#include <concepts>
#include <optional>
#include <type_traits>
#include <vector>

namespace sel {

enum class Mode { Single, Multi };

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

    // The largest base role plus one, so it cannot collide with the base's roles.
    static consteval int selectedRole()
    {
        long long largest = detail::roleValue(Roles[0]);
        for (const auto role : Roles)
            largest = std::max(largest, detail::roleValue(role));
        return static_cast<int>(largest) + 1;
    }

    static constexpr const char * selectedRoleName() { return "isSelected"; }

    static QHash<int, QByteArray> & appendRoleNames(QHash<int, QByteArray> & names)
    {
        Q_ASSERT(!names.contains(selectedRole()));
        Q_ASSERT(!hasName(names));
        names.insert(selectedRole(), selectedRoleName());
        return names;
    }

    bool hasSelection() const { return count() > 0; }

    int count() const
    {
        if constexpr (M == Mode::Single)
            return m_storage.has_value() ? 1 : 0;
        else
            return m_selectedCount;
    }

    bool isSelected(int row) const
    {
        if constexpr (M == Mode::Single) {
            return m_storage.has_value() && *m_storage == row;
        } else {
            Q_ASSERT(row >= 0 && row < rowCount());
            return m_storage[static_cast<std::size_t>(row)] != 0;
        }
    }

    void clear()
    {
        if constexpr (M == Mode::Single) {
            m_storage.reset();
        } else {
            std::ranges::fill(m_storage, char{0});
            m_selectedCount = 0;
        }
    }

    void rowInserted(int row)
    {
        if constexpr (M == Mode::Single) {
            if (m_storage.has_value() && *m_storage >= row)
                ++*m_storage;
        } else {
            Q_ASSERT(row >= 0 && row <= rowCount());
            m_storage.insert(m_storage.begin() + row, char{0});
        }
    }

    Delta rowRemoved(int row)
    {
        if constexpr (M == Mode::Single) {
            if (!m_storage.has_value() || *m_storage < row)
                return Delta{};
            if (*m_storage == row) {
                m_storage.reset();
                return Delta{.presenceFlipped = true, .selectionChanged = true, .countChanged = true};
            }
            --*m_storage;
            return Delta{.presenceFlipped = false, .selectionChanged = true, .countChanged = false};
        } else {
            Q_ASSERT(row >= 0 && row < rowCount());
            const bool had = hasSelection();
            const bool wasSelected = m_storage[static_cast<std::size_t>(row)] != 0;
            m_storage.erase(m_storage.begin() + row);
            if (wasSelected)
                --m_selectedCount;
            return Delta{.presenceFlipped = had != hasSelection(),
                         .selectionChanged = wasSelected,
                         .countChanged = wasSelected};
        }
    }

    void reset(int rows)
    {
        if constexpr (M == Mode::Single) {
            (void) rows;
            m_storage.reset();
        } else {
            m_storage.assign(static_cast<std::size_t>(rows), char{0});
            m_selectedCount = 0;
        }
    }

    // Single only
    int selectedRow() const
        requires (M == Mode::Single)
    {
        Q_ASSERT(m_storage.has_value());
        return *m_storage;
    }

    Delta set(std::optional<int> row)
        requires (M == Mode::Single)
    {
        if (m_storage == row)
            return Delta{};
        const bool had = m_storage.has_value();
        m_storage = row;
        const bool flipped = had != m_storage.has_value();
        return Delta{.presenceFlipped = flipped, .selectionChanged = true, .countChanged = flipped};
    }

    // Multi only
    std::vector<int> selectedRows() const
        requires (M == Mode::Multi)
    {
        std::vector<int> rows;
        for (std::size_t i = 0; i < m_storage.size(); ++i) {
            if (m_storage[i] != 0)
                rows.push_back(static_cast<int>(i));
        }
        return rows;
    }

    int rowCount() const
        requires (M == Mode::Multi)
    {
        return static_cast<int>(m_storage.size());
    }

    Delta set(int row, bool selected)
        requires (M == Mode::Multi)
    {
        Q_ASSERT(row >= 0 && row < rowCount());
        char & flag = m_storage[static_cast<std::size_t>(row)];
        if ((flag != 0) == selected)
            return Delta{};
        const bool had = hasSelection();
        flag = selected ? char{1} : char{0};
        m_selectedCount += selected ? 1 : -1;
        return Delta{.presenceFlipped = had != hasSelection(), .selectionChanged = true, .countChanged = true};
    }

private:
    static bool hasName(const QHash<int, QByteArray> & names)
    {
        for (const QByteArray & n : names) {
            if (n == selectedRoleName())
                return true;
        }
        return false;
    }

    std::conditional_t<M == Mode::Single, std::optional<int>, std::vector<char>> m_storage{};
    int m_selectedCount = 0;
};

namespace detail {

template <class T>
inline constexpr bool isTracker = false;

template <Mode M, auto Roles>
inline constexpr bool isTracker<Tracker<M, Roles>> = true;

}  // namespace detail

template <class T>
concept IsTracker = detail::isTracker<std::remove_cvref_t<T>>;

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
