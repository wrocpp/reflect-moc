# Binding A, addendum 1 (changes made during a notification): report

Binding A (classic moc, keys as `QVariant`), C++20, GCC 16.2, Qt 6.10.3 in the `reflect-moc/gcc16-qt610` image.
Flags: `-Wall -Wextra -Wpedantic`, as the tree's own CMakeLists sets them.

| Gate | Result |
|---|---|
| Debug, full rebuild (`--clean-first`), `ctest` | 17 of 17 pass, 0 compiler warnings |
| Release (`build/rel`), full build, `ctest` | 17 of 17 pass, 0 compiler warnings |
| `tst_container` (Debug and Release) | 31 test functions pass (16 existing, 15 new) |
| `tst_list_model` (Debug and Release) | 17 test functions pass (14 existing, 3 new) |
| ASan + UBSan build of the two executables | pass, no report (leak detection switched off) |

The twelve acceptance tests are test functions inside the two existing executables, so `ctest` stays at 17 entries.
Per-test details of the "before" run are in `notes/before.txt`. "After" was measured by running each function on its own
(`./tst_container <function>`) and the two executables whole, in Debug; `extra_n6_...` has no isolated after-run, it is
verified in the whole run (33 of 33 and 19 of 19).

| Test | Before (unchanged library plus an `isNotifying()` stub) | After |
|---|---|---|
| AT-10.1 | fail: nested `remove(2)` applied inside the about of 1 | pass |
| AT-10.2 | fail: `isNotifying()` is false inside the receiver (the stub) | pass |
| AT-10.3 | stated checks hold; fails only on the added N2 check (5 already present right after the queued `insert(5)`) | pass |
| AT-10.4 | fail: nesting depth of `entryAdded` is 100 | pass |
| AT-10.5 | fail: `entryRemoved(1)` emitted twice | pass |
| AT-10.6 | fail: a per-entry pair for 5 inside the batch | pass |
| AT-10.7 | fail: `takeMany` took 1 and 2 at once | pass |
| AT-10.8 | stated checks hold; fails only on the added N2 check (`f` already ran) | pass |
| AT-10.9 | stated checks hold; fails only on the added N2 check (`size()` already 0) | pass |
| AT-10.10 | fail: model rows differ at index 2 | pass |
| AT-10.11 | fail: model rows differ at index 4 | pass |
| AT-10.12 | crash: SIGSEGV after five `QAbstractItemModelTester` failures | pass |

The model tests print one `QWARN` "Trying to construct an instance of an invalid type" when the tester attaches; section 12
names it as a known warning of Qt's tester, not a failure. "The tester reports nothing" means no tester failure.

Not tested: MSVC 19.44 (only read for portability, see Other 3), clang, any other Qt version, threads, the matrix and the
selection (untouched, no new tests; their existing tests pass), a receiver that throws from inside a DRAINED call (same code path as the tested outer case), a
container destroyed inside one of its own receivers (undefined, see 12), mutation testing, leak detection.

Changed: `include/obs/container.h`, `tests/tst_container.cpp`, `tests/tst_list_model.cpp`. Added: `notes/before.txt`,
this report. `include/obs/container_list_model.h` needed no change. Existing tests: nothing removed or edited (the diff against
the delivered tree has no removed line in either test file), because no existing test makes a nested change.

## Ambiguities and the reading chosen

1. **N1, where the scope opens.** The addendum says a mutating function opens the scope before its "about". A call that
   fails validation (G3) emits nothing, so I open the scope only after validation succeeded. Nothing can observe the
   difference: no receiver runs for a rejected call.
2. **N1, "scopes nest".** The counter exists, but with N2 and N9 a second scope on the SAME container can never open while one
   is open (every mutator is queued and both takes are refused). The depth is 0 or 1 in practice. I kept a counter because the
   addendum asks for nesting; a `bool` would behave the same.
3. **N2/N7, which calls are "during the notification".** `isNotifying()` is read when the call is entered. A call made by `f`
   inside `editValue`, or by a value's constructor, copy or destructor that runs inside the mutation, is inside the scope, so it
   is queued. A call made by the destructor of an already applied queued call runs at depth 0 between two drained calls; see 8.
