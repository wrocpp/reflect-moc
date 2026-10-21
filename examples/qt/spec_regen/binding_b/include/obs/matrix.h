// obs::Matrix<RowKey, ColKey, Value>: a dense matrix labelled by keys (SPEC 9), binding B: the class template
// declares its own typed signals through reflect-moc, no moc input.
#pragma once

#include "traits.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace obs {

template <class RowKey, class ColKey, class Value>
    requires HashableKey<RowKey> && HashableKey<ColKey> && std::equality_comparable<Value>
class Matrix : public QObject {
    RQT_OBJECT

public:
    using RowKeyType = RowKey;
    using ColKeyType = ColKey;
    using ValueType = Value;

    explicit Matrix(Value uninitialized, QObject * parent = nullptr) : QObject(parent), sentinel_(std::move(uninitialized)) {}

signals:
    rqt::signal<void(const RowKey &, qsizetype)> rowAboutToBeAdded;
    rqt::signal<void(const RowKey &, qsizetype)> rowAdded;
    rqt::signal<void(const ColKey &, qsizetype)> columnAboutToBeAdded;
    rqt::signal<void(const ColKey &, qsizetype)> columnAdded;
    rqt::signal<void(qsizetype, qsizetype, const QList<RowKey> &)> rowsAboutToBeAdded;
    rqt::signal<void(qsizetype, qsizetype, const QList<RowKey> &)> rowsAdded;
    rqt::signal<void(qsizetype, qsizetype, const QList<ColKey> &)> columnsAboutToBeAdded;
    rqt::signal<void(qsizetype, qsizetype, const QList<ColKey> &)> columnsAdded;
    rqt::signal<void(const RowKey &, qsizetype)> rowAboutToBeRemoved;
    rqt::signal<void(const RowKey &, qsizetype)> rowRemoved;
    rqt::signal<void(const ColKey &, qsizetype)> columnAboutToBeRemoved;
    rqt::signal<void(const ColKey &, qsizetype)> columnRemoved;
    rqt::signal<void(qsizetype, qsizetype, const QList<RowKey> &)> rowsAboutToBeRemoved;
    rqt::signal<void(qsizetype, qsizetype, const QList<RowKey> &)> rowsRemoved;
    rqt::signal<void(qsizetype, qsizetype, const QList<ColKey> &)> columnsAboutToBeRemoved;
    rqt::signal<void(qsizetype, qsizetype, const QList<ColKey> &)> columnsRemoved;
    rqt::signal<void(const RowKey &, const RowKey &, qsizetype)> rowAboutToBeRenamed;
    rqt::signal<void(const RowKey &, const RowKey &, qsizetype)> rowRenamed;
    rqt::signal<void(const ColKey &, const ColKey &, qsizetype)> columnAboutToBeRenamed;
    rqt::signal<void(const ColKey &, const ColKey &, qsizetype)> columnRenamed;
    rqt::signal<void(const RowKey &, const ColKey &, qsizetype, qsizetype)> cellAboutToChange;
    rqt::signal<void(const RowKey &, const ColKey &, qsizetype, qsizetype)> cellChanged;
    rqt::signal<void(qsizetype, qsizetype, qsizetype, qsizetype)> cellsAboutToChange;
    rqt::signal<void(qsizetype, qsizetype, qsizetype, qsizetype)> cellsChanged;
    rqt::signal<void()> aboutToBeCleared;
    rqt::signal<void()> cleared;

public:
    qsizetype rowCount() const { return rows_.size(); }
    qsizetype columnCount() const { return columns_.size(); }
    const QList<RowKey> & rowKeys() const { return rows_; }
    const QList<ColKey> & columnKeys() const { return columns_; }

    qsizetype rowIndexOf(const RowKey & key) const
    {
        if (rowsStale_) {
            rowIndex_.clear();
            for (qsizetype i = 0; i < rows_.size(); ++i)
                rowIndex_.insert(rows_[i], i);
            rowsStale_ = false;
        }
        return rowIndex_.value(key, -1);
    }

