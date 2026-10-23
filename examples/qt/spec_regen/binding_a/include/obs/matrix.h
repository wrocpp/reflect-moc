#pragma once

#include "obs/matrix_signals.h"
#include "obs/traits.h"

#include <QHash>
#include <QList>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace obs {

template <class RowKey, class ColKey, class Value>
    requires HashableKey<RowKey> && HashableKey<ColKey>
          && VariantKey<RowKey> && VariantKey<ColKey>
          && std::equality_comparable<Value>
class Matrix : public MatrixSignals {
public:
    using RowKeyType = RowKey;
    using ColKeyType = ColKey;
    using ValueType = Value;

    explicit Matrix(Value uninitialized, QObject * parent = nullptr)
        : MatrixSignals(parent), m_sentinel(std::move(uninitialized))
    {}

    qsizetype rowCount() const { return m_rowKeys.size(); }
    qsizetype columnCount() const { return m_colKeys.size(); }
    const QList<RowKey> & rowKeys() const { return m_rowKeys; }
    const QList<ColKey> & columnKeys() const { return m_colKeys; }

    qsizetype rowIndexOf(const RowKey & key) const
    {
        return m_rowIndex.value(key, -1);
    }

    qsizetype columnIndexOf(const ColKey & key) const
    {
        return m_colIndex.value(key, -1);
    }

    // NaN never compares equal to itself, so a NaN sentinel is tested with isnan.
    bool isUninitialized(const Value & v) const
    {
        if constexpr (std::is_floating_point_v<Value>) {
            if (std::isnan(m_sentinel))
                return std::isnan(v);
        }
        return v == m_sentinel;
    }

    Value value(const RowKey & row, const ColKey & column) const
    {
        const qsizetype r = rowIndexOf(row);
        const qsizetype c = columnIndexOf(column);
        return r < 0 || c < 0 ? m_sentinel : cell(r, c);
    }

    Value valueAt(qsizetype row, qsizetype column) const
    {
        Q_ASSERT(row >= 0 && row < rowCount() && column >= 0 && column < columnCount());
        return cell(row, column);
    }

    bool addRow(const RowKey & key)
    {
        if (rowIndexOf(key) >= 0)
            return false;
        const qsizetype index = rowCount();
        emit rowAboutToBeAdded(detail::box(key), index);
        appendRows(QList<RowKey>{key});
        emit rowAdded(detail::box(key), index);
        return true;
    }

    bool addColumn(const ColKey & key)
    {
        if (columnIndexOf(key) >= 0)
            return false;
        const qsizetype index = columnCount();
        emit columnAboutToBeAdded(detail::box(key), index);
        appendColumns(QList<ColKey>{key});
        emit columnAdded(detail::box(key), index);
        return true;
    }

    qsizetype addRows(const QList<RowKey> & keys)
    {
        const QList<RowKey> fresh = detail::uniqueKeys(keys, [this](const RowKey & k) { return rowIndexOf(k) < 0; });
        if (fresh.isEmpty())
            return 0;
        const qsizetype first = rowCount();
        const qsizetype last = first + fresh.size() - 1;
        emit rowsAboutToBeAdded(first, last, detail::boxAll(fresh));
        appendRows(fresh);
        emit rowsAdded(first, last, detail::boxAll(fresh));
        return fresh.size();
    }

    qsizetype addColumns(const QList<ColKey> & keys)
    {
        const QList<ColKey> fresh = detail::uniqueKeys(keys, [this](const ColKey & k) { return columnIndexOf(k) < 0; });
        if (fresh.isEmpty())
            return 0;
        const qsizetype first = columnCount();
        const qsizetype last = first + fresh.size() - 1;
        emit columnsAboutToBeAdded(first, last, detail::boxAll(fresh));
        appendColumns(fresh);
        emit columnsAdded(first, last, detail::boxAll(fresh));
        return fresh.size();
    }

    bool removeRow(const RowKey & key)
    {
        const qsizetype index = rowIndexOf(key);
        if (index < 0)
            return false;
        emit rowAboutToBeRemoved(detail::box(key), index);
        eraseRows(index, index);
        emit rowRemoved(detail::box(key), index);
        return true;
    }

    bool removeColumn(const ColKey & key)
    {
        const qsizetype index = columnIndexOf(key);
        if (index < 0)
            return false;
        emit columnAboutToBeRemoved(detail::box(key), index);
        eraseColumns(index, index);
        emit columnRemoved(detail::box(key), index);
        return true;
    }

