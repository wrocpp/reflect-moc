# observable_container

An observable container whose signals are typed by its template argument, built twice from the same harness:

| tree | what it is | built with |
|---|---|---|
| `original/` | the shape moc forces: a non-template `ContainerSignals` with `Q_OBJECT` and `QVariant` keys, a template that inherits it, and a store with `Q_OBJECT` again | moc |
| `ported/` | one class template, `ObservableContainer<C>`, with `rqt::signal<void(const Key&)>` | reflect-moc, no moc |

`ported/` is written by hand. moc cannot read a class template, so `tools/rqt-migrate`, which takes its input from
`moc --output-json`, never sees `ObservableContainer`. Its fold step reads the signals-only base through moc and moves
the signals into the template (see `tools/README.md`); on a tree where the model unboxes in every lambda it ports the
shape end to end (`examples/qt/observable_models_qvariant`, `tools/tests/fold_e2e.sh`). On `original/` it folds the
base and lists eight sites it will not guess: the template boxes the key in a local before it emits, and the model is
not a template, so `&ContainerSignals::entryAdded` does not say which instantiation it connects to.

```sh
docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 examples/qt/observable_container/check.sh original
docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 examples/qt/observable_container/check.sh ported
```

`harness.cpp` inserts into a `QList`-backed model under `QAbstractItemModelTester` (fatal mode) and into a
`QHash`-backed store, and prints what a view sees. `expected_output.txt` is the moc build's output, and the
ported build must match it, with `-Wall -Wextra` and no warnings.

Size: the moc tree is 107 lines in 7 files, the ported tree 103 lines in 6. The saving is the signals-only base
and the `QVariant` boxing and unboxing, not a number of lines.

On Qt 6.10.3 `QAbstractItemModelTester` prints `Trying to construct an instance of an invalid type, type id: 4097` to
stderr in both builds, so `check.sh` compares stdout only.