4. **N3, where the drain runs.** The addendum describes "when the outermost scope closes". I do not drain from the scope's
   destructor (a destructor must not run receivers that may throw). A public mutator opens no scope itself: when the container
   is notifying it queues, otherwise (depth 0) it calls a private `apply*` function, which opens, uses and closes a scope, and
   then calls `drainQueue()`. So a drain always belongs to an outermost gesture. A drained call goes to `apply*` directly, so it
   never starts a drain of its own: N3's "does not start another one" holds by construction.
5. **N3, the drain flag.** `drainQueue()` still sets a `m_draining` flag (restored by a small RAII object) and returns at once
   if it is set. By construction it is redundant; it only matters in the case of 8, and no test can reach it.
6. **N4, order of nested reactions.** The queue is one FIFO per container, so reactions are breadth-first: if two receivers of
   the same twin each queue a call, both run before any call queued by the first call's own receivers. Each drained call is its
   own gesture; its receivers see `isNotifying() == true` (the scope of that call) and `false` again between calls.
7. **N5 and N8, what "validated at its turn" means for the return value.** The outer caller learns nothing about the queued
   call: `remove(5)` queued after `insert(5)` returns `true` while 5 is absent (AT-10.3 itself shows it). `void` mutators
   (`clear`, `resetWith`) return nothing either way.
8. **N3, the drain flag when a receiver throws, and the exotic re-entry.**
   - A receiver that throws (not covered by the addendum, G7 says the library does not handle it). By R2 (change nothing, emit
     nothing) I chose: the exception propagates; the `Scope` destructor restores the counter, and when it was the outermost
     scope and the stack is unwinding because of an exception raised after the scope opened (`std::uncaught_exceptions()` stored
     at construction), it DROPS what the receivers had queued. The RAII object resets `m_draining`. So the container is never
     stuck "notifying" and a later gesture never applies the leftovers of an abandoned one. Pinned by
     `pin_r2_aThrowingReceiverDropsTheQueueAndLeavesTheContainerUsable`. The code path for a throw inside a drained call is the
     same; I did not write a test for it.
   - Exotic: the destructor of a queued call (it owns copies of the arguments) can run user code at depth 0 between two drained
     calls, for example a `Value` whose destructor calls `insert`. That call is not notifying, so it is applied at once (N7) and
     jumps the queue; the flag stops it from starting a nested drain. No test reaches this.
9. **N8, the counts.** A queued `insertMany` returns `entries.size()` (all entries passed, including keys already present), a
   queued `removeMany` returns `keys.size()` (duplicates and absent keys included), both read before any move and cast to
   `qsizetype`. Pinned by `extra_n8_...`.
10. **N9, order of checks.** `take` and `takeMany` test `isNotifying()` before any validation, so the refusal does not depend on
    whether the key is present. A refused call does not drain (it is inside a gesture by definition).
11. **N10, how a queued call stores its arguments in C++20.** A `std::function` needs a copyable callable, and a queued call must
    own move-only arguments (`std::move_only_function` is C++23). The queue is a `std::deque<std::unique_ptr<QueuedCall>>` where
    `QueuedCall` is a private abstract class with one virtual `apply(Container &)`, and `QueuedCallOf<F>` holds the closure.
    The closure is a `mutable` lambda that captures the arguments BY VALUE (`key` copied, `value`/`content`/`entries`
    init-captured by move) and calls the private `apply*` function. Nothing needs to be copyable; the move-only case is pinned by
    `pin_n10_aQueuedCallTakesAMoveOnlyValueAndAMoveOnlyCallable` (a `std::vector<std::unique_ptr<int>>` container and a
    move-only lambda). `removeMany` copies its `QList<Key>` (an ownership boundary: the queue outlives the call). "Queues `f`
    itself" I read as: a decayed copy of the callable if the caller passed an lvalue, the moved callable if an rvalue, never a
    reference (a caller who wants one passes `std::ref`). The doc comment of `editValue` carries the sentence that `f` must own
    what it captures. The immediate path (not notifying) forwards `f` without capturing it, so it neither copies it nor
    requires it to be copyable.
