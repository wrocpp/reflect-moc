# Binding B, addendum 1 (changes made during a notification): report

## Header

- Build: GCC 16.2.0 and Qt 6.10.3 in the image `reflect-moc/gcc16-qt610`, Ninja, `-j4`, AUTOMOC off, flags
  `-std=c++26 -freflection -Wall -Wextra -Wpedantic`, Debug (`-g`) and Release (`-O3 -DNDEBUG`).
- ctest: 17 of 17 passed in Debug and in Release. These are the 17 entries that were there (5 test executables and the 12
  compile-fail entries). The twelve acceptance tests are functions inside `tst_container` and `tst_list_model`, so the ctest count
  does not change. QtTest totals (functions plus `initTestCase` and `cleanupTestCase`): `tst_container` 43, `tst_list_model` 21,
  `tst_traits` 5, `tst_matrix` 26, `tst_selection` 29; identical in Debug and Release.
- Warnings: 0. Counted as `warning:` in the log of a `--clean-first` build of every target, and in the ctest log (which holds the
  6 compile-fail twin builds and the 6 deliberate break builds), for both configurations.
- Also run, beyond the gate: the two changed executables, every test function alone, under `-fsanitize=address,undefined`
  (Debug): all pass, no report. That is the only evidence that destroying the container inside a receiver of a twin is survived
  (see Other 3); a plain build cannot show it.
- Test functions added: 10 in `tst_container` and 3 in `tst_list_model` for AT-10.1 to AT-10.12 (AT-10.5 is two functions, the
  `remove` half and the `take` half, because the `take` half crashes the old library); 6 `capability_` and 4 `reading_` functions
  beyond them (listed under Other). Before/after per test: `notes/before.txt` and `notes/after-each-*.txt`.
- Not tested: mutation testing (not asked for, not run); the queue on a `std::vector` (the instantiations compile, since the existing `capability_stdVectorAndStdMapAreSupported` uses
  the same member functions, but no nested change runs on one); a nested change on a `std::map` is exercised only by the move-only
  test; threads (G6); an exception thrown by a `Value` constructor
  or destructor (G7); destroying the container from a receiver of an "about" (undefined, as before); the matrix (A.7, left as it
  was, no test); a queued call dropped by an exception whose captured `Value` has a destructor that calls the container.

## Ambiguities and the reading chosen

1. **N1, which calls open a scope.** The text says every mutating function. I read it as: a call that goes on to emit. A call
   that is rejected by its check (absent key, bad index, clearing an empty container) opens no scope. This is unobservable, since
   no receiver runs for it. `take` and `takeMany` open one when they do act.
2. **N1, "scopes nest".** With N2 in force nesting can only happen in two places: a mutator that calls another mutator (here
   `append` calls `insert`, and it makes its queueing check first, so it opens no scope of its own), and a drained call, whose
   scope joins the gesture that is running. Nothing else nests.
3. **A.4 and N1, what `isNotifying()` says in the gaps.** True inside every receiver, of the "about" and of the twin (the scope
   closes when the call returns, after the twin; AT-10.2 checks both). False between two queued calls of a drain, and false at
   the start of a queued call's turn, which must be so or the call would be queued again forever. True inside the receivers of a
   drained call.
4. **N2 and N5, order of checks.** In every mutator the "is it notifying" test comes first, then the validity check. So
   `remove` of an absent key, made during a notification, is queued, returns `true`, and at its turn finds nothing and emits
   nothing. `clear` of an empty container and an empty batch are queued too and do nothing at their turn.
5. **N8, "requested".** Read as the raw size of the argument: `entries.size()` for `insertMany`, `keys.size()` for `removeMany`,
   so a duplicate, a key that is present for `insertMany` and a key that is absent for `removeMany` all count. An empty argument
   returns 0. `clear` and `resetWith` return `void` and have nothing to report.
6. **N8, `true` for a call that cannot succeed.** Read literally: a queued `insert(-1, v)` or `replace` of an absent key answers
   `true` and is rejected at its turn (the test `reading_aQueuedCallAnswersQueuedEvenWhenItWillBeRejectedAtItsTurn` pins it).
7. **N9, refusal order.** The refusal comes before any validity check, so a refused `take` of an absent key and of a present key
   look the same (`std::nullopt`). `takeMany` returns a default `C`.
