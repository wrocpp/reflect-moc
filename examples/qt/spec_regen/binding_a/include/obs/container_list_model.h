#pragma once

#include "obs/container.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QModelIndex>
#include <QVariant>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace obs {

template <class C, class Less = std::less<KeyOf<C>>>
    requires SupportedContainer<C>
class ContainerListModel : public QAbstractListModel {
public:
    using Key = KeyOf<C>;
    using Value = ValueOf<C>;

    explicit ContainerListModel(Container<C> & source, QObject * parent = nullptr)
        : QAbstractListModel(parent), m_source(source)
    {
        static_assert(HashableKey<Key>, "the list model needs a hashable key for its row map");
        rebuild();
        connectToSource();
    }

    int rowCount(const QModelIndex & parent = {}) const override
    {
        return parent.isValid() ? 0 : ownCount();
    }

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override
    {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};
        const Key key = keyAt(index.row());
        const Value * value = m_source.find(key);
        if (value == nullptr)
            return {};
        return valueData(key, *value, role);
    }

    Key keyAt(int row) const
    {
        Q_ASSERT(row >= 0 && row < rowCount());
        if constexpr (SequentialContainer<C>)
            return static_cast<Key>(row);
        else
            return m_keys[static_cast<std::size_t>(row)];
    }

    int rowOf(const Key & key) const
    {
        if constexpr (SequentialContainer<C>) {
            return key >= 0 && key < m_count ? static_cast<int>(key) : -1;
        } else {
            if (m_stale) {
                m_rowOf.clear();
                for (std::size_t i = 0; i < m_keys.size(); ++i)
                    m_rowOf.insert(m_keys[i], static_cast<int>(i));
                m_stale = false;
            }
            const auto it = m_rowOf.constFind(key);
            return it == m_rowOf.constEnd() ? -1 : it.value();
        }
    }

protected:
    virtual QVariant valueData(const Key & key, const Value & value, int role) const = 0;

