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

## What else the models do

- `insertRange` and `removeRange` change a run of a list in one announcement, so a model makes one begin and one end.
- `insertMany`, `removeMany` and `takeMany` do the same for a map, hash or set: a gesture on N entries is one event,
  announced once with the list of keys. Keys that are already there (or absent, or repeated) are skipped, and a call
  with nothing left announces nothing. The model splits the list into runs of consecutive rows and makes one begin
  and one end per run, so a view keeps its selection and scroll position. `to.insertMany(from.takeMany(keys))` moves
  entries between two containers, one announcement on each side. A list has `insertRange`, and a multi container has no
  key to name an entry by before it exists, so both return 0.
- `resetWith(content)` replaces everything between one pair of signals, and the model follows with one reset. No
  listener sees the empty state that `clear()` followed by inserts would show. It announces even when both sides are
  empty.
- `editValue(key, edit, roles)` changes a value in place, and the data signals carry the roles that changed; the model
  passes them to `dataChanged`, and an empty list means every role. `replace` announces an empty list.
- The model counts its own rows. While a batch is announced the container already holds the final content, so its size
  differs from what the view has been told, and a row inside a removal that the container has dropped reads as empty.
- `setData` builds the new value with the column's editor and calls `replace`; a column without an editor is read only.
- `ContainerTableModel<C>` gives one column per `Column` (header, display function, optional editor).
- A container and its models live in one thread. A debug build asserts it; move data between threads, not the model.
- A value type whose move can throw is refused at compile time: a Qt container allocates inside the insert, so a
  throw between the "about" and the "done" signal could not be undone.

A mutator that is asked for something invalid (an index out of range, a key already present, a replace of a missing
key, a move in a container with no order) returns `false`, changes nothing and emits nothing. A `QSet` has no value to
replace, so `replace` returns `false` for it.

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
- `capability_observable_batches` checks the batch and reset contract: one announcement per batch with the keys that
  were really added or removed, the exact begin and end cycles a model makes (runs from the top for an insert, from the
  bottom for a removal), the selection and a persistent index surviving a batch and not a reset, roles reaching
  `dataChanged`, and 4000 random batches against a reference.
- The random driver of the stress test also calls `editValue`, `insertMany`, `removeMany` and `resetWith`, and the
  flaws test has a hand-written map model with four batch mistakes (runs removed in the wrong order, a scattered
  insert announced as one span, a reset that keeps the old rows, a row count read from the container).

## Mutation testing

`scripts/mutate-observable-models.py` applies 96 hand-listed single-line mutants to a copy of these headers, rebuilds
the observable-model tests and runs them. Mull needs clang and the image has only GCC, so the mutants are written by
hand (`docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 python3 scripts/mutate-observable-models.py`;
give it part of a mutant's name to run only that one).
Result: 93 killed, 3 survived, all three judged equivalent. They keep a stale hash entry that the next insert of the
key overwrites, never compact dead slots (only memory grows), and keep an emptied key in a multi hash's slot lists.

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
