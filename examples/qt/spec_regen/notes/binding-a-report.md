# Binding A regeneration: report (agent text, verified by a host rebuild)

Build: GCC 16.2, Qt 6.10.3, `-Wall -Wextra -Wpedantic`, 0 warnings (Release and Debug).
ctest 17/17 (re-run by me, source mounted read-only). AT-1.1 to AT-9.6 all passed, including the six compile-fail pairs.
Not tested: Clang, MSVC.

## Ambiguities and the reading chosen
1. 6.2 `is_std_array_v`: who strips cv-ref is unsaid; the variable template does.
2. 9.4 step 1: order of "last occurrence wins" and "skip unchanged" unsaid; dedupe first, then drop unchanged (the other order writes a stale value for `[(c,new),(c,current)]`).
3. 9.2 matrix `clear()` "empty": taken as no rows and no columns.
4. 9.1/9.3 NaN when the sentinel is not NaN: NaN over NaN counts as a change.
5. 8.1 `HashableKey` stated only in the associative paragraph: `static_assert` in the class for both kinds, no `requires` (15.4 lists only `SupportedContainer`).
6. 8.2 `entryDataChanged` for a key with no row: nothing emitted (R2).
7. 7.2 `insertMany` key order follows the iteration order of `entries` (arbitrary for QHash); tests compare sorted.
8. 7.2 batch key dedup: O(n) through a QHash for hashable keys, O(n^2) otherwise.
9. 10.2 Multi `hasSelection()`/`count()` cost unspecified: private counter, O(1).
10. 10.5 `selectedValue` on Multi: `static_assert` in both overloads, body skipped with `if constexpr`.
11. 10.5 Single `applySelectionValue` early returns written as `isSelected(row) == requested`.
12. 11 T13 vs 12.8 AT-8.4: T13 removes row 2, AT-8.4 says apply it to the new selected row (3); used `removeItem(3)`.
13. 14 vs 15.2/15.5: `src/obs_signals.cpp` may include only the two headers and the synopsis declares only constructors; constructors are inline in the headers.
14. 12 intro: `QSignalSpy` for counts not used; recorder logs give order and counts.
15. 12.4 test model "implements roleNames" while the base class has one: overrides by calling the base and adding a role.

## Possible design mistakes (agent's opinion)
1. 8.3 batch removal: `beginRemoveRows` runs after the container erased the entries, so `data()` of the doomed rows is invalid during the cycle; a proxy or view reading them in `rowsAboutToBeRemoved` sees nothing. The spec chose this knowingly (C5).
2. 7.1 vs G3: G3 lists "a value already equal" as nothing changed, but `replace` always notifies.
3. 14 vs 10.5/6.4: `sel_kit` is meant to be independent of `obs_kit`, but Multi `clearSelectionAndNotify` needs `contiguousRuns` from `obs`; a neutral header would be cleaner.
4. 15.3 `template <class Self = C>`: a caller can write `insertMany<QHash<int,int>>(...)` on a QList container and defeat the `static_assert`.
5. 10.6 `rowInserted` returns nothing, so every host compares `selectedRow()` before and after; returning a `Delta` as `rowRemoved` does would remove that.
6. 10.2 a Multi tracker starts with 0 rows; a host over a model that already has rows must call `reset(n)` first or `Q_ASSERT` fires.
7. 16 the Qt API list is smaller than the spec needs: `QModelIndex`, `Qt::ItemFlags`, `Qt::Orientation`, `CheckIndexOption`, `QObject::disconnect`, `QMetaObject::Connection`, and in tests `QVariant::toString/toInt/toDouble`, `QString::number`, `QChar`.

## Other
- Qt 6.12 vs 6.10.3: nothing needed 6.12; `QMap` has no `emplace` in 6.10.3, so `container.h` has a private `insertEntry`.
- Compiler dependence: in `Container<QList<T>>`, `insert(qsizetype, Value)` and `insert(const Key&, Value)` have the same signature and differ only by `requires`; GCC 16.2 accepts it, Clang and MSVC not tested.
- Could not be built as written: nothing.