private:
    using CS = ContainerSignals;

    int ownCount() const
    {
        if constexpr (SequentialContainer<C>)
            return m_count;
        else
            return static_cast<int>(m_keys.size());
    }

    // The row a key has, or would have, in the sorted row list.
    int sortedPosition(const Key & key) const
    {
        return static_cast<int>(std::lower_bound(m_keys.begin(), m_keys.end(), key, Less{}) - m_keys.begin());
    }

    void rebuild()
    {
        if constexpr (SequentialContainer<C>) {
            m_count = static_cast<int>(m_source.size());
        } else {
            m_keys.clear();
            detail::forEachKey(m_source.data(), [this](const Key & k) { m_keys.push_back(k); });
            std::stable_sort(m_keys.begin(), m_keys.end(), Less{});
            m_stale = true;
        }
    }

    void emptyRows()
    {
        if constexpr (SequentialContainer<C>) {
            m_count = 0;
        } else {
            m_keys.clear();
            m_stale = true;
        }
    }

    void connectToSource()
    {
        Container<C> * s = &m_source;
        connect(s, &CS::entryAboutToBeAdded, this, [this](const QVariant & v) {
            const Key k = detail::unbox<Key>(v);
            m_pending = positionForNew(k);
            beginInsertRows({}, m_pending, m_pending);
        });
        connect(s, &CS::entryAdded, this, [this]([[maybe_unused]] const QVariant & v) {
            if constexpr (SequentialContainer<C>) {
                ++m_count;
            } else {
                m_keys.insert(m_keys.begin() + m_pending, detail::unbox<Key>(v));
                m_stale = true;
            }
            endInsertRows();
        });
        connect(s, &CS::entryAboutToBeRemoved, this, [this](const QVariant & v) {
            m_pending = rowOf(detail::unbox<Key>(v));
            beginRemoveRows({}, m_pending, m_pending);
        });
        connect(s, &CS::entryRemoved, this, [this](const QVariant &) {
            if constexpr (SequentialContainer<C>) {
                --m_count;
            } else {
                m_keys.erase(m_keys.begin() + m_pending);
                m_stale = true;
            }
            endRemoveRows();
        });
        connect(s, &CS::entryDataChanged, this, [this](const QVariant & v, const QList<int> & roles) {
            const int row = rowOf(detail::unbox<Key>(v));
            if (row >= 0 && row < ownCount())
                emit dataChanged(index(row), index(row), roles);
        });
        connect(s, &CS::aboutToBeCleared, this, [this] { beginResetModel(); });
        connect(s, &CS::cleared, this, [this] {
            emptyRows();
            endResetModel();
        });
        connect(s, &CS::aboutToBeReset, this, [this] { beginResetModel(); });
        connect(s, &CS::resetCompleted, this, [this] {
            rebuild();
            endResetModel();
        });
        if constexpr (AssociativeContainer<C>) {
            connect(s, &CS::entriesAdded, this,
                    [this](const QList<QVariant> & keys) { onEntriesAdded(detail::unboxAll<Key>(keys)); });
            connect(s, &CS::entriesRemoved, this,
                    [this](const QList<QVariant> & keys) { onEntriesRemoved(detail::unboxAll<Key>(keys)); });
        }
    }

    int positionForNew(const Key & key) const
    {
        if constexpr (AssociativeContainer<C>)
            return sortedPosition(key);
        else
            return static_cast<int>(key);
    }

    // 8.3: merge the new keys into the sorted row list and announce one
    // insertion per run of final positions, ascending.
    void onEntriesAdded(const QList<Key> & added)
    {
        if constexpr (AssociativeContainer<C>) {
            std::vector<Key> fresh(added.begin(), added.end());
            std::stable_sort(fresh.begin(), fresh.end(), Less{});
            std::vector<Key> merged;
            std::vector<int> positions;
            merged.reserve(m_keys.size() + fresh.size());
            std::size_t i = 0;
            std::size_t j = 0;
            while (i < m_keys.size() || j < fresh.size()) {
                const bool takeOld = j >= fresh.size() || (i < m_keys.size() && Less{}(m_keys[i], fresh[j]));
                if (takeOld) {
                    merged.push_back(m_keys[i++]);
                } else {
                    positions.push_back(static_cast<int>(merged.size()));
                    merged.push_back(fresh[j++]);
                }
            }
            for (const auto & [first, last] : obs::contiguousRuns(positions.begin(), positions.end())) {
                beginInsertRows({}, first, last);
                m_keys.insert(m_keys.begin() + first, merged.begin() + first, merged.begin() + last + 1);
                m_stale = true;
                endInsertRows();
            }
        }
    }

    // 8.3: the rows of the removed keys, ascending, split into runs, announced
    // from the bottom run up.
    void onEntriesRemoved(const QList<Key> & removed)
    {
        if constexpr (AssociativeContainer<C>) {
            std::vector<int> rows;
            rows.reserve(static_cast<std::size_t>(removed.size()));
            for (const Key & k : removed) {
                const int row = rowOf(k);
                if (row >= 0)
                    rows.push_back(row);
            }
            std::sort(rows.begin(), rows.end());
            const auto runs = obs::contiguousRuns(rows.begin(), rows.end());
            for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
                beginRemoveRows({}, run->first, run->second);
                m_keys.erase(m_keys.begin() + run->first, m_keys.begin() + run->second + 1);
                m_stale = true;
                endRemoveRows();
            }
        }
    }

    Container<C> & m_source;
    int m_count = 0;                 // sequential source: the rows are the indices
    int m_pending = 0;               // the row between an "about" and its twin
    std::vector<Key> m_keys;         // associative source: the sorted row keys
    mutable QHash<Key, int> m_rowOf; // lazy key -> row map
    mutable bool m_stale = true;
};

}  // namespace obs