    qsizetype columnIndexOf(const ColKey & key) const
    {
        if (columnsStale_) {
            columnIndex_.clear();
            for (qsizetype j = 0; j < columns_.size(); ++j)
                columnIndex_.insert(columns_[j], j);
            columnsStale_ = false;
        }
        return columnIndex_.value(key, -1);
    }

    // "absent" is a value: NaN never equals itself, so a NaN sentinel is tested with isnan
    bool isUninitialized(const Value & v) const
    {
        if constexpr (std::is_floating_point_v<Value>) {
            if (std::isnan(sentinel_))
                return std::isnan(v);
        }
        return v == sentinel_;
    }

    // total: an absent row or column yields the sentinel, and reading never creates structure
    Value value(const RowKey & row, const ColKey & column) const
    {
        const qsizetype i = rowIndexOf(row);
        const qsizetype j = columnIndexOf(column);
        return i < 0 || j < 0 ? sentinel_ : cells_[cellOffset(i, j)];
    }

    Value valueAt(qsizetype row, qsizetype column) const
    {
        Q_ASSERT(row >= 0 && row < rowCount() && column >= 0 && column < columnCount());
        return cells_[cellOffset(row, column)];
    }

    bool addRow(const RowKey & key)
    {
        if (rowIndexOf(key) >= 0)
            return false;
        const qsizetype index = rowCount();
        rowAboutToBeAdded(key, index);
        rows_.append(key);
        appendSentinels(columnCount());
        rowsStale_ = true;
        rowAdded(key, index);
        return true;
    }

    bool addColumn(const ColKey & key)
    {
        if (columnIndexOf(key) >= 0)
            return false;
        const qsizetype index = columnCount();
        columnAboutToBeAdded(key, index);
        columns_.append(key);
        insertColumnSentinels(index, 1);
        columnsStale_ = true;
        columnAdded(key, index);
        return true;
    }

    qsizetype addRows(const QList<RowKey> & keys)
    {
        const QList<RowKey> fresh = freshKeys(keys, [this](const RowKey & k) { return rowIndexOf(k) >= 0; });
        if (fresh.isEmpty())
            return 0;
        const qsizetype first = rowCount();
        const qsizetype last = first + fresh.size() - 1;
        rowsAboutToBeAdded(first, last, fresh);
        rows_.append(fresh);
        appendSentinels(columnCount() * fresh.size());
        rowsStale_ = true;
        rowsAdded(first, last, fresh);
        return fresh.size();
    }

    qsizetype addColumns(const QList<ColKey> & keys)
    {
        const QList<ColKey> fresh = freshKeys(keys, [this](const ColKey & k) { return columnIndexOf(k) >= 0; });
        if (fresh.isEmpty())
            return 0;
        const qsizetype first = columnCount();
        const qsizetype last = first + fresh.size() - 1;
        columnsAboutToBeAdded(first, last, fresh);
        columns_.append(fresh);
        insertColumnSentinels(first, fresh.size());
        columnsStale_ = true;
        columnsAdded(first, last, fresh);
        return fresh.size();
    }

    bool removeRow(const RowKey & key)
    {
        const qsizetype index = rowIndexOf(key);
        if (index < 0)
            return false;
        rowAboutToBeRemoved(key, index);
        eraseRows(index, index);
        rowRemoved(key, index);
        return true;
    }

    bool removeColumn(const ColKey & key)
    {
        const qsizetype index = columnIndexOf(key);
        if (index < 0)
            return false;
        columnAboutToBeRemoved(key, index);
        eraseColumns(index, index);
        columnRemoved(key, index);
        return true;
    }