    // One pair per run of indices, from the last run to the first, so the
    // indices of the runs still to process stay valid.
    qsizetype removeRows(const QList<RowKey> & keys)
    {
        const std::vector<int> indices = sortedIndices(keys, [this](const RowKey & k) { return rowIndexOf(k); });
        const auto runs = runsOf(indices);
        for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
            const QList<RowKey> keysOfRun = m_rowKeys.mid(run->first, run->second - run->first + 1);
            emit rowsAboutToBeRemoved(run->first, run->second, detail::boxAll(keysOfRun));
            eraseRows(run->first, run->second);
            emit rowsRemoved(run->first, run->second, detail::boxAll(keysOfRun));
        }
        return static_cast<qsizetype>(indices.size());
    }

    qsizetype removeColumns(const QList<ColKey> & keys)
    {
        const std::vector<int> indices = sortedIndices(keys, [this](const ColKey & k) { return columnIndexOf(k); });
        const auto runs = runsOf(indices);
        for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
            const QList<ColKey> keysOfRun = m_colKeys.mid(run->first, run->second - run->first + 1);
            emit columnsAboutToBeRemoved(run->first, run->second, detail::boxAll(keysOfRun));
            eraseColumns(run->first, run->second);
            emit columnsRemoved(run->first, run->second, detail::boxAll(keysOfRun));
        }
        return static_cast<qsizetype>(indices.size());
    }

    // A rename keeps the index and the cells: it is never a remove plus add.
    bool renameRow(const RowKey & from, const RowKey & to)
    {
        const qsizetype index = rowIndexOf(from);
        if (index < 0)
            return false;
        if (from == to)
            return true;
        if (rowIndexOf(to) >= 0)
            return false;
        emit rowAboutToBeRenamed(detail::box(from), detail::box(to), index);
        m_rowKeys[index] = to;
        m_rowIndex.remove(from);
        m_rowIndex.insert(to, index);
        emit rowRenamed(detail::box(from), detail::box(to), index);
        return true;
    }

    bool renameColumn(const ColKey & from, const ColKey & to)
    {
        const qsizetype index = columnIndexOf(from);
        if (index < 0)
            return false;
        if (from == to)
            return true;
        if (columnIndexOf(to) >= 0)
            return false;
        emit columnAboutToBeRenamed(detail::box(from), detail::box(to), index);
        m_colKeys[index] = to;
        m_colIndex.remove(from);
        m_colIndex.insert(to, index);
        emit columnRenamed(detail::box(from), detail::box(to), index);
        return true;
    }

    bool setValue(const RowKey & row, const ColKey & column, const Value & v)
    {
        const qsizetype r = rowIndexOf(row);
        const qsizetype c = columnIndexOf(column);
        if (r < 0 || c < 0)
            return false;
        if (sameValue(cell(r, c), v))
            return true;
        emit cellAboutToChange(detail::box(row), detail::box(column), r, c);
        cell(r, c) = v;
        emit cellChanged(detail::box(row), detail::box(column), r, c);
        return true;
    }

    qsizetype setValues(const QList<std::tuple<RowKey, ColKey, Value>> & cells)
    {
        // 1. last occurrence wins, then drop the cells that would not change
        std::map<std::pair<qsizetype, qsizetype>, Value> wanted;
        for (const auto & [row, column, v] : cells) {
            const qsizetype r = rowIndexOf(row);
            const qsizetype c = columnIndexOf(column);
            if (r >= 0 && c >= 0)
                wanted.insert_or_assign(std::pair<qsizetype, qsizetype>{r, c}, v);
        }
        std::erase_if(wanted, [this](const auto & entry) {
            return sameValue(cell(entry.first.first, entry.first.second), entry.second);
        });
        if (wanted.empty())
            return 0;

        // 2. full rectangles: per row runs of columns, merged with the open
        //    rectangle of the previous row that has exactly the same run
        struct Rect {
            qsizetype top, left, bottom, right;
        };
        std::vector<Rect> rects;
        auto entry = wanted.begin();
        while (entry != wanted.end()) {
            const qsizetype row = entry->first.first;
            std::vector<int> columns;
            for (; entry != wanted.end() && entry->first.first == row; ++entry)
                columns.push_back(static_cast<int>(entry->first.second));
            for (const auto & [left, right] : runsOf(columns)) {
                const auto open = std::ranges::find_if(rects, [&](const Rect & rect) {
                    return rect.bottom == row - 1 && rect.left == left && rect.right == right;
                });
                if (open != rects.end())
                    open->bottom = row;
                else
                    rects.push_back(Rect{row, left, row, right});
            }
        }

        // 3. announce all, write all, confirm all, in (top, left) order
        std::ranges::sort(rects, [](const Rect & a, const Rect & b) {
            return a.top != b.top ? a.top < b.top : a.left < b.left;
        });
        for (const Rect & rect : rects)
            emit cellsAboutToChange(rect.top, rect.left, rect.bottom, rect.right);
        for (const auto & [position, v] : wanted)
            cell(position.first, position.second) = v;
        for (const Rect & rect : rects)
            emit cellsChanged(rect.top, rect.left, rect.bottom, rect.right);
        return static_cast<qsizetype>(wanted.size());
    }

    void clear()
    {
        if (rowCount() == 0 && columnCount() == 0)
            return;
        emit aboutToBeCleared();
        m_rowKeys.clear();
        m_colKeys.clear();
        m_values.clear();
        m_rowIndex.clear();
        m_colIndex.clear();
        emit cleared();
    }

