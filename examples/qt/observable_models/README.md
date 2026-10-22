# observable_models

An observable Qt container and one generic list model, written once, with typed signals. It is the answer to
wiring a container to a view by hand for every data type: a hand-written adapter has to pair every
`begin...Rows` with its `end...Rows`, find the right row, and keep the selection and persistent indexes valid, and
a mistake in any of them shows up as a ghost row or a ghost selection.

| file | what it holds |
|---|---|
| `container_traits.hpp` | per container: its key, how a new key finds its position, which changes it supports |
| `observable_container.hpp` | `ObservableContainer<C>`: insert, remove, replace, move, clear, batches and reset, each announced before and confirmed after, with typed `rqt::signal` signals |
| `container_model.hpp` | `ContainerModel<C>`: a `QAbstractListModel` for any of them, driven by those signals |
| `observable_matrix.hpp` | `ObservableMatrix<RowKey, ColKey, Value>`: a dense matrix labelled by keys, with typed signals |
| `matrix_table_model.hpp` | `MatrixTableModel<M>`: a `QAbstractTableModel` for it |
| `selection_tracker.hpp` | `SelectionTracker<Mode, Roles>`: the selection of a list model, Single or Multi, and the helpers that announce its changes |

`ObservableContainer` is a class template with signals, which moc cannot compile. The header is C++26 and needs
reflect-moc (GCC 16.2, Qt 6.10).

## Containers

| container | the key is | row order |
|---|---|---|
| `QList<T>`, `QQueue<T>`, `QStack<T>`, `QStringList` | the index (the model offers no key role: the row is the identity, and it shifts without a signal) | the index |
| `QMap<K, V>` | the key | the key order |
| `QHash<K, V>`, `QSet<K>` | the key | the order entries were added |
| `QMultiMap<K, V>` | a `MultiKey`: the key and the occurrence, 0 the most recently inserted (no key role, the occurrence shifts) | the key order, the newest of a key first |
| `QMultiHash<K, V>` | a `MultiKey`, as above | the order entries were added |

A new entry of a multi container is occurrence 0 and the older ones of its key move up, so an insert with another
occurrence is refused. The models find a row from a key in logarithmic time (`row_index.hpp`): the key itself for a
list, a sorted copy of the keys for a map, a Fenwick tree over insertion slots for a hash or set.

## Keys

- **NaN.** A floating point key that is not equal to itself cannot be found again (a hash never finds it, a map finds it
  for every key), so `insert` refuses it, `insertMany` skips it like a key that is already there, `contains`, `remove`
  and `replace` never match it, and a content given whole (the constructor, `resetWith`) loses the entries that have
  one. Infinity is a key, and `-0.0` and `0.0` are one key.
- **Order of a map.** `QMap<K>` orders by `std::less<K>`, which a program may specialise, not by `operator<`. The
  model's sorted index of the keys uses `std::less<K>` too, so its rows follow the container.
- **Row limit.** A model counts rows in an `int`. A change that would give a container more than `observable::kMaxRows`
  (`INT_MAX`) entries is refused: `insert`, `insertRange` and `insertMany` return `false` or 0, `resetWith` returns
  `false`, nothing is announced. A constructor cannot refuse, and asserts in a debug build.
- **Documented, not fixed.** A reference from `value(key)` is valid until the next change (a copy of `items()` held
  elsewhere makes the next change detach, and the reference then points into that copy). A hash or set that arrives
  whole (constructor, `resetWith`, an `insertMany` argument) is listed in the hash's own iteration order, which changes
  with its seed; after that the model follows insertion order. Hand a sorted container in if the order matters.

## What else the models do

- `insertRange` and `removeRange` change a run of a list in one announcement, so a model makes one begin and one end.
- `insertMany`, `removeMany` and `takeMany` do the same for a map, hash or set: a gesture on N entries is one event,
  announced once with the list of keys. Keys that are already there (or absent, or repeated) are skipped, and a call
  with nothing left announces nothing. The model splits the list into runs of consecutive rows and makes one begin
  and one end per run, so a view keeps its selection and scroll position. `to.insertMany(from.takeMany(keys))` moves
  entries between two containers, one announcement on each side. A list has `insertRange`, and a multi container has no
  key to name an entry by before it exists, so both return 0.
- A batch is staged, then announced. `insertMany` copies each value once into a local staging container, makes the
  list of keys, detaches a shared container and reserves room, all before `entriesAboutToBeAdded`; the commit moves
  the staged values in (a map takes the staged nodes over, a hash emplaces them by move). A value whose copy throws
  therefore changes nothing and announces nothing, at the first, a middle or the last entry. `takeMany` copies before
  it announces too, and `resetWith` takes its content by value, so the caller's copy happens before the call and the
  swap in is a move. The cost is one staging container per batch; the copies per entry went from two to one. What a
  Qt container's own allocation can still do inside the commit (an allocation that fails there) is not covered.