    // one pair per contiguous run, from the LAST run to the first, so the runs still to do keep their indices
    qsizetype removeRows(const QList<RowKey> & keys)
    {
        const std::vector<qsizetype> indices = sortedIndices(keys, [this](const RowKey & k) { return rowIndexOf(k); });
        const auto runs = contiguousRuns(indices.begin(), indices.end());
        for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
            const qsizetype first = run->first;
            const qsizetype last = run->second;
            const QList<RowKey> keysOfRun = rows_.mid(first, last - first + 1);
            rowsAboutToBeRemoved(first, last, keysOfRun);
            eraseRows(first, last);
            rowsRemoved(first, last, keysOfRun);
        }
        return static_cast<qsizetype>(indices.size());
    }

    qsizetype removeColumns(const QList<ColKey> & keys)
    {
        const std::vector<qsizetype> indices = sortedIndices(keys, [this](const ColKey & k) { return columnIndexOf(k); });
        const auto runs = contiguousRuns(indices.begin(), indices.end());
        for (auto run = runs.rbegin(); run != runs.rend(); ++run) {
            const qsizetype first = run->first;
            const qsizetype last = run->second;
            const QList<ColKey> keysOfRun = columns_.mid(first, last - first + 1);
            columnsAboutToBeRemoved(first, last, keysOfRun);
            eraseColumns(first, last);
            columnsRemoved(first, last, keysOfRun);
        }
        return static_cast<qsizetype>(indices.size());
    }

    // never a move, a remove plus add, or a merge: the row keeps its index and its cells
    bool renameRow(const RowKey & from, const RowKey & to)
    {
        const qsizetype index = rowIndexOf(from);
        if (index < 0)
            return false;
        if (from == to)
            return true;
        if (rowIndexOf(to) >= 0)
            return false;
        const RowKey oldKey = rows_[index];  // a copy: `from` may refer to the very element we overwrite
        rowAboutToBeRenamed(oldKey, to, index);
        rows_[index] = to;
        rowsStale_ = true;  // a rename changes keys without changing counts: the lookup map is stale all the same
        rowRenamed(oldKey, to, index);
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
        const ColKey oldKey = columns_[index];
        columnAboutToBeRenamed(oldKey, to, index);
        columns_[index] = to;
        columnsStale_ = true;
        columnRenamed(oldKey, to, index);
        return true;
    }

    bool setValue(const RowKey & row, const ColKey & column, const Value & v)
    {
        const qsizetype i = rowIndexOf(row);
        const qsizetype j = columnIndexOf(column);
        if (i < 0 || j < 0)
            return false;
        if (sameValue(cells_[cellOffset(i, j)], v))
            return true;
        cellAboutToChange(row, column, i, j);
        cells_[cellOffset(i, j)] = v;
        cellChanged(row, column, i, j);
        return true;
    }

    // changed cells become FULL rectangles, never the bounding box; all "about" first, then the writes, then the twins
    qsizetype setValues(const QList<std::tuple<RowKey, ColKey, Value>> & cells)
    {
        // 1. filter: absent keys skipped, the LAST occurrence of a cell wins, unchanged cells dropped.
        //    The map is ordered by (row, column), which is the order step 2 walks in.
        std::map<std::pair<qsizetype, qsizetype>, Value> changed;
        for (const auto & [row, column, v] : cells) {
            const qsizetype i = rowIndexOf(row);
            const qsizetype j = columnIndexOf(column);
            if (i >= 0 && j >= 0)
                changed.insert_or_assign(std::pair<qsizetype, qsizetype>{i, j}, v);
        }
        std::erase_if(changed, [this](const auto & entry) {
            return sameValue(cells_[cellOffset(entry.first.first, entry.first.second)], entry.second);
        });
        if (changed.empty())
            return 0;

        // 2. per row the changed columns form runs; a run joins the open rectangle of the previous row with the same extent
        struct Rect {
            qsizetype top, left, bottom, right;
        };
        std::vector<Rect> rects;
        std::map<std::pair<qsizetype, qsizetype>, std::size_t> open;  // (left, right) -> rect whose bottom is the previous row
        auto it = changed.begin();
        while (it != changed.end()) {
            const qsizetype row = it->first.first;
            std::map<std::pair<qsizetype, qsizetype>, std::size_t> next;
            while (it != changed.end() && it->first.first == row) {
                const qsizetype left = it->first.second;
                qsizetype right = left;
                for (++it; it != changed.end() && it->first.first == row && it->first.second == right + 1; ++it)
                    ++right;
                const auto joined = open.find(std::pair<qsizetype, qsizetype>{left, right});
                std::size_t index = 0;
                if (joined != open.end()) {
                    index = joined->second;
                    rects[index].bottom = row;
                } else {
                    index = rects.size();
                    rects.push_back({row, left, row, right});
                }
                next.insert_or_assign(std::pair<qsizetype, qsizetype>{left, right}, index);
            }
            open = std::move(next);
            // a rectangle is open only for the very next row: after a gap nothing may join it
            if (it != changed.end() && it->first.first != row + 1)
                open.clear();
        }

        // 3. order by (top, left); about for each, write, twin for each
        std::ranges::sort(rects, [](const Rect & a, const Rect & b) { return std::tie(a.top, a.left) < std::tie(b.top, b.left); });
        for (const Rect & r : rects)
            cellsAboutToChange(r.top, r.left, r.bottom, r.right);
        for (const auto & [position, v] : changed)
            cells_[cellOffset(position.first, position.second)] = v;
        for (const Rect & r : rects)
            cellsChanged(r.top, r.left, r.bottom, r.right);
        return static_cast<qsizetype>(changed.size());
    }

    void clear()
    {
        if (rows_.isEmpty() && columns_.isEmpty())
            return;
        aboutToBeCleared();
        rows_.clear();
        columns_.clear();
        cells_.clear();
        rowsStale_ = true;
        columnsStale_ = true;
        cleared();
    }