private:
    // The key-to-position indexes are kept in step with the key lists on every change (a hand change, see the README):
    // appending adds the new keys, a rename moves one key, erasing a range drops its keys and renumbers the keys after it,
    // clear empties them.
    template <class Keys, class Index>
    static void indexAppended(Index & index, const Keys & keys, qsizetype first)
    {
        for (qsizetype i = first; i < keys.size(); ++i)
            index.insert(keys[i], i);
    }

    template <class Keys, class Index>
    static void indexDropRange(Index & index, const Keys & keys, qsizetype first, qsizetype last)
    {
        for (qsizetype i = first; i <= last; ++i)
            index.remove(keys[i]);
    }

    template <class Keys, class Index>
    static void indexRenumberFrom(Index & index, const Keys & keys, qsizetype first)
    {
        for (qsizetype i = first; i < keys.size(); ++i)
            index[keys[i]] = i;
    }

    template <class Keys, class IndexOf>
    static std::vector<int> sortedIndices(const Keys & keys, IndexOf indexOf)
    {
        std::vector<int> indices;
        for (const auto & k : keys) {
            const qsizetype i = indexOf(k);
            if (i >= 0)
                indices.push_back(static_cast<int>(i));
        }
        std::ranges::sort(indices);
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        return indices;
    }

    static std::vector<std::pair<int, int>> runsOf(const std::vector<int> & sorted)
    {
        return contiguousRuns(sorted.begin(), sorted.end());
    }

    // NaN written over a NaN cell counts as equal when the sentinel is NaN.
    bool sameValue(const Value & a, const Value & b) const
    {
        if constexpr (std::is_floating_point_v<Value>) {
            if (std::isnan(m_sentinel) && std::isnan(a) && std::isnan(b))
                return true;
        }
        return a == b;
    }

    const Value & cell(qsizetype r, qsizetype c) const { return m_values[r * columnCount() + c]; }
    Value & cell(qsizetype r, qsizetype c) { return m_values[r * columnCount() + c]; }

    void appendRows(const QList<RowKey> & keys)
    {
        const qsizetype first = m_rowKeys.size();
        m_rowKeys.append(keys);
        m_values.insert(m_values.size(), keys.size() * columnCount(), m_sentinel);
        indexAppended(m_rowIndex, m_rowKeys, first);
    }

    // One sentinel at the end of every row, last row first, so the offsets
    // still to use do not move.
    void appendColumns(const QList<ColKey> & keys)
    {
        const qsizetype columns = columnCount();
        for (qsizetype r = rowCount() - 1; r >= 0; --r)
            m_values.insert((r + 1) * columns, keys.size(), m_sentinel);
        const qsizetype firstColumn = m_colKeys.size();
        m_colKeys.append(keys);
        indexAppended(m_colIndex, m_colKeys, firstColumn);
    }

    void eraseRows(qsizetype first, qsizetype last)
    {
        m_values.remove(first * columnCount(), (last - first + 1) * columnCount());
        indexDropRange(m_rowIndex, m_rowKeys, first, last);
        m_rowKeys.remove(first, last - first + 1);
        indexRenumberFrom(m_rowIndex, m_rowKeys, first);
    }

    void eraseColumns(qsizetype first, qsizetype last)
    {
        const qsizetype columns = columnCount();
        for (qsizetype r = rowCount() - 1; r >= 0; --r)
            m_values.remove(r * columns + first, last - first + 1);
        indexDropRange(m_colIndex, m_colKeys, first, last);
        m_colKeys.remove(first, last - first + 1);
        indexRenumberFrom(m_colIndex, m_colKeys, first);
    }

    Value m_sentinel;
    QList<RowKey> m_rowKeys;
    QList<ColKey> m_colKeys;
    QList<Value> m_values;  // row-major
    QHash<RowKey, qsizetype> m_rowIndex;
    QHash<ColKey, qsizetype> m_colIndex;
};

}  // namespace obs
