# Qt edge cases of the observable models

One row per case that Qt documents, that `QAbstractItemModelTester` checks, that the Qt bug tracker records or that
Qt's own tests cover, for the models in `examples/qt/observable_models`. Each row names the test that covers it and a
verdict. `scripts/check_edge_catalogue.py` (run by ctest as `capability_edge_catalogue_consistent`) fails on a blank
cell, an unknown verdict, a missing test file or a duplicate id.

| file | category |
|---|---|
| [protocol.md](protocol.md) | model protocol: moves, layout changes, parents, indexes, headers, flags, roles |
| [reentrancy-lifetime.md](reentrancy-lifetime.md) | a receiver that mutates or deletes, destruction order, two models on one container |
| [proxies-selection.md](proxies-selection.md) | sort and filter proxies, the other proxy models, the selection model |
| [views-qml.md](views-qml.md) | widget views and QML views |
| [data-types.md](data-types.md) | keys and values: NaN, null strings, move-only, sizes, implicit sharing, metatypes |
| [threads.md](threads.md) | threads: deleteLater, moveToThread, queued delivery |

Verdicts: **works** (the model keeps the contract), **refused** (it says no and changes nothing), **pinned** (a known
gap; a `limit_edge_` test shows it and is flipped by the fix), **fixed** (a gap that was pinned and is closed).
Sources: `doc:<page>`, `tester:<check>`, `QTBUG-<n>`, `qttest:<file>`, or `own`.

Row format: `| id | case | source | test | verdict |`, with the test as `<file>` or `<file>::<function>`.
