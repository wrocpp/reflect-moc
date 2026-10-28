# The matrix and a change made during its own notification

Addendum 1 of the spec ([gist revision `a2fc33b`](https://gist.github.com/96Ven96/100c0ec4917dced4d6c8ac9a4cc06b7e/a2fc33b0d04280c896e72a69a90e234b94ba7c44))
queues such a change for the containers and leaves the matrix out: its author's reason is that no test shows the failure.
This note and the test beside it are that test. The test is `tests/limit_matrix_nested_change.cpp` in each binding,
a ctest entry wherever CMake's `UNIX` is true (it forks, so it is not built under MSVC). It was run on Linux in the CI image.

## What the test does

For each of 16 changes it runs two children and reads how each ended.

- **The change:** add or remove, one entry or a batch, rows or columns. Each is announced by an "about" signal and a "done" signal.
- **The nested change:** a receiver of one of those signals changes the same matrix once. After a row is added, for example, the receiver adds another row.
- **The receiver's place:** connected after the table model, or before it. That decides whether the model has heard the signal when the receiver runs.
- **The observers:** the model has a `QAbstractItemModelTester` in fatal mode, and a mirror that follows the model from its signals alone, as a view does.
- **The control:** the same two changes, one after the other, with no receiver. All 32 controls end consistent.

## What happened

The result is the same in binding A and binding B, and in a Debug and a Release build.

| the receiver runs | the nested change | the 8 cases of each row end |
|---|---|---|
| in an "about" signal, connected after the model | any | crashed: the tester sees a change already in flight |
| in a "done" signal, connected before the model | any | crashed: the same check |
| in a "done" signal, connected after the model | any | consistent |
| in an "about" signal, connected before the model | an add | out of step (4 cases) |
| in an "about" signal, connected before the model | a removal of the first entry | crashed in the matrix (4 cases) |

The last two rows are the ones that do not come from the model.

- **An add that moved the index.** The outer add takes the index it will announce before the "about" signal. A receiver that adds an entry moves it.
  The model and the matrix agree at the end, but the "done" signal names a position that is wrong, so a view that follows the signals reads the wrong key.
- **A removal that moved the index.** The outer removal takes its index before the "about" signal and erases at that index afterwards.
  After a receiver has removed an earlier entry, the index is past the end: `QList::remove` asserts in Debug and the process dies on a signal in Release.

These cases are the matrix's version of the pointer taken before the "about" signal, which the addendum describes for the containers.

## Not shown

- The renames and the cell changes. A nested `setValue` from a `cellChanged` receiver is a different case and is not in this table.
- A receiver in another thread.
- A fix. Nothing in the matrix was changed. The pinned classes pass while the failure reproduces, so the first change that queues a nested
  call makes the test fail on purpose, and the pin is then changed with it.
