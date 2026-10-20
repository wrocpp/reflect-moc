# observable_models

An observable Qt container and one generic list model, written once, with typed signals. It is the answer to
wiring a container to a view by hand for every data type: a hand-written adapter has to pair every
`begin...Rows` with its `end...Rows`, find the right row, and keep the selection and persistent indexes valid, and
a mistake in any of them shows up as a ghost row or a ghost selection.

| file | what it holds |
|---|---|
| `container_traits.hpp` | per container: its key, how a new key finds its position, which changes it supports |
| `observable_container.hpp` | `ObservableContainer<C>`: insert, remove, replace, move and clear, each announced before and confirmed after, with `rqt::signal<void(const Key&)>` signals |
| `container_model.hpp` | `ContainerModel<C>`: a `QAbstractListModel` for any of them, driven by those signals |

`ObservableContainer` is a class template with signals, which moc cannot compile. The header is C++26 and needs
reflect-moc (GCC 16.2, Qt 6.10).

## Containers

| container | the key is | row order |
|---|---|---|
| `QList<T>`, `QQueue<T>`, `QStack<T>`, `QStringList` | the index | the index |
| `QMap<K, V>` | the key | the key order |
| `QHash<K, V>`, `QSet<K>` | the key | the order entries were added (the model keeps a key list) |

Not supported: `QMultiMap` and `QMultiHash`. Two entries can share a key there, so a key does not identify an entry;
the container would need another identity (an iterator or an id) and a different signal payload.

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

## What this does not show

The tests come from the `QAbstractItemModel` contract, not from anyone's production code. They show that these
classes keep the contract on the changes listed; they do not show that a given application has no ghost rows.
Selection behaviour is checked for `QItemSelectionModel` and `QPersistentModelIndex`; proxy models and views that
cache row data are not covered.
