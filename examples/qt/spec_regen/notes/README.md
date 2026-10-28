# The agents' notes

The two reports the regenerating agents wrote, kept as they wrote them, for the author of the spec. They were written
against `SPEC.md` and `PROMPT.md` at gist revision `d1e540d`.

| file | agent | what it holds |
|---|---|---|
| [`binding-a-report.md`](binding-a-report.md) | binding A (moc, C++20) | the 15 readings it chose where the spec is silent, the seven places it thinks are wrong, what it could not build |
| [`binding-b-report.md`](binding-b-report.md) | binding B (typed signals, reflect-moc, C++26) | its readings, the parts of the spec written for binding A only, what reflect-moc made hard |

The header of the binding B report says the host re-run was pending when the agent wrote it. It was done afterwards: both
trees were rebuilt from a read-only mount, 0 warnings, 17 of 17 ctest entries (see [`../README.md`](../README.md)).

Two points have an answer from the author since (23 Oct 2026). Section 8.3 is a mistake in the spec, not a choice: the
removal cycle must start at the "about" notification, while the entry is still in the container. And the lazy key-to-row
map of the list model is in his real code too. Both bindings were changed by hand for both; the README lists the hand
changes.

## Addendum 1 (28 Oct 2026)

Two more reports, written against the addendum on nested changes (gist revision `a2fc33b`) by agents that had the tree and the gist and nothing else.

| file | what it holds |
|---|---|
| [`binding-a-addendum-report.md`](binding-a-addendum-report.md), [`binding-a-addendum-before.txt`](binding-a-addendum-before.txt) | binding A: the readings chosen, possible design mistakes, and each AT-10 test on the library before the change |
| [`binding-b-addendum-report.md`](binding-b-addendum-report.md), [`binding-b-addendum-before.txt`](binding-b-addendum-before.txt) | the same for binding B |
| [`matrix-nested-change.md`](matrix-nested-change.md) | not an agent report: what the matrix and its table model do when a receiver changes the matrix (the pinned cases) |
