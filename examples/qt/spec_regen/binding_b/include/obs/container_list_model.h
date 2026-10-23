// obs::ContainerListModel<C, Less>: a QAbstractListModel wired to an obs::Container (SPEC 8.1 to 8.3).
// It adds no signals, so it has no RQT_OBJECT and, under binding B as under A, no moc input.
#pragma once

#include "container.h"
#include "traits.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

namespace obs {

template <class C, class Less = std::less<KeyOf<C>>>
    requires SupportedContainer<C>
class ContainerListModel : public QAbstractListModel {
public:
    using Key = KeyOf<C>;
    using Value = ValueOf<C>;

    explicit ContainerListModel(Container<C> & source, QObject * parent = nullptr) : QAbstractListModel(parent), source_(&source)
    {
        static_assert(SequentialContainer<C> || HashableKey<Key>, "the rows of an associative source are found through a key to row map, so the key must be hashable");
        rebuild();

        using Source = Container<C>;
        QObject::connect(&source, &Source::entryAboutToBeAdded, this, [this](const Key & key) {
            const int row = insertionRow(key);
            beginInsertRows({}, row, row);
        });
        QObject::connect(&source, &Source::entryAdded, this, [this]([[maybe_unused]] const Key & key) {
            if constexpr (SequentialContainer<C>) {
                ++count_;
            } else {
                rows_.insert(rows_.begin() + insertionRow(key), key);
                mapStale_ = true;
            }
            endInsertRows();
        });
        QObject::connect(&source, &Source::entryAboutToBeRemoved, this, [this](const Key & key) {
            const int row = rowOf(key);
            beginRemoveRows({}, row, row);
        });
        QObject::connect(&source, &Source::entryRemoved, this, [this]([[maybe_unused]] const Key & key) {
            if constexpr (SequentialContainer<C>) {
                --count_;
            } else {
                rows_.erase(rows_.begin() + rowOf(key));
                mapStale_ = true;
            }
            endRemoveRows();
        });
        // the roles go THROUGH unchanged: an empty list stays empty
        QObject::connect(&source, &Source::entryDataChanged, this, [this](const Key & key, const QList<int> & roles) {
            const QModelIndex at = index(rowOf(key), 0);
            dataChanged(at, at, roles);
        });
        QObject::connect(&source, &Source::aboutToBeCleared, this, [this] { beginResetModel(); });
        QObject::connect(&source, &Source::cleared, this, [this] {
            clearRows();
            endResetModel();
        });
        QObject::connect(&source, &Source::aboutToBeReset, this, [this] { beginResetModel(); });
        QObject::connect(&source, &Source::resetCompleted, this, [this] {
            rebuild();
            endResetModel();
        });

        if constexpr (AssociativeContainer<C>) {
            QObject::connect(&source, &Source::entriesAdded, this, [this](const QList<Key> & keys) { insertBatch(keys); });
            // A removal is announced at the "about" notification, while the container still holds the entries, so a view
            // can read the rows it is losing (SPEC 8.3 as its author meant it; a hand change, see the README).
            QObject::connect(&source, &Source::entriesAboutToBeRemoved, this, [this](const QList<Key> & keys) { removeBatch(keys); });
        }
    }

    // the model's OWN count: it changes only between a begin* and its end*, while the container's size leads or lags
    int rowCount(const QModelIndex & parent = {}) const override
    {
        if (parent.isValid())
            return 0;
        if constexpr (SequentialContainer<C>)
            return count_;
        else
            return static_cast<int>(rows_.size());
    }

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override
    {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};
        const Key key = keyAt(index.row());
        const Value * value = source_->find(key);
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
            return rows_[static_cast<std::size_t>(row)];
    }

    int rowOf(const Key & key) const
    {
        if constexpr (SequentialContainer<C>) {
            return key >= 0 && key < count_ ? static_cast<int>(key) : -1;
        } else {
            if (mapStale_) {
                rowMap_.clear();
                for (std::size_t i = 0; i < rows_.size(); ++i)
                    rowMap_.insert(rows_[i], static_cast<int>(i));
                mapStale_ = false;
            }
            return rowMap_.value(key, -1);
        }
    }

protected:
    virtual QVariant valueData(const Key & key, const Value & value, int role) const = 0;

private:
    // the row a key has (sequential) or will have (associative: the first row whose key is not less than it)
    int insertionRow(const Key & key) const
    {
        if constexpr (SequentialContainer<C>)
            return static_cast<int>(key);
        else
            return static_cast<int>(std::lower_bound(rows_.begin(), rows_.end(), key, less_) - rows_.begin());
    }

    void clearRows()
    {
        count_ = 0;
        rows_.clear();
        mapStale_ = true;
    }

    void rebuild()
    {
        clearRows();
        if constexpr (SequentialContainer<C>) {
            count_ = static_cast<int>(source_->size());
        } else {
            detail::forEachEntry(source_->data(), [this](const Key & key, const Value &) { rows_.push_back(key); });
            std::sort(rows_.begin(), rows_.end(), less_);
        }
    }

    // One insert cycle per run of FINAL positions, ascending: each position is already right when it is used.
    void insertBatch(const QList<Key> & keys)
    {
        std::vector<Key> added(keys.begin(), keys.end());
        std::sort(added.begin(), added.end(), less_);
        std::vector<Key> merged;
        merged.reserve(rows_.size() + added.size());
        std::vector<int> positions;
        auto oldIt = rows_.begin();
        auto newIt = added.begin();
        while (oldIt != rows_.end() || newIt != added.end()) {
            const bool takeNew = oldIt == rows_.end() || (newIt != added.end() && less_(*newIt, *oldIt));
            if (takeNew) {
                positions.push_back(static_cast<int>(merged.size()));
                merged.push_back(*newIt++);
            } else {
                merged.push_back(*oldIt++);
            }
        }
        for (const auto & [first, last] : contiguousRuns(positions.begin(), positions.end())) {
            beginInsertRows({}, first, last);
            rows_.insert(rows_.begin() + first, merged.begin() + first, merged.begin() + last + 1);
            mapStale_ = true;
            endInsertRows();
        }
    }

    // One remove cycle per run of current rows, DESCENDING: the rows of the runs still to do do not move. It runs at the
    // "about" notification, so the container still holds every entry of every run.
    void removeBatch(const QList<Key> & keys)
    {
        std::vector<int> positions;
        for (const Key & key : keys)
            if (const int row = rowOf(key); row >= 0)
                positions.push_back(row);
        std::sort(positions.begin(), positions.end());
        const auto runs = contiguousRuns(positions.begin(), positions.end());
        for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
            beginRemoveRows({}, run->first, run->second);
            rows_.erase(rows_.begin() + run->first, rows_.begin() + run->second + 1);
            mapStale_ = true;
            endRemoveRows();
        }
    }

    Container<C> * source_;
    [[no_unique_address]] Less less_{};
    int count_ = 0;                  // sequential source only
    std::vector<Key> rows_;          // associative source only: the keys, sorted by Less
    mutable QHash<Key, int> rowMap_; // the lazy key to row map, rebuilt on the first lookup after a change
    mutable bool mapStale_ = true;
};

}  // namespace obs
