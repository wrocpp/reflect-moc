#!/usr/bin/env python3
"""Mutation testing for examples/qt/observable_models, by hand-listed textual mutants.

Mull needs clang and the image has only GCC, and the changed code here is ordinary run-time code, so
this applies one small mutation at a time to a COPY of the repository, rebuilds the two observable-model
tests and runs them. A mutant is killed when a test fails, aborts or times out; it survives when both
pass; it is not viable when it does not compile. Surviving mutants are the output that matters: each is
either a missing test or an equivalent mutant, and the table says which one it was judged to be.

Run it in the image (the repository is mounted read-only; the work happens in /tmp):

  docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 python3 scripts/mutate-observable-models.py [name...]
"""
import os
import shutil
import subprocess
import sys

SRC = os.environ.get("RQT_SRC", "/src")
WORK = os.environ.get("RQT_MUTATE_WORK", "/tmp/rqt-mutate")
TARGETS = ["capability_observable_models_stress", "capability_observable_models_flaws_detected",
           "capability_observable_container_announcements", "capability_row_index",
           "capability_observable_ranges_edits", "capability_observable_multi", "capability_observable_batches",
           "capability_edge_data_nan_keys", "capability_edge_data_key_order", "capability_edge_data_size_limit",
           "capability_observable_matrix_signals", "capability_observable_matrix_model", "capability_observable_matrix_random",
           "capability_observable_selection_tracker", "capability_observable_selection_models", "capability_observable_selection_random",
           "capability_edge_matrix_contract", "capability_edge_matrix_reentrancy_release", "capability_edge_matrix_lifetime"]
TEST_TIMEOUT_SECONDS = 180
DIR = "examples/qt/observable_models/"
CONTAINER, MODEL, TRAITS = DIR + "observable_container.hpp", DIR + "container_model.hpp", DIR + "container_traits.hpp"
INDEX = DIR + "row_index.hpp"
MATRIX, MATRIX_MODEL, SELECTION = DIR + "observable_matrix.hpp", DIR + "matrix_table_model.hpp", DIR + "selection_tracker.hpp"