- `resetWith(content)` replaces everything between one pair of signals, and the model follows with one reset. No
  listener sees the empty state that `clear()` followed by inserts would show. It announces even when both sides are
  empty.
- `editValue(key, edit, roles)` changes a value in place, and the data signals carry the roles that changed; the model
  passes them to `dataChanged`, and an empty list means every role. `replace` announces an empty list.
- The model counts its own rows. While a batch is announced the container already holds the final content, so its size
  differs from what the view has been told.
- A batch removal (`removeMany`, `takeMany`) erases the entries before the model announces its runs, yet a receiver of
  `rowsAboutToBeRemoved` may read the rows that are about to go. So the model copies the (key, value) pairs of the
  doomed entries when the batch is announced, serves them to `data()` while the runs are announced, and drops the
  copies after the last one. The cost is k value copies per batch removal of k entries; a single `remove` and
  `removeRange` announce before they erase and copy nothing.
- `setData` builds the new value with the column's editor and calls `replace`; a column without an editor is read only.
  An editable column answers `Qt::EditRole` with what it displays, so a delegate starts from the value. `data` and
  `setData` refuse an index of another model (Qt's `checkIndex` warns about it) and an invalid one.
- A table with no column announces no `dataChanged` (its row has no cell to name).
- `ContainerTableModel<C>` gives one column per `Column` (header, display function, optional editor).
- A container and its models live in one thread. A debug build asserts it; move data between threads, not the model.
- A value type whose move can throw is refused at compile time: a Qt container allocates inside the insert, so a
  throw between the "about" and the "done" signal could not be undone.

A mutator that is asked for something invalid (an index out of range, a key already present, a replace of a missing
key, a move in a container with no order) returns `false`, changes nothing and emits nothing. A `QSet` has no value to
replace, so `replace` returns `false` for it.

## The keyed matrix

`ObservableMatrix<RowKey, ColKey, Value>` is a table whose rows and columns are named by keys (a string, a number, a struct
with `qHash`) and whose cells hold a `Value`; a cell nobody wrote holds the value the matrix was made with (the
"uninitialised" value, tested with `isUninitialized`, which treats a NaN as equal to a NaN). Reading is total: a row or column
that is not there gives that value and creates nothing. The mutators follow the container's rules: a request for something
invalid (a key already there, a key that is not, a rename onto a key in use, a NaN key) returns false (0 for a batch), changes
nothing and says nothing; a mutator called from a receiver of the matrix's own signals is refused and a debug build asserts; a
matrix and its models live in one thread.

- Rows and columns are added at the end, one at a time or many (one pair of signals over the new range), and removed one or many
  (one pair per run of consecutive indices, from the last run to the first, so the runs still to do keep their indices).
- A rename is its own pair of signals: the row keeps its index and cells, and the table model emits `headerDataChanged`, never a
  removal and an insertion, so a selection and a persistent index survive it.
- `setValues` announces the cells that changed as FULL rectangles (rows that follow each other with the same columns make one),
  never the bounding box: all the "about" signals first, then the writes, then the twins. A cell set to the value it holds says
  nothing; the last of a repeated cell wins.
- `MatrixTableModel` counts its own rows and columns, holds the matrix by `QPointer` (it resets to empty when the matrix goes),
  answers the display and edit roles with the value (an invalid `QVariant` for an uninitialised cell), and reads empty, never
  out of range, in the moment between the matrix's signal and its own. It is read only.
- The index of a key is found with `InsertionOrderIndex` (a Fenwick tree over insertion slots, `row_index.hpp`), so a lookup, an
  add, a rename and a removal of a key cost O(log n) however many rows there are. The spec's own design marks a hash of keys stale
  after every change and rebuilds it at the next lookup, which costs a whole pass over the keys per change: see
  docs/measurements.md ("The keyed matrix"). What a removal still costs is the move of the cells behind it, which a dense
  row-major table cannot avoid.

## Selection

`SelectionTracker<Mode, Roles>` keeps which rows of a list model are selected, in a form that follows the rows when they come
and go. It is a plain value (no `QObject`), owned by the model that hosts it; the host announces what a change did with its own
`rqt::signal` members.

- `Mode::Single` holds at most one row, `Mode::Multi` one flag per row. Every mutation returns a `SelectionDelta` with three
  separate facts: the presence of a selection flipped, the selection changed, the count changed (a second ticked row changes the
  count, not the presence).
- `Roles` is a `std::array` of the host's own roles, checked at compile time by `ValidRoleList`: not empty, all distinct, all
  strictly above `Qt::UserRole` (which the container models use for the key). The tracker's role, named `isSelected` for QML, is
  the largest of them plus one. A list that fails does not compile.
- `applySelectionValue` (the `setData` branch of the role), `clearSelectionAndNotify` and `selectedValue` are free functions. The
  host must have `hasSelectionChanged` and `selectedRowChanged` (Single) or `selectedCountChanged` (Multi) as members (the
  `SingleHost` and `MultiHost` concepts); `selectedValue` is for Single only and says so at compile time for a Multi tracker.
- The order of what a host announces: the selected row or count first, then `dataChanged` (with the selected role alone) for the
  rows whose flag changed, then whether there is a selection. A host that follows the rows of a container keeps the tracker in
  step in the model's own signals (so it already points right when a view hears of an insertion or a removal) and announces its
  notifications afterwards. `tests/qt/selection_hosts.hpp` is a worked example, a Single and a Multi model over a container of
  colours, and `capability_observable_selection_*` the tests of it.

## Tests

Both run in ctest (`ctest --preset qt -R observable`).

- `capability_observable_models_stress` applies 4000 random changes per seed, for 8 seeds, to each container above.
  After every change the container, the model, a view mirror, the selection and a persistent index must agree with a
  plain reference. The view mirror is built from the model's signals only, as a view would be, so a change that was
  not announced, announced for the wrong rows, announced late or never confirmed makes it differ. A
  `QAbstractItemModelTester` in fatal mode runs throughout.
- `capability_observable_models_flaws_detected` runs the same driver against hand-written adapters with one mistake
  each (a missing `endInsertRows`, a late announcement, a wrong row on remove, a replace with no `dataChanged`, a
  missing `endMoveRows`, a missing `endResetModel`) and requires every one to be reported, and the right adapter not
  to be.

- `capability_observable_container_announcements` checks the container's own signals: each change is announced
  before it happens and confirmed after, the container shows its old state at the announcement and its new state at
  the confirmation, and a refused request announces nothing.
- `capability_observable_matrix_signals`, `_model` and `_random` check the matrix and its model: the signal contract (the spec's acceptance
  tests), the model in step after every structural change, and 8 seeds of 3,000 random changes against a plain reference, with a view
  mirror built from signals alone, a selection of cells and a persistent index, and three flawed adapters (a rename announced as a removal and
  an insertion, a bounding box, a column with no announcement) that it must catch.
- `capability_observable_selection_tracker`, `_models` and `_random` check the tracker (the deltas), its two example hosts (the order of
  notifications) and 8 seeds of 4,000 random changes with exact notifications; the `limit_observable_selection_*` files must not compile.
- `capability_observable_batches` checks the batch and reset contract: one announcement per batch with the keys that
  were really added or removed, the exact begin and end cycles a model makes (runs from the top for an insert, from the
  bottom for a removal), the selection and a persistent index surviving a batch and not a reset, roles reaching
  `dataChanged`, and 4000 random batches against a reference.
- The random driver of the stress test also calls `editValue`, `insertMany`, `removeMany` and `resetWith`, and the
  flaws test has a hand-written map model with four batch mistakes (runs removed in the wrong order, a scattered
  insert announced as one span, a reset that keeps the old rows, a row count read from the container).

## Mutation testing

`scripts/mutate-observable-models.py` applies 133 hand-listed single-line mutants to a copy of these headers, rebuilds
the observable-model tests and runs them. Mull needs clang and the image has only GCC, so the mutants are written by
hand (`docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 python3 scripts/mutate-observable-models.py`;
give it part of a mutant's name to run only that one).
Result: 93 killed, 3 survived, all three judged equivalent. They keep a stale hash entry that the next insert of the
key overwrites, never compact dead slots (only memory grows), and keep an emptied key in a multi hash's slot lists.

Since then 37 mutants were added and run: 6 for the NaN and row-limit rules (all killed) and 31 for the matrix, its model and the
selection tracker (all killed). The first run of the 31 gave 25 killed, one survivor (a row announced after it was added: no test read the
matrix at the "about" signal, and `capability_aboutSeesTheOldStateAndTheTwinTheNew` now does) and five that did not compile (an unused
parameter under `-Werror`, fixed in the mutant). The other 96 were last run before the re-entrancy guard; 14 of their texts had gone
stale against it and were updated to the current code, and they were not run again here.

The script had gone stale: 17 of its 59 mutants no longer matched the sources after the row index replaced the
model's own key list, and the runner stops at the first one it cannot apply. They were rewritten against the current
code. Of the mutants added for batches, resets and roles, six did not compile at first (an unused parameter under
`-Werror`) and were fixed; none survived. The batch tests pass the keys of a removal in an order other than the
rows', because a removal that trusted the caller's order would otherwise go unnoticed.

## What this does not show

The tests come from the `QAbstractItemModel` contract, not from anyone's production code. They show that these
classes keep the contract on the changes listed; they do not show that a given application has no ghost rows.
Selection behaviour is checked for `QItemSelectionModel` and `QPersistentModelIndex`; proxy models and views that
cache row data are not covered.