8. **N10, what is copied.** `replace` and `insert` take their value by value, so the queue gets a copy for an lvalue argument and
   the moved value for an rvalue. The key, the roles and the key list are always copied: they may be references into the frame of
   the outer call (a receiver's `const Key &` parameter is one). `editValue` copies or moves the callable (decayed). A callable
   that cannot be copied and is passed as an lvalue does not compile (a `static_assert` with a message); passing it with
   `std::move` works. The other reading, queueing a reference to the lvalue, would be a dangling reference waiting to happen.
   The sentence that `f` must own what it captures is in the doc comment of `editValue`.
9. **N11, `append`.** A queued `append` is queued as an `append`, so it means the end of the container at its turn, not
   `insert` at the size the container had when it was queued.
10. **N3 and N4, exceptions.** The addendum is silent, G7 says the library does not handle them. Chosen (R2): a receiver that
    throws ends the gesture; the container is idle again (`isNotifying()` is false); the calls still queued are dropped, not
    applied by a later gesture; nothing is emitted while unwinding. If the throw happens in a drained call, the exception leaves
    the outer call that started the drain. Pinned by two `reading_` tests.
11. **N3, destruction inside a receiver.** Also silent. Chosen: destroying the container inside a receiver of a twin (of the
    outer call or of a drained call) is survived: the gesture notices, stops draining and touches nothing of the container.
    Destroying it inside an "about" stays undefined, because the mutation still has to follow; the class comment says so.
12. **A.5, "look the entry up again".** Already the case in this tree: `take` and `editValue` call `valueOf(key)` after the
    "about" and never hold a pointer across it. The failure of the old code in AT-10.5 (`take`) was not a stale pointer but a null
    one: the nested `take` erased the entry, the outer `take` looked it up again and dereferenced nothing.
13. **A.5, the list model.** No change was needed: the model's cycles already run inside the scope, because the scope opens before
    the "about" and the batch removal begins on the "about" while the batch insertion runs on the twin. A test pins it
    (`capability_everyCycleOfTheModelRunsInsideTheContainersScope`).
14. **Section 12 conventions and connection order.** Qt calls receivers in connection order and the recorder connects in its
    constructor. A nested call made in a receiver of a twin, with the recorder connected first, gives the same log queued or
    immediate. So where an AT says that something runs "after the twin", I made it distinguishable: AT-10.1, AT-10.3, AT-10.6
    and AT-10.7 connect the reacting receiver before the recorder; AT-10.8 and AT-10.9 make the nested call from the "about" of
    the outer call. AT-10.2, AT-10.4, AT-10.5 and AT-10.10 to AT-10.12 need neither (no recorder, or a model that shows the
    difference).
15. **AT-10.5, receiver guards.** A receiver of `entryAboutToBeRemoved(1)` that calls `remove(1)` or `take(1)` would, on the old
    library, recurse until the stack ends; the tests act "the first time" only (the text uses the same words for AT-10.10 and
    AT-10.11). With the queue the guard changes nothing.
16. **AT-10.6, "once in the recorder".** The batch payload is logged twice (the "about" and the twin). I read it as: no
    per-entry notification for key 5; the test compares the whole log.
17. **AT-10.10, the first run.** Removal cycles go bottom up, so the first `rowsRemoved` is the cycle of key 8; the view's
    `remove(3)` is made there and arrives after the cycle of key 2.
18. **N6.** Needs no code: the state is per container. A test pins that a call on another container is applied at once when
    that container is idle, and queued when it is notifying (`capability_aCallOnAnotherContainerFollowsThatContainersOwnState`).
19. **A.7.** `obs::Matrix` and `MatrixTableModel` are untouched.

## Possible design mistakes

1. **Some ATs cannot tell the queue from the old behaviour as worded.** AT-10.3, AT-10.8 and AT-10.9 say that the nested calls
   come "after" the outer call's notifications, but with the recorder connected before the reacting receiver and the nested call
   made in a twin, immediate application writes the same log. I did not run that variant against the old library (I only
   reasoned from the log order). My versions fail on the old library (`notes/before.txt`). The spec's section 12 should state the
   connection order, or the ATs should trigger from the "about".
2. **`true` from a call that can never succeed (N8).** `insert(-1, v)` answers `true` inside a notification and `false` outside.
   The `bool` stops meaning anything there. A cheap structural check at queue time (negative index) would not contradict N5, since
   the state-dependent checks would still wait for the turn. As specified, a caller has to ask `isNotifying()` first to know
   whether to trust the answer.
3. **A refused `take` looks like an absent key (N9).** Both are `std::nullopt`. `to.insertMany(from.takeMany(keys))` made inside
   a receiver of `from` silently moves nothing (`takeMany` returns an empty `C`, `insertMany` of nothing emits nothing); the
   entries stay in `from`. No failing assertion, no signal. A `Q_ASSERT` or a distinct result would have made it visible.
4. **The hazard of N10 is silent and survives review.** A lambda with `[&]` compiles and works while the container is idle and
   fails only inside a notification, where it reads a dead frame. The library cannot detect it. The cost of the compile error for
   a non-copyable lvalue callable (Ambiguity 8) is that `editValue(k, namedMoveOnlyLambda)` stopped compiling for code that
   compiled before.
5. **An endless chain is now a hang, not a stack overflow (N4).** The queue pops each call before it runs, so memory stays flat
   while a receiver keeps queueing the next call. There is no bound and no diagnostic. I did not run an endless chain (only the
   chain of 100 of AT-10.4).
6. **The queue is per container, so there is no order across containers (N6).** A's receiver changes B at once, B's receiver changes
   A and is queued. Two cooperating containers get an order that depends on which one was notifying first. This is stated in N6,
   but it is the case where a user will be surprised.
7. **The matrix keeps the bug (A.7).** A nested change on an `obs::Matrix` is still applied at once, in the middle of its own pair,
   so the library now has one container that honours the about/done contract against reentrancy and one that does not. A.7
   explains why (no failing test). I have not written one.
8. **A.5's first bullet does not describe this tree.** There was no pointer taken before the "about" to go stale (Ambiguity 12);
   the real hazard was a second lookup that finds nothing. The SHOULD is already satisfied and says nothing about the case that
   crashed.
9. **A queued call is applied on the stack of the outermost call.** A receiver that is slow or that blocks (a modal dialog) delays
   the whole drain, and every queued change lands only when it returns. That is inherent to "no timer, no event loop" and
   deliberate, but the addendum does not say it.

## Other

**How a queued call stores its arguments.** Each queued call is a closure that captures `this` and copies or moves what the call
was given (the key, the roles, the moved value, the moved entries, the key list, the decayed callable), stored as a
`std::move_only_function<void()>`. I used it, behind `__cpp_lib_move_only_function` (a missing feature is an `#error`, not a
silent fallback), because a value or a callable may be move-only (a `Value` that owns a resource, a lambda that captures a
`std::unique_ptr`) and `std::function` needs a copyable callable. The test
`capability_aQueuedCallCanOwnAMoveOnlyValueAndAMoveOnlyCallable` queues both. GCC 16.2's library provides it. The calls sit in a
`std::deque`, allocated when the first call is queued, so an idle container and a call that queues nothing allocate nothing for
this.

**Where the queue lives, and the drain flag.** The queue is not a member of the container. It belongs to a `Gesture` object on
the stack of the outermost mutating call (the scope that finds the container idle owns it); the container holds one pointer to
it (`Link`, 8 bytes). So there is no drain flag to leave set. The drain is the destructor of the owning scope, and the "drain
already running" state is the fact that a drained call's scope finds a gesture running and joins it instead of owning one.

1. A receiver throws in the outer call: the scope unwinds (the destructor sees an exception in flight, so it does not drain),
   the pointer is cleared, the queue is destroyed with the stack object. `isNotifying()` is false afterwards.
2. A receiver throws in a drained call: the owning destructor lets the exception through (it is declared `noexcept(false)`, and
   only drains when no exception is already unwinding), the pointer is cleared, the rest of the queue is destroyed.
3. A receiver destroys the container in a twin: the destructor of the `Link` marks the running gesture, the owning scope returns
   without touching the container, and the drain loop (a static function that only reads the gesture) stops. A queued call that
   was waiting is destroyed unapplied. Clean under ASan and UBSan. A design with the queue and a depth counter inside the container
   would write into freed memory at the close of the scope; the old code touched nothing after the twin, so that would have been a
   regression for code that deletes its sender from a slot.

**What the list model does between the begin and end calls of a batch.** It was not changed. Removal: the cycles run in the
receiver of `entriesAboutToBeRemoved`. The row positions of all runs were computed from the model's own rows before the first
cycle; each cycle is `beginRemoveRows`, erase the run from the model's row list, `endRemoveRows`, from the bottom up. During all
of it the container still holds every entry and its `size()` is the old one, `isNotifying()` is true, `data()` of a row still in
the model reads the real value, and `rowCount()` is the model's own count, which falls by one run per cycle. The container erases
the entries after the last cycle. Insertion: the cycles run in the receiver of `entriesAdded`, so the container already holds
every new entry; the final positions were computed first; each cycle is `beginInsertRows`, insert the run into the model's row
list, `endInsertRows`, from the top down. Between cycles the model has only the runs done so far, so `rowOf(key)` of a key from
a later run is -1 although the container has it. In both directions a view that changes the container in these receivers is now
queued and arrives after the last cycle as an ordinary change (AT-10.10, AT-10.11); before, it landed between two cycles and made
the positions of the next one stale (the old library removed the wrong row and inserted out of order).

**Where each point is.** N1 `Scope` (private nested class), opened before the "about" in every mutator that emits. N2 the
`isNotifying()` test at the top of each mutator and `defer`. N3 the destructor of the owning `Scope` and `drain`. N4 a drained
call's scope joins the running gesture. N5 the validity checks sit after the queueing test, so they run at the turn. N6 per
container state. N7 the old path. N8 `defer` returns `true`, batches return the raw size. N9 `take`, `takeMany`. N10 `editValue`
and its doc comment. N11 the queued call is the public function itself, called again at its turn. A.4 `isNotifying()`, which is
`[[nodiscard]] bool ... const noexcept` as in the synopsis. No other public name was added (R1); the class comment above
`Container` states the contract for callers.

**Changes to existing tests.** None. No existing test makes a nested call from a receiver (they only read), none asserted the
immediate behaviour, and the 37 existing functions of the two executables passed before the change and pass after it.

**Qt API.** Library code uses nothing new from Qt. Standard additions: `<deque>`, `<memory>`, `<functional>`, `<exception>`
(`std::uncaught_exceptions`), `<version>`. The tests use `QHash::keys()` and `QStringList::filter()`, which are not in the list of
section 16 (only the classes are); `QPointer` and `QObject::destroyed` were not used.

**What reflect-moc made easy or hard here.** Mostly nothing, which is the finding: the queue is plain C++ inside the class
template and needed no annotation. Easy: the signals are data members that are called like functions, so no emission site
changed; the typed `const Key &` arrives in the receiver as itself, and the queued closures copy it with no boxing; adding a
private nested class, a private data member and a member function template to a class with `RQT_OBJECT` compiled at the first
try; the signal goes through `QMetaObject::activate`, so Qt's own handling of a sender deleted in a slot and of exceptions
passing through applies unchanged (both used above, and the exception tests pass through it). Cost to remember: every signal is
a data member holding its owner pointer, 14 of them in each `Container`, and the one pointer I added is the same kind of cost.
The rule that the anchor comes first and no `rqt` class may sit between it and a signal did not bite because the new member is
last, but it is a rule a later change to this class has to know.

**Tests beyond the twelve** (each derived from a point of the addendum that has no AT of its own, or the pin of a reading above).
In `tst_container`: `capability_aQueuedCallIsKeptAndNotAppliedWhereItIsMade` (N2, N8),
`capability_aCallOnAnotherContainerFollowsThatContainersOwnState` (N6),
`capability_callsQueuedByAQueuedCallGoToTheBackOfTheQueue` (N4),
`capability_aQueuedSequentialIndexMeansTheStateAtItsTurn` (N11, `append` included),
`capability_aQueuedCallCanOwnAMoveOnlyValueAndAMoveOnlyCallable` (N10), and the readings
`reading_aQueuedCallAnswersQueuedEvenWhenItWillBeRejectedAtItsTurn` (5, 6 above),
`reading_aReceiverThatThrowsLeavesTheContainerUsableAndDropsTheQueue` and
`reading_aReceiverOfADrainedCallThatThrowsDropsTheRestOfTheQueue` (10),
`reading_aContainerDestroyedByAReceiverOfATwinEndsTheGestureQuietly` (11; a smoke test, it only means something under a
sanitizer). In `tst_list_model`: `capability_everyCycleOfTheModelRunsInsideTheContainersScope` (A.5, 13).

**Process.** The before-run was done twice: the first version of the destroy test deleted the container twice on the old library
(its receiver ran again for the nested key), which made it crash for a reason that had nothing to do with the library; I fixed the
test and repeated the run, so `notes/before.txt` describes the final tests, except the two that are marked there as written
later. The stub `isNotifying()` is gone; `container.h` has only the real one.

**Helper files.** `notes/run-each.sh` (every test function alone, with a timeout), `notes/gate-before.sh`, `notes/gate-after.sh`
and `notes/gate-asan.sh` (host side, they take the shared lock and run the build image), and the raw results.