# (name, file, old text, new text, judged equivalent?)
MUTANTS = [
    ("container: announce an insert after it", CONTAINER,
     "emit entryAboutToBeAdded(key);\n    Traits::insert(m_items, key, std::move(value));",
     "Traits::insert(m_items, key, std::move(value));\n    emit entryAboutToBeAdded(key);", False),
    ("container: never confirm an insert", CONTAINER, "    emit entryAdded(key);\n", "", False),
    ("container: remove without checking the key", CONTAINER,
     "if (refusedAsNested() || !Traits::contains(m_items, key)) return false;\n    const Announcing announcing(*this);\n    emit entryAboutToBeRemoved(key);",
     "if (refusedAsNested()) return false;\n    const Announcing announcing(*this);\n    emit entryAboutToBeRemoved(key);", False),
    ("container: never confirm a remove", CONTAINER, "    emit entryRemoved(key);\n", "", False),
    ("container: never confirm a replace", CONTAINER, "      emit entryChanged(key, {});\n", "", False),
    ("container: never announce a replace", CONTAINER, "      emit entryAboutToBeChanged(key, {});\n", "", False),
    ("container: move without checking the request", CONTAINER,
     "      if (refusedAsNested() || !Traits::can_move(m_items, from, to)) return false;\n", "      if (refusedAsNested()) return false;\n", False),
    ("container: never confirm a move", CONTAINER, "      emit entryMoved(from, to);\n", "", False),
    ("container: never announce a move", CONTAINER, "      emit entryAboutToBeMoved(from, to);\n", "", False),
    ("container: clear an empty container", CONTAINER,
     "    if (refusedAsNested() || Traits::size(m_items) == 0) return false;\n", "    if (refusedAsNested()) return false;\n", False),
    ("container: never confirm a clear", CONTAINER, "    emit cleared();\n", "", False),
    ("container: never announce a clear", CONTAINER, "    emit aboutToBeCleared();\n", "", False),
    ("traits: a list refuses an insert at its end", TRAITS,
     "static bool can_insert(const QList<V>& c, Key k) { return k >= 0 && k <= c.size(); }",
     "static bool can_insert(const QList<V>& c, Key k) { return k >= 0 && k < c.size(); }", False),
    ("traits: a list accepts the index one past its end", TRAITS,
     "static bool contains(const QList<V>& c, Key k) { return k >= 0 && k < c.size(); }",
     "static bool contains(const QList<V>& c, Key k) { return k >= 0 && k <= c.size(); }", False),
    ("traits: a list move to itself is allowed", TRAITS,
     "return contains(c, from) && contains(c, to) && from != to; }", "return contains(c, from) && contains(c, to); }", False),
    ("traits: a list cannot be moved", TRAITS,
     "  static constexpr bool movable = true;", "  static constexpr bool movable = false;", False),
    ("index: a sorted index inserts after an equal key", INDEX,
     "return std::lower_bound(m_keys.begin(), m_keys.end(), key, std::less<Key>{}) - m_keys.begin(); }",
     "return std::upper_bound(m_keys.begin(), m_keys.end(), key) - m_keys.begin(); }", True),
    ("index: a sorted index erases the row before", INDEX,
     "void erase(const Key&, qsizetype row) { m_keys.erase(m_keys.begin() + row); }",
     "void erase(const Key&, qsizetype row) { m_keys.erase(m_keys.begin() + (row ? row - 1 : 0)); }", False),
    ("index: a sorted index inserts one row late", INDEX,
     "m_keys.insert(m_keys.begin() + row, key); }", "m_keys.insert(m_keys.begin() + row + (row < static_cast<qsizetype>(m_keys.size()) ? 1 : 0), key); }", False),
    ("index: a row is one too high", INDEX,
     "qsizetype row_of(qsizetype slot) const { return prefix(slot + 1) - 1; }",
     "qsizetype row_of(qsizetype slot) const { return prefix(slot + 1); }", False),
    ("index: selection of the k-th live slot stops early", INDEX,
     "m_tree[static_cast<std::size_t>(next)] < k) {", "m_tree[static_cast<std::size_t>(next)] <= k) {", False),
    ("index: an erase leaves the slot counted", INDEX, "    add(slot + 1, -1);\n", "", False),
    ("index: an erase keeps the key", INDEX, "m_slots.kill(m_slot_of.take(key));", "m_slots.kill(m_slot_of.value(key));", True),  # a stale entry is overwritten by the next insert of that key and never read: costs memory only
    ("index: a new slot is counted twice", INDEX,
     "m_tree[static_cast<std::size_t>(position)] = 1 + prefix(position - 1) - prefix(position - lowbit(position));",
     "m_tree[static_cast<std::size_t>(position)] = 2 + prefix(position - 1) - prefix(position - lowbit(position));", False),
    ("index: dead slots are never compacted", INDEX,
     "bool needs_compaction() const { return count() > kCompactFloor && m_live * 2 < count(); }",
     "bool needs_compaction() const { return false; }", True),  # only memory grows: every answer is the same
    ("index: compaction keeps the dead slots", INDEX,
     "if (m_slots.alive(slot)) live.append(m_keys[static_cast<std::size_t>(slot)]);",
     "live.append(m_keys[static_cast<std::size_t>(slot)]);", False),
    ("index: a hash row is announced before the end", INDEX,
     "qsizetype rowToInsert(const Key&) const { return m_slots.live(); }",
     "qsizetype rowToInsert(const Key&) const { return 0; }", False),
    ("traits: a hash accepts a key it already holds", TRAITS,
     "static bool can_insert(const QHash<K, V>& c, const K& k) { return usable_key(k) && !c.contains(k); }",
     "static bool can_insert(const QHash<K, V>&, const K&) { return true; }", False),
    ("traits: a set can be assigned", TRAITS,
     "  static constexpr bool assignable = false;", "  static constexpr bool assignable = true;", False),
    ("model: a move down is counted one row short", MODEL,
     "target > source ? target + 1 : target", "target", False),
    ("model: an insert is announced at row 0", MODEL,
     "m_pendingRow = to_row(rowToInsert(key));", "m_pendingRow = to_row(rowToInsert(key)) * 0;", False),
    ("model: a row is never added to the index", MODEL, "      m_index.insert(key, m_pendingRow);\n", "      (void)key;\n", False),
    ("model: a row is never dropped from the index", MODEL, "      m_index.erase(key, m_pendingRow);\n      --m_rowCount;", "      (void)key;\n      --m_rowCount;", False),
    ("model: a removal is announced for row 0", MODEL,
     "m_pendingRow = to_row(rowOf(key));", "m_pendingRow = to_row(rowOf(key)) * 0;", False),
    ("model: a change is reported for row 0", MODEL,
     "      const int row = to_row(rowOf(key));\n      const int lastColumn",
     "      const int row = to_row(rowOf(key)) * 0;\n      const int lastColumn", False),
    ("model: a clear is never ended", MODEL, "      m_rowCount = 0;\n      this->endResetModel();\n", "      m_rowCount = 0;\n", False),
    ("model: rowCount ignores the parent", MODEL,
     "    return parent.isValid() ? 0 : m_rowCount;",
     "    (void)parent;\n    return m_rowCount;", False),
    ("model: a list offers the key role again", MODEL,
     "static constexpr bool hasKeyRole = Traits::order == Order::sorted || Traits::order == Order::unordered;",
     "static constexpr bool hasKeyRole = true;", False),
    ("model: no container offers the key role", MODEL,
     "static constexpr bool hasKeyRole = Traits::order == Order::sorted || Traits::order == Order::unordered;",
     "static constexpr bool hasKeyRole = false;", False),
    ("container: a range insert is never confirmed", CONTAINER, "      emit rangeAdded(first, last);\n", "", False),
    ("container: a range insert is never announced", CONTAINER, "      emit rangeAboutToBeAdded(first, last);\n", "", False),
    ("container: an empty range is accepted", CONTAINER,
     "if (refusedAsNested() || values.isEmpty() || !Traits::can_insert(m_items, first) ||",
     "if (refusedAsNested() || !Traits::can_insert(m_items, first) ||", False),
    ("container: a range remove may pass the end", CONTAINER, " || first + count > Traits::size(m_items)", "", False),
    ("container: a range remove is announced one row short", CONTAINER,
     "const Key last = first + count - 1;\n      const Announcing announcing(*this);\n      emit rangeAboutToBeRemoved", "const Key last = first + count - 2;\n      const Announcing announcing(*this);\n      emit rangeAboutToBeRemoved", False),
    ("model: a refused edit still replaces", MODEL,
     "return edited && m_source->replace(key, *edited);", "return m_source->replace(key, edited.value_or(Value{}));", False),
    ("model: every cell is editable", MODEL,
     "if (index.isValid() && editable(index.column())) flags |= Qt::ItemIsEditable;", "if (index.isValid()) flags |= Qt::ItemIsEditable;", False),
    ("model: a change reports one column", MODEL,
     "static_cast<const QAbstractItemModel*>(this)->columnCount() - 1;", "0;", False),
    ("model: a range insert is never ended", MODEL,
     "        this->endInsertRows();\n      });\n      QObject::connect(m_source, &Source::rangeAboutToBeRemoved",
     "      });\n      QObject::connect(m_source, &Source::rangeAboutToBeRemoved", False),
    ("multi: a map accepts the occurrence one past its count", TRAITS,
     "static bool contains(const QMultiMap<K, V>& c, const Key& k) { return k.occurrence >= 0 && k.occurrence < c.count(k.key); }",
     "static bool contains(const QMultiMap<K, V>& c, const Key& k) { return k.occurrence >= 0 && k.occurrence <= c.count(k.key); }", False),
    ("multi: a map insert may name any occurrence", TRAITS,
     "static bool can_insert(const QMultiMap<K, V>&, const Key& k) { return usable_key(k.key) && k.occurrence == 0; }",
     "static bool can_insert(const QMultiMap<K, V>&, const Key&) { return true; }", False),
    ("multi: a hash erases the oldest, not the n-th", TRAITS,
     "static void erase(QMultiHash<K, V>& c, const Key& k) { c.erase(std::next(c.find(k.key), k.occurrence)); }",
     "static void erase(QMultiHash<K, V>& c, const Key& k) { c.erase(c.find(k.key)); }", False),
    ("multi: a sorted index puts the row at the key's start", INDEX,
     "qsizetype rowOf(const Key& entry) const { return first_row_of(entry.key) + entry.occurrence; }",
     "qsizetype rowOf(const Key& entry) const { return first_row_of(entry.key); }", False),
    ("multi: a sorted index names the wrong occurrence", INDEX,
     "return Key{key, row - first_row_of(key)};", "return Key{key, row - first_row_of(key) + 1};", False),
    ("multi: an unordered index counts occurrence from the oldest", INDEX,
     "return Key{key, static_cast<qsizetype>(list.end() - at) - 1};", "return Key{key, static_cast<qsizetype>(at - list.begin())};", False),
    ("multi: an unordered erase takes the oldest slot", INDEX,
     "const qsizetype slot = list[list.size() - 1 - static_cast<std::size_t>(entry.occurrence)];\n    list.erase(list.end() - 1 - entry.occurrence);",
     "const qsizetype slot = list[static_cast<std::size_t>(entry.occurrence)];\n    list.erase(list.begin() + entry.occurrence);", False),
    ("multi: an unordered index forgets an emptied key", INDEX,
     "    if (list.empty()) m_slots_of.remove(entry.key);\n", "", True),
    ("model: the key role answers nothing", MODEL,
     "    if (role == Base::KeyRole && Base::hasKeyRole) return QVariant::fromValue(key);\n", "", False),
    ("index: a sorted insert is announced at the end", INDEX,
     "qsizetype rowToInsert(const Key& key) const { return std::lower_bound(m_keys.begin(), m_keys.end(), key, std::less<Key>{}) - m_keys.begin(); }",
     "qsizetype rowToInsert(const Key& key) const { (void)key; return static_cast<qsizetype>(m_keys.size()); }", False),

    # roles, batches and resetWith
    ("container: editValue announces no roles at the about", CONTAINER,
     "emit entryAboutToBeChanged(key, roles);", "emit entryAboutToBeChanged(key, {});", False),
    ("container: editValue announces no roles at the twin", CONTAINER,
     "emit entryChanged(key, roles);", "emit entryChanged(key, {});", False),
    ("container: editValue never confirms", CONTAINER, "      emit entryChanged(key, roles);\n", "", False),
    ("container: editValue edits an absent key", CONTAINER,
     "      if (refusedAsNested() || !Traits::contains(m_items, key)) return false;\n      Value edited", "      if (refusedAsNested()) return false;\n      Value edited", False),
    ("container: insertMany never announces", CONTAINER, "      emit entriesAboutToBeAdded(keys);\n", "", False),
    ("container: insertMany never confirms", CONTAINER, "      emit entriesAdded(keys);\n", "", False),
    ("container: insertMany replaces keys that are there", CONTAINER,
     "        if (Traits::can_insert(m_items, key)) keys.append(key);", "        keys.append(key);", False),
    ("container: insertMany announces nothing left to add", CONTAINER,
     "      if (keys.isEmpty() || !fits_rows(Traits::size(m_items), keys.size())) return 0;\n      const Announcing announcing(*this);\n      emit entriesAboutToBeAdded", "      const Announcing announcing(*this);\n      emit entriesAboutToBeAdded", False),
    ("container: removeMany never announces", CONTAINER, "      emit entriesAboutToBeRemoved(present);\n      for (const Key& key : present) Traits::erase(m_items, key);\n      emit entriesRemoved(present);\n      return present.size();",
     "      for (const Key& key : present) Traits::erase(m_items, key);\n      emit entriesRemoved(present);\n      return present.size();", False),
    ("container: removeMany never confirms", CONTAINER, "      emit entriesRemoved(present);\n      return present.size();", "      return present.size();", False),
    ("container: removeMany keeps absent keys (a map)", CONTAINER,
     "if (Traits::contains(m_items, key) && seen.insert(key).second) out.append(key);", "if (seen.insert(key).second) out.append(key);", False),
    ("container: removeMany keeps repeated keys (a map)", CONTAINER,
     "if (Traits::contains(m_items, key) && seen.insert(key).second) out.append(key);", "if (Traits::contains(m_items, key)) out.append(key);", False),
    ("container: removeMany keeps repeated keys (a hash)", CONTAINER,
     "if (Traits::contains(m_items, key) && !seen.contains(key)) {", "if (Traits::contains(m_items, key)) {", False),
    ("container: takeMany returns nothing", CONTAINER,
     "      for (const Key& key : present) Traits::insert(taken, key, Traits::value(m_items, key));\n", "", False),
    ("container: takeMany never confirms", CONTAINER,
     "      emit entriesRemoved(present);\n    }\n    return taken;", "    }\n    return taken;", False),
    ("container: resetWith never announces", CONTAINER, "    emit aboutToBeReset();\n", "", False),
    ("container: resetWith never confirms", CONTAINER, "    emit resetCompleted();\n", "", False),
    ("container: resetWith skips an empty reset", CONTAINER,
     "    if (Traits::size(content) > kMaxRows) return false;\n    const Announcing announcing(*this);\n    emit aboutToBeReset();", "    if (Traits::size(content) > kMaxRows) return false;\n    if (Traits::size(m_items) == 0 && Traits::size(content) == 0) return true;\n    const Announcing announcing(*this);\n    emit aboutToBeReset();", False),
    ("model: a batch insert gives every new key the same offset", MODEL,
     "rows.push_back(rowToInsert(keys[i]) + i);", "rows.push_back(rowToInsert(keys[i]));", False),
    ("model: a batch insert is one span", MODEL,
     "while (to + 1 < rows.size() && rows[to + 1] == rows[to] + 1) ++to;\n      this->beginInsertRows",
     "while (to + 1 < rows.size()) ++to;\n      this->beginInsertRows", False),
    ("model: a batch insert is one cycle per row", MODEL,
     "while (to + 1 < rows.size() && rows[to + 1] == rows[to] + 1) ++to;\n      this->beginInsertRows",
     "while (false) ++to;\n      this->beginInsertRows", False),
    ("model: a batch insert is not counted", MODEL,
     "      m_rowCount += to_row(static_cast<qsizetype>(to - from + 1));\n      this->endInsertRows();", "      this->endInsertRows();", False),
    ("model: a batch removal takes its rows in the order of the list", MODEL,
     "    std::sort(doomed.begin(), doomed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });\n", "", False),
    ("model: a batch removal drops the rows of a run from the top", MODEL,
     "for (std::size_t k = to; k-- > from;) m_index.erase(doomed[k].second, doomed[k].first);",
     "for (std::size_t k = from; k < to; ++k) m_index.erase(doomed[k].second, doomed[k].first);", False),
    ("model: a batch removal is not counted", MODEL,
     "      m_rowCount -= to_row(static_cast<qsizetype>(to - from));\n      this->endRemoveRows();", "      this->endRemoveRows();", False),
    ("model: a batch removal announces one row too many", MODEL,
     "to_row(doomed[to - 1].first));\n      for (std::size_t k = to;", "to_row(doomed[to - 1].first) + 1);\n      for (std::size_t k = to;", False),
    ("model: a batch removal runs are never split", MODEL,
     "while (from > 0 && doomed[from - 1].first + 1 == doomed[from].first) --from;", "while (from > 0) --from;", False),
    ("model: a reset keeps the old rows", MODEL,
     "      m_index.assign(Traits::keys(m_source->items()));\n      m_rowCount = to_row(m_source->size());\n    } else {",
     "      m_rowCount = to_row(m_source->size());\n    } else {", False),
    ("model: a reset keeps the old count", MODEL,
     "      m_index.assign(Traits::keys(m_source->items()));\n      m_rowCount = to_row(m_source->size());\n    } else {",
     "      m_index.assign(Traits::keys(m_source->items()));\n    } else {", False),
    ("model: a reset is never begun", MODEL,
     "QObject::connect(m_source, &Source::aboutToBeReset, this, [this] {\n      m_open = true;\n      this->beginResetModel();\n    });",
     "QObject::connect(m_source, &Source::aboutToBeReset, this, [this] {\n      m_open = true;\n    });", False),
    ("model: a reset is never ended", MODEL,
     "      takeContent();\n      this->endResetModel();", "      takeContent();", False),
    ("model: a clear keeps the old count", MODEL, "      m_index.clear();\n      m_rowCount = 0;\n", "      m_index.clear();\n", False),
    ("model: an insert is not counted", MODEL, "      ++m_rowCount;\n", "", False),
    ("model: a remove is not counted", MODEL, "      --m_rowCount;\n      this->endRemoveRows();", "      this->endRemoveRows();", False),
    ("model: a range insert is not counted", MODEL,
     "        m_rowCount += to_row(last - first + 1);\n", "        (void)first;\n        (void)last;\n", False),
    ("model: a range removal is not counted", MODEL,
     "        m_rowCount -= to_row(last - first + 1);\n", "        (void)first;\n        (void)last;\n", False),
    ("model: the roles of a change are dropped", MODEL,
     "this->index(row, lastColumn), roles);", "this->index(row, lastColumn));\n      (void)roles;", False),
    # keys and sizes
    ("traits: a NaN key is usable", TRAITS, "    return key == key;", "    return (void)key, true;", False),
    ("index: a map is searched by operator< and not by std::less", INDEX,
     "std::lower_bound(m_keys.begin(), m_keys.end(), key, std::less<Key>{}) - m_keys.begin(); }", "std::lower_bound(m_keys.begin(), m_keys.end(), key) - m_keys.begin(); }", False),
    ("container: an insert past the row limit", CONTAINER, " || !fits_rows(Traits::size(m_items), 1)) return false;", ") return false;", False),
    ("container: a reset past the row limit", CONTAINER, "    if (Traits::size(content) > kMaxRows) return false;\n    const Announcing announcing(*this);\n    emit aboutToBeReset();", "    const Announcing announcing(*this);\n    emit aboutToBeReset();", False),
    ("container: a reset keeps the NaN entries", CONTAINER, "    drop_unusable_keys(content);\n", "", False),
    ("container: a constructor keeps the NaN entries", CONTAINER, "    drop_unusable_keys(m_items);\n", "", False),
    # the keyed matrix, its table model and the selection tracker
    ("matrix: a row is announced after it is added", MATRIX, "    emit rowAboutToBeAdded(key, index);\n    appendRows({key});", "    appendRows({key});\n    emit rowAboutToBeAdded(key, index);", False),
    ("matrix: addRows names one index too many", MATRIX, "last = first + fresh.size() - 1;\n    emit rowsAboutToBeAdded", "last = first + fresh.size();\n    emit rowsAboutToBeAdded", False),
    ("matrix: removeRows goes from the first run", MATRIX,
     "for (auto run = runs.rbegin(); run != runs.rend(); ++run) {\n      const QList<RowKey> keysOfRun", "for (auto run = runs.begin(); run != runs.end(); ++run) {\n      const QList<RowKey> keysOfRun", False),
    ("matrix: a row rename says nothing", MATRIX, "    emit rowRenamed(oldKey, to, index);\n", "", False),
    ("matrix: a rename onto a row in use", MATRIX, "    if (hasRow(to)) return false;\n", "", False),
    ("matrix: a rename leaves the key index", MATRIX, "    m_rowIndex.rename(oldKey, to);\n", "", False),
    ("matrix: setValue announces an unchanged cell", MATRIX, "    if (sameValue(m_cells[cellOffset(i, j)], v)) return true;\n", "", False),
    ("matrix: a gap between rows joins a rectangle", MATRIX, "      if (row != previousRow + 1) open.clear();  // a gap ends every rectangle\n", "      if (false && row != previousRow + 1) open.clear();\n", False),
    ("matrix: the first of a repeated cell wins", MATRIX, "changed.insert_or_assign(Position{i, j}, v);", "changed.emplace(Position{i, j}, v);", False),
    ("matrix: unchanged cells are counted", MATRIX,
     "return sameValue(m_cells[cellOffset(entry.first.first, entry.first.second)], entry.second); });", "return (void)entry, false; });", False),
    ("matrix: a removed column takes the wrong cells", MATRIX, "m_cells.remove(r * oldWidth + first, last - first + 1);", "m_cells.remove(r * oldWidth, last - first + 1);", False),
    ("matrix: new columns are inserted at the start of a row", MATRIX, "m_cells.insert((r + 1) * oldWidth, keys.size(), m_sentinel);", "m_cells.insert(r * oldWidth, keys.size(), m_sentinel);", False),
    ("matrix: a nested change is not refused", MATRIX, "    return m_announcing;\n  }\n\n  qsizetype cellOffset", "    return false;\n  }\n\n  qsizetype cellOffset", False),
    ("matrix: a NaN key is added", MATRIX, "return usable_key(key) && !index.contains(key) && fits_rows(count, 1);", "return !index.contains(key) && fits_rows(count, 1);", False),
    ("matrix: clear keeps the row index", MATRIX, "    m_rowIndex.clear();\n    m_columnIndex.clear();", "    m_columnIndex.clear();", False),
    ("matrix: a removed row stays in the index", MATRIX, "    for (qsizetype r = first; r <= last; ++r) m_rowIndex.erase(m_rows[r], r);\n", "", False),
    ("matrix model: a rename is not a header change", MATRIX_MODEL,
     "      if (i < m_rows) headerDataChanged(Qt::Vertical, to_row(i), to_row(i));", "      (void)i;", False),
    ("matrix model: dataChanged covers whole rows", MATRIX_MODEL,
     "emit dataChanged(index(to_row(top), to_row(left)), index(to_row(bottom), to_row(right)));", "emit dataChanged(index(to_row(top), to_row(left) * 0), index(to_row(bottom), to_row(right)));", False),
    ("matrix model: removed rows are not counted", MATRIX_MODEL, "    m_rows -= to_row(last - first + 1);\n", "    m_rows -= 0 * to_row(last - first + 1);\n", False),
    ("matrix model: added columns are not counted", MATRIX_MODEL, "    m_columns += to_row(last - first + 1);\n", "    m_columns += 0 * to_row(last - first + 1);\n", False),
    ("matrix model: the edit role is not answered", MATRIX_MODEL, "if ((role != Qt::DisplayRole && role != Qt::EditRole) ||", "if (role != Qt::DisplayRole ||", False),
    ("matrix model: it reads the matrix ahead of itself", MATRIX_MODEL,
     "    if (index.row() >= m_source->rowCount() || index.column() >= m_source->columnCount()) return {};  // the matrix is ahead\n", "", False),
    ("matrix model: a model made mid-change ends the change", MATRIX_MODEL, "    resync();\n    return false;\n  }\n\n  void begin()", "    return true;\n  }\n\n  void begin()", False),
    ("selection: an insert at the selected row does not push it", SELECTION, "if (!m_state.has_value() || *m_state < first) return false;", "if (!m_state.has_value() || *m_state <= first) return false;", False),
    ("selection: a removed selected row of a Multi is not counted", SELECTION, "      m_selected -= removedSelected;\n", "", False),
    ("selection: the selected row of a Single survives its removal", SELECTION, "      if (*m_state <= last) {", "      if (*m_state < last) {", False),
    ("selection: a Single set always flips the presence", SELECTION, ".presenceFlipped = flipped, .selectionChanged = true, .countChanged = flipped};", ".presenceFlipped = true, .selectionChanged = true, .countChanged = flipped};", False),
    ("selection: the selected role is a base role", SELECTION, "return static_cast<int>(largest + 1);", "return static_cast<int>(largest);", False),
    ("selection: a Multi clear reads the rows after it clears", SELECTION,
     "  const std::vector<int> rows = tracker.selectedRows();\n  tracker.clear();\n\n  const QList<int> roles{T::selectedRole()};", "  tracker.clear();\n  const std::vector<int> rows = tracker.selectedRows();\n\n  const QList<int> roles{T::selectedRole()};", False),
    ("selection: a Single select forgets the old row", SELECTION, "  if (old.has_value()) host->dataChanged(host->index(*old, 0), host->index(*old, 0), roles);\n", "  if (false && old.has_value()) host->dataChanged(host->index(*old, 0), host->index(*old, 0), roles);\n", False),
    ("selection: a Multi set announces no count", SELECTION, "  if (delta.countChanged) host->selectedCountChanged();\n  if (delta.presenceFlipped) host->hasSelectionChanged();\n  return true;", "  if (delta.presenceFlipped) host->hasSelectionChanged();\n  return true;", False),
    ("model: a row the container dropped is still read", MODEL,
     "return m_source && m_source->contains(key) ? &m_source->value(key) : nullptr;", "return m_source ? &m_source->value(key) : nullptr;", False),
]