private:
    qsizetype cellOffset(qsizetype row, qsizetype column) const { return row * columnCount() + column; }

    // equal for the purpose of "nothing changed": a NaN over a NaN cell is equal when the sentinel is NaN
    bool sameValue(const Value & a, const Value & b) const
    {
        if constexpr (std::is_floating_point_v<Value>) {
            if (std::isnan(sentinel_) && std::isnan(a) && std::isnan(b))
                return true;
        }
        return a == b;
    }

    template <class Key, class Present>
    static QList<Key> freshKeys(const QList<Key> & keys, Present present)
    {
        QList<Key> fresh;
        QHash<Key, char> seen;
        for (const Key & k : keys)
            if (!present(k) && !seen.contains(k)) {
                seen.insert(k, char{1});
                fresh.append(k);
            }
        return fresh;
    }

    // the sorted, duplicate-free indices of the keys that are present
    template <class Key, class IndexOf>
    static std::vector<qsizetype> sortedIndices(const QList<Key> & keys, IndexOf indexOf)
    {
        std::vector<qsizetype> indices;
        for (const Key & k : keys)
            if (const qsizetype i = indexOf(k); i >= 0)
                indices.push_back(i);
        std::ranges::sort(indices);
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        return indices;
    }

    void appendSentinels(qsizetype n)
    {
        for (qsizetype k = 0; k < n; ++k)
            cells_.append(sentinel_);
    }

    // `count` new columns at `at` (the old width): one sentinel run at the end of every row, LAST row first,
    // so the offsets still to use do not move
    void insertColumnSentinels(qsizetype at, qsizetype count)
    {
        for (qsizetype r = rowCount() - 1; r >= 0; --r)
            cells_.insert(r * at + at, count, sentinel_);
    }

    void eraseRows(qsizetype first, qsizetype last)
    {
        const qsizetype width = columnCount();
        cells_.remove(first * width, (last - first + 1) * width);
        rows_.remove(first, last - first + 1);
        rowsStale_ = true;
    }

    void eraseColumns(qsizetype first, qsizetype last)
    {
        const qsizetype oldWidth = columnCount();
        for (qsizetype r = rowCount() - 1; r >= 0; --r)
            cells_.remove(r * oldWidth + first, last - first + 1);
        columns_.remove(first, last - first + 1);
        columnsStale_ = true;
    }

    Value sentinel_;
    QList<RowKey> rows_;
    QList<ColKey> columns_;
    QList<Value> cells_;  // row-major
    mutable QHash<RowKey, qsizetype> rowIndex_;
    mutable QHash<ColKey, qsizetype> columnIndex_;
    mutable bool rowsStale_ = true;
    mutable bool columnsStale_ = true;
};

}  // namespace obs