12. **R2 and G7 (the addendum is silent): container destroyed inside a receiver.** Undefined, as it was before the addendum: after the `emit` returns, the apply
    function goes on to read and write `m_data`, and now also the scope counter and the flag of a dead object. I did not add a
    mitigation: a guard would need `QPointer`, which section 16 does not list. A container destroyed with calls still in its
    queue (only possible after an exception, where the queue was already dropped) would drop them without applying or emitting.
13. **N11 and `append`.** N11 names `insert`, `remove` and `replace`, not `append`. I read `append(v)` as `insert(size(), v)` with
    `size()` taken WHEN THE CALL IS APPLIED, so a queued append still goes to the end. Pinned by
    `pin_n11_aQueuedAppendGoesToTheEndAtItsTurn` (an `insert(0, ...)` queued before it grows the container). N11 itself is
    checked by `extra_n11_...`.
14. **N6.** The queue is a member of the container, so a call on another container consults only that container's
    `isNotifying()`. `extra_n6_...` shows both directions in one gesture.
15. **A.5, "look the entry up again after the about".** The delivered `take`, `replace` and `editValue` already reach the value
    through `valueRef(key)` after the "about" emission, so the lookup is repeated; I kept that and wrote it into a comment. With
    the queue nothing can change the storage between the lookup and the use.
16. **A.5, the wired list model.** The model needed no change. Its batch cycles run inside its receivers of
    `entriesAboutToBeRemoved` and `entriesAdded`, which are inside the container's scope, so a view that changes the container
    from `rowsRemoved` or `rowsInserted` is queued. Between the `begin*` and `end*` of one cycle the model changes only its own row
    list: it erases (or inserts) the run's keys after `begin*` and before `end*`, never reads or writes the container (only
    `data()` reads it, through `find(key)`), and `rowCount()` is the size of that list. Between the cycles of one batch the model
    shows a partly translated list while the container already holds the final content (insertion) or all the old content
    (removal); no container change can land there any more, so the row indices computed before the first cycle stay right.
    The deliberate changes of the tree (removal at the "about", binary search over sorted row keys) are unchanged.
17. **AT-10.10 and AT-10.11, "the first time it runs".** Read as: the first invocation of the view's receiver, which is the
    twin of the first cycle of the batch; a `first` flag in the test. The receivers of AT-10.1, 10.5, 10.6 and 10.7 also act on
    their first call only (see Possible design mistakes 7).
18. **N10 and AT-10.8, the roles of a queued `editValue`.** The `roles` list is copied into the queue and passed unchanged at the turn (AT-10.8).

## Possible design mistakes

1. **The silent lifetime trap of `editValue` (N10).** The same call is safe outside a notification and a use-after-return inside
   one: `int local = 3; c.editValue(2, [&local](QString & v) { v += QString::number(local); });` works everywhere except when
   the caller itself runs inside a receiver of `c`, where `local` is dead when `f` runs. Nothing can check it (the closure type
   does not say what it captures), so a correct function becomes wrong when it is called from a new place. I did not run this
   case. The only defence is the sentence in the doc comment and `isNotifying()`. A signature that makes ownership visible (a
   sink parameter, or a separate queued variant of `editValue`) is not possible under R1.
2. **A sink that constrains callers.** Because `f` may have to be queued, `editValue` now static-asserts that
   `std::decay_t<F>` is constructible from `F`. An lvalue callable that is not copyable compiled before and does not now. I
   judged it better than a hard error deep inside a lambda, but it is a source-incompatible change that N10 implies.
3. **`true` from a queued call carries no information (N8).** `if (!c.remove(k)) handleMissing();` never takes the branch inside a
   notification. AT-10.3 shows the extreme: `remove(5)` returns `true` for a key that is absent when it returns. Callers have to
   test `isNotifying()` first if they care, which is easy to forget.
4. **Refusal looks like absence (N9).** Inside a receiver, `take(k)` of a PRESENT key returns `std::nullopt`, the same as for an
   absent key, and `takeMany` returns an empty `C`. For `to.insertMany(from.takeMany(keys))` run in a receiver of `from` nothing
   moves and nothing says so (`insertMany` of an empty `C` returns 0, which is also a legal outcome). No data is lost, the
   entries stay in `from`, but a caller cannot tell "refused" from "nothing to take" without `isNotifying()`. (Worked out by
   reasoning, not run.)