def run(cmd, **kw):
    return subprocess.run(cmd, text=True, capture_output=True, **kw)


def main(argv):
    if os.path.exists(WORK):
        shutil.rmtree(WORK)
    shutil.copytree(SRC, WORK, ignore=shutil.ignore_patterns("build", ".git"), symlinks=True)
    os.chdir(WORK)
    configured = run(["cmake", "--preset", "qt"])
    if configured.returncode:
        sys.exit("configure failed:\n" + configured.stdout[-2000:] + configured.stderr[-2000:])
    baseline = run(["cmake", "--build", "--preset", "qt", "--target", *TARGETS])
    if baseline.returncode:
        sys.exit("baseline does not build:\n" + baseline.stderr[-3000:])

    def exe(name):
        for root, _, files in os.walk("build"):
            if name in files and os.access(os.path.join(root, name), os.X_OK):
                return os.path.join(root, name)
        raise SystemExit(f"no binary {name}")

    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", LC_ALL="C.UTF-8")

    def tests_pass():
        for name in TARGETS:
            try:
                if run([exe(name)], env=env, timeout=TEST_TIMEOUT_SECONDS).returncode:
                    return False
            except subprocess.TimeoutExpired:
                return False
        return True

    assert tests_pass(), "the unmutated tests must pass first"
    chosen = [m for m in MUTANTS if not argv or any(a in m[0] for a in argv)]
    rows, killed, survived, unviable = [], 0, [], 0
    for name, path, old, new, equivalent in chosen:
        original = open(path).read()
        if old not in original:
            sys.exit(f"mutant text not found in {path}: {name}")
        open(path, "w").write(original.replace(old, new, 1))
        try:
            built = run(["cmake", "--build", "--preset", "qt", "--target", *TARGETS])
            if built.returncode:
                verdict = "not viable"
                unviable += 1
            elif tests_pass():
                verdict = "SURVIVED (judged equivalent)" if equivalent else "SURVIVED"
                survived.append((name, equivalent))
            else:
                verdict = "killed"
                killed += 1
        finally:
            open(path, "w").write(original)
        print(f"{verdict:30} {name}", flush=True)
        rows.append((name, verdict))
    equivalents = sum(1 for _, e in survived if e)
    print(f"\nMutation: hand-listed textual mutants on observable_models, {len(chosen)} mutants: {killed} killed, "
          f"{len(survived)} survived ({equivalents} judged equivalent), {unviable} not viable")
    return 1 if any(not e for _, e in survived) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