5. **Livelock instead of a stack overflow (N3, N4).** A receiver that always queues a follow-up (AT-10.4 without the `k < 100`
   test) used to end in a stack overflow and now never returns, because the outer call drains forever. The queue has no limit.
   I did not run it. A bound on the number of calls one gesture may drain, or a way to see the queue length, would turn it into
   a diagnosable failure; the addendum does not offer either.
6. **Queued positions can silently fall off (N11).** Three queued `remove(2)` on a three-element list all return `true`; the first
   removes the last element and the other two find nothing and are dropped without a sign. The addendum states the ambiguity and
   leaves it. Together with 3 the caller sees only successes. (Worked out by reasoning, not run.)
7. **Three of the twelve acceptance tests pass against the unchanged library.** AT-10.3, AT-10.8 and AT-10.9 check only the
   recorder log. The recorder is connected first and the nested call is made by the last receiver, so immediate application
   produces the same log as the queue (see `notes/before.txt`). I added a check of the container's state INSIDE the receiver right
   after the queued call (the call must not have been applied yet, N2); with it all three fail before and pass after. Also,
   AT-10.1, 10.5, 10.6 and 10.7 as written (a receiver that acts on every call) recurse until the stack ends against an unchanged
   library (10.5 does, by inspection; I did not run the unguarded form); I made those receivers act on their first call only, so the result is a failed check instead
   of a crash. Whether to harden the text of the acceptance tests is the lead's call.
8. **The matrix has no queue (A.7).** The two containers now behave differently when a receiver changes them. A table model on
   a matrix has the problem the addendum fixes for the list model. I did not touch the matrix and did not test it.

## Other

1. **Test names.** Twelve acceptance tests: `at_10_1_...` to `at_10_9_...` in `tst_container.cpp`, `at_10_10_...` to
   `at_10_12_...` in `tst_list_model.cpp`. Extra tests derived from the addendum: `extra_n6_...`, `extra_n8_...`, `extra_n11_...`.
   Pins of a reading where the addendum is silent: `pin_n11_...` (append), `pin_r2_...` (throwing receiver), `pin_n10_...`
   (move-only arguments).
2. **Constants and helpers in the tests.** The chain length of AT-10.4 is the named constant `kChainEnd`. `numbered`, `sortedKeys`
   (both files) and `modelKeys`, `rowsReadTheirOwnValue` (list model) are file-local helpers.
3. **Portability.** No GCC extension and no library feature after C++20 (`std::uncaught_exceptions` is C++17,
   `std::is_constructible_v` and `std::exchange` older). No init-capture reuses the name of the enclosing parameter, to stay clear
   of MSVC shadow warnings. Not compiled with MSVC here.
4. **Qt API.** The library change uses no Qt API beyond what the tree already used. The new tests use `QVariant::toInt()`,
   `QVariant::toString()`, `QHash::constBegin()/constEnd()/key()` and `QList::contains()`, which the delivered tests and headers
   already use, and the standard `<stdexcept>`, `<memory>`, `<utility>`. Nothing outside section 16 plus the tree.
5. **Shape of the change.** `container.h` grew from 299 to 533 lines: every mutator is now a short public entry (queue, or
   apply and drain) in front of a private `apply*` function that holds the old body with a `Scope` object added. The emissions
   are written out in each `apply*` function in the order of the specification; there is no generic wrapper around them.
6. **Public API.** The only new public name is `isNotifying()`, `[[nodiscard]] bool isNotifying() const noexcept`. The rest
   (`QueuedCall`, `QueuedCallOf`, `Scope`, `Draining`, `enqueue`, `drainQueue`, `submitInsert`, the `apply*` functions, three
   data members) is private.
7. **Before and after.** Before: AT-10.1, 10.2, 10.4 to 10.7, 10.10, 10.11 fail on their own stated checks, AT-10.12 crashes
   (SIGSEGV after five tester failures), AT-10.3, 10.8 and 10.9 fail only on the added N2 check. After: all pass, in Debug, in
   Release and under ASan and UBSan.
