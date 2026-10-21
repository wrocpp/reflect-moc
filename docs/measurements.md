# Measurements of the Qt layer

All numbers: GCC 16.2.0, Qt 6.10.3, Linux aarch64 in the `reflect-moc/gcc16-qt610` docker image, run inside a
Mac that was heavily loaded at the time, so timings are noisy. The compile-time and binary-size comparison of
whole ports against moc (birthdayparty, mandelbrot, sliders) is in `examples/qt/RESULTS.md`.

## Signals as data members: the owner-anchor design

`tests/qt/measure/proof.sh` builds a header class with data-member signals in two translation units and a
micro-benchmark, and compares the owner-anchor design (current) with the lambda-tag design (the commit that added `capability_data_signals_two_tus`,
built from `git archive`).

| check | result |
|---|---|
| header class in two translation units, `-Werror -Wall -Wextra`, no `-Wno-subobject-linkage` | compiles with 0 warnings, test passes |
| the same with `-flto -Wodr` | compiles with 0 warnings, test passes |
| `sizeof` of the benchmark class (3 signals, a `QObject`) | 24 bytes (lambda tag, 1 byte per signal) -> 48 bytes (anchor: 8 bytes per signal, 1 byte for the anchor, padding) |
| emit of a signal connected to one lambda, `-O2`, 5 000 000 emits | anchor 52.6 / 87.7 / 39.1 ns; lambda tag 261.8 / 49.7 / 51.3 ns over three runs: the same within the noise of the loaded machine (about 40 to 50 ns per emit, most of it inside Qt's own `QMetaObject::activate` and the functor call) |

The emit path of the anchor design does one pointer subtraction and a search of the class's signal-offset table
(a handful of entries) before `QMetaObject::activate`.

## Static signals: size, emit cost and the ThreadSanitizer run

`capability_static_signal_size` prints and asserts the `sizeof` of five classes (`sizeof(QObject)` is 16):

| class | `sizeof` |
|---|---|
| 1 non-static `rqt::signal`, `RQT_OBJECT` | 32 |
| 3 non-static `rqt::signal`, `RQT_OBJECT` | 48 |
| 3 `rqt::static_signal`, `RQT_OBJECT_STATIC` | 16 |
| 3 `rqt::static_signal`, `RQT_OBJECT` (the anchor stays) | 24 |
| 1 non-static + 2 static, `RQT_OBJECT` | 32 |

So a non-static signal costs 8 bytes and the class 8 bytes for the anchor; a static signal costs 0 bytes, and
`RQT_OBJECT_STATIC` saves the anchor's 8.

Emit with no connection, `tests/qt/measure/static_signal_bench.cpp`, `-O2`, 10 000 000 emits per run, a class of
four signals under `RQT_OBJECT` (so 56 bytes non-static, 24 static). **One invocation (three runs), on a loaded
Mac: noisy, do not read differences under about 2 ns.**

| form | first signal (ns/emit, runs 1/2/3) | last signal (ns/emit, runs 1/2/3) |
|---|---|---|
| non-static `rqt::signal`, `a(v)` | 9.51 / 10.35 / 10.55 | 11.90 / 14.44 / 11.49 |
| `rqt::static_signal`, `emit a(v)` | 8.82 / 8.78 / 10.09 | 8.38 / 9.55 / 9.50 |

The static form is not slower here: it searches the class's static signals by address (a fold over a handful of
entries) where the non-static one searches a table of offsets, and it builds a small tuple of the arguments.
In an earlier spike run (spikes/SPIKES.md, 09) the numbers were 8.2 / 10.1 (non-static) and 8.5 / 9.1 (static).

ThreadSanitizer, `scripts/tsan-qt.sh` (GCC 16.2, `-fsanitize=thread -g -O1`, Qt 6.10.3 not instrumented,
`TSAN_OPTIONS=halt_on_error=0:ignore_noninstrumented_modules=1`, `setarch -R`, seccomp unconfined):

    scripts/tsan-qt.sh
    control (a real race): 1 report(s)
    capability_static_signal_queued: exit 0, 0 report(s)
    capability_queued_across_qthread: exit 0, 0 report(s)
    capability_signal_to_signal_connect: exit 0, 0 report(s)
    3 tests, 0 reports, 0 failed

3 tests, 0 reports. The new threaded test is `capability_static_signal_queued` (queued main to worker, 50 emissions from
two instances sharing one static signal object, and an emit from a `QThread::run`); the other two are existing
threaded tests. Without `ignore_noninstrumented_modules=1` TSan reports the queued-argument copy as a race in every
one of them, the existing ones included, because Qt hands the arguments over through a futex mutex it cannot see;
`capability_custom_metatype_queued` reports 8 races even with the option (it reads plain members after a
`QSemaphore::acquire`), so it is left out. A TSan build of Qt would remove both limits; it was not tried.

## Typed signals against the QVariant split (observable containers)

The question: a class template cannot have `Q_OBJECT`, so the moc design puts the signals in a non-template
base and boxes every key in a `QVariant`. What does the typed template (`examples/qt/observable_models`) cost or
save against that? The moc side, `examples/qt/observable_models_qvariant`, is a reconstruction from the description
in a reader's post, not his code. It has the same 10 signals and the same row logic as the typed model, line for
line; only how the key travels differs (`QVariant::fromValue` at the emit, `value<Key>()` in the slot).

Before any timing, the twin passes the same seeded random driver as the typed version (7 containers, 8 seeds of
4,000 steps, `QAbstractItemModelTester` in fatal mode, view mirror, selection, persistent index), and a copy of the
twin that announces every removal for row 0 is caught by that driver (`run.sh` does both).

GCC 16.2.0, Qt 6.10.3, `-O2`, aarch64 in the docker image on a Mac that was in use (load 0.9 to 1.3), 7 runs
alternating the two designs, median with (min-max). The machine is noisy: **do not read a difference under about
10 percent, and none at all under 2 ns**. Numbers are from `tests/qt/measure/observable_bench/run.sh`; the x86_64
run is the `bench` workflow (manual), linked from the pull request.

### Size

`sizeof` of a class shaped like the container (ten signals and a `QHash` member):

| design | bytes |
|---|---|
| moc, signals in a base (QVariant) | 24 |
| `rqt::signal` members, `RQT_OBJECT` | 112 |
| `rqt::static_signal` members, `RQT_OBJECT_STATIC` | 24 |

A data-member signal costs 8 bytes in every instance and the class 8 more for its anchor. Static signals cost
nothing per instance and match moc.

### Signal only (one connected slot, ns per emit)

| key | QVariant | `rqt::signal` | `rqt::static_signal` |
|---|---|---|---|
| `quint32` | 37.8 (35.8-46.3) | 25.8 (24.7-37.0) | 26.7 (23.6-30.7) |
| `QString` | 38.5 (34.6-41.2) | 27.9 (26.1-46.5) | 27.5 (23.6-38.7) |
| 32-byte struct | 46.0 (43.6-70.6) | 28.2 (27.3-32.4) | 25.5 (23.6-32.8) |

The struct is the worst case for the QVariant side: it does not fit the variant's inline storage, so every boxing
allocates. A `quint32` or a `QString` does not, and the gap is already there for them.

### End to end (model connected, no view, ns per change at 1,000 entries)

| container | change | QVariant | typed | typed / QVariant |
|---|---|---|---|---|
| `QList` | fill | 153.7 (145.5-175.1) | 130.2 (120.6-166.1) | 0.85 |
| `QList` | replace | 91.5 (86.8-100.4) | 72.4 (71.1-97.3) | 0.79 |
| `QList` | move | 769.6 (705.5-817.2) | 753.6 (667.5-805.7) | 0.98 |
| `QList` | drain | 155.8 (144.8-166.3) | 127.5 (122.3-237.2) | 0.82 |
| `QHash` | fill | 162.4 (147.0-223.6) | 120.5 (115.8-126.7) | 0.74 |
| `QHash` | replace | 325.0 (301.6-594.1) | 278.0 (264.1-297.7) | 0.86 |
| `QHash` | drain | 285.5 (271.1-392.5) | 246.9 (242.6-262.3) | 0.86 |
| `QMap` | fill | 1204.7 (1119.7-1693.9) | 1167.1 (1087.6-1251.8) | 0.97 |
| `QMap` | replace | 1857.9 (1750.4-2446.3) | 1875.6 (1716.5-1946.0) | 1.01 |
| `QMap` | drain | 1192.8 (1089.9-1591.9) | 1152.3 (1076.8-1219.1) | 0.97 |

On this machine the typed version is faster or equal in all ten rows, by about 15 to 25 percent for a list and a hash, and by
nothing measurable for a map (the x86_64 run below has one exception), where the row count (a walk of the tree) dominates both. The model's begin/end calls
and `QMetaObject::activate` are most of every figure, so the key transport is a small part of a change.

### The same on x86_64

The `bench` workflow ([run 39480417966](https://github.com/wrocpp/reflect-moc/actions/runs/39480417966)), GitHub
ubuntu-24.04 runner, 4 cores, GCC 16.2.0, Qt 6.10.3, 7 runs, load 1 to 2.5. The spread is far smaller than on the Mac
(most cells within 2 percent), so the ratios are readable. ns per change at 1,000 entries, median:

| container | change | QVariant | typed | typed / QVariant |
|---|---|---|---|---|
| `QList` | fill | 146.3 | 120.1 | 0.82 |
| `QList` | replace | 78.8 | 52.5 | 0.67 |
| `QList` | move | 746.9 | 676.9 | 0.91 |
| `QList` | drain | 152.2 | 128.1 | 0.84 |
| `QHash` | fill | 154.9 | 126.9 | 0.82 |
| `QHash` | replace | 258.4 | 236.6 | 0.92 |
| `QHash` | drain | 267.5 | 297.8 | 1.11 (code placement, see below) |
| `QMap` | fill / replace / drain | 1279 / 2055 / 1262 | 1299 / 2046 / 1246 | 1.02 / 1.00 / 0.99 |

Signal only: 51.2 against 38.1 ns for a `quint32` key, 64.2 against 37.8 for a `QString` and for the 32-byte
struct. Sizes are the same as above. Compile, one translation unit: typed 7.29 s, QVariant 5.16 s plus 2.51 s for moc
and its output (7.67 s). Binary 147,984 against 130,520 bytes.

**One result went against the typed version, and it was not the signals.** Draining a hash model (removing entries in
random order) was slower with typed signals: 1.11 at 1,000 entries, 1.62 at 10,000 and 1.73 at 100,000 (16.1 against
9.3 microseconds per removal), with a spread under 1 percent, and the Mac showed the same direction at 100,000
entries (1.22, three noisy runs). Chasing it (the `investigate/hash-drain` runs, `drain_probe.sh` and
`drain_profile.sh` in `tests/qt/measure/observable_bench`):

- The scan that finds a key's row is a four-instruction loop, the same in both builds. In the typed build the
  compiler placed it so that the fused compare and jump crosses a 32-byte boundary (`0x4084fe` to `0x408504`); in
  the QVariant build it does not.
- Building both with `-Wa,-mbranches-within-32B-boundaries` made them equal at 8.2 microseconds per removal. Building
  both with `-falign-loops=64` made both slow, 15.1. The runner's CPU was an AMD EPYC, so this is not the Intel
  jcc erratum by name; I did not find which fetch or branch mechanism it is, only that moving the loop moves the cost.
- The cost is user time in the model's slots. `perf record` puts essentially all samples in the removal and replace
  slots (the split is 46/46 typed and 60/30 QVariant, which is the expected 1:2 for a shrinking list against a fixed
  one); system time is under 0.01 s and `strace -c` shows only start-up calls. Hardware counters are not available in the
  runner's VM, so cycles, instructions and branch misses were not read.
- Replace in the same model never showed a gap: its copy of the loop landed somewhere harmless.
- With the key list as a `std::vector` and `std::find`, both builds cost 7.6 microseconds a removal and 11.9 a
  replace, with no gap. On aarch64 `std::find` also scans about 35 percent faster than `QList::indexOf` (13 against 20
  microseconds for 100,000 keys). The model now keeps its keys in a `std::vector`.

So the gap measured between the designs on this path was code placement, not signal transport. The tables above were
measured with the `QList` key list; the removal rows for a hash at 1,000 entries and above should be read with
that in mind.

### How the cost grows

The model finds a row from a key by walking (a `QMap` counts from the beginning, a hash model searches its key
list), so a change costs time proportional to the size. Three runs per point, same row logic on both sides, so this
is the model's cost and not the comparison. Typed side, ns per change:

| entries | `QHash` replace | `QMap` replace | `QList` move |
|---|---|---|---|
| 100 | 105 | 450 | 195 |
| 1,000 | 336 | 1,876 | 731 |
| 10,000 | 2,463 | 53,789 | 6,800 |
| 100,000 | 27,827 | not run | 68,128 |

At 100,000 entries a replace in a hash model takes about 28 microseconds, and 10,000 entries in a map about 54.
The QVariant column is within the noise of these everywhere from 10,000 entries up (ratios 0.91 to 1.22 in the
same runs); the full output is in the pull request. The 1,000-entry `QMap` figure above is from the seven-run table: in the three-run set that point read 3,467 ns
(ratio 1.7 against QVariant), which did not repeat, so it is noise.

### After the row index

The models now keep a row index (`examples/qt/observable_models/row_index.hpp`): a sorted vector of keys for a map and
a Fenwick tree over insertion slots for a hash or set, so a row is found from a key, and a key from a row, in
logarithmic time. The QVariant twin takes the same index. Same machine as above (aarch64, busy: load 2.8 to 6, 5 runs,
median), typed side, ns per change:

| entries | change | linear scan (before) | row index (after) |
|---|---|---|---|
| `QHash`, 10,000 | replace | 2,463 | 98 |
| `QHash`, 100,000 | replace | 27,827 | 137 |
| `QHash`, 100,000 | drain | 21,032 | 257 |
| `QMap`, 10,000 | replace | 53,789 | 255 |
| `QMap`, 10,000 | drain | 25,012 | 507 |

A change is 49 to 211 times cheaper (replace: hash 203 times, map 211; remove: hash 82 times, map 49), measured at 100,000
entries for a hash and 10,000 for a map. The
typed against QVariant ratios stay inside the noise of this machine (0.78 to 1.22), so the row index does not change
the comparison above, only the size of the numbers it is made of. A list needed none. Costs that remain are a
memmove for an insert or erase in the sorted copy of a map's keys (linear in bytes, not in tree nodes), and the
key list each model keeps.

### Build

One translation unit that includes the container and model and runs the benchmark, `-O2 -c`, median of three:

| | seconds |
|---|---|
| typed (reflection) | 7.9 |
| QVariant, the translation unit | 5.5 |
| QVariant, plus moc and compiling its output | 3.5 |

The reflection build is 2.4 seconds slower for the translation unit, and about 1.1 seconds faster than the moc
build when moc and the generated file are counted. The generated file is compiled once per project, the
translation unit once per file that includes the header, so the total depends on how many files do. Binary:
176,560 bytes typed against 168,240 bytes.

### What this does not show

One compiler, one machine for the main run, and a benchmark with no view attached (a view's own work would make
every difference here smaller in proportion). A real application with 19 files of hand-copied wiring would differ;
this measures the transport of the key and the model's own cost, nothing else.

## Final numbers: typed signals against the QVariant split, with the row index

The tables above were measured before the row index, on a busy Mac, and two of their claims did not survive the
quiet re-run below ("faster or equal in all ten rows", and a hash removal that was slower on x86_64, which was loop
placement, see the section on it). This section is the current measurement: both designs use the same row index,
GCC 16.2.0, Qt 6.10.3, `-O2`, 7 runs alternating the designs, median of ns per change at 1,000 entries.

- aarch64: docker on a Mac, load 0.14 at the start (1.0 at the end).
- x86_64: the manual `bench` workflow on a GitHub runner ([run 39549191481](https://github.com/wrocpp/reflect-moc/actions/runs/39549191481)), spread under 2 percent.

| container, change | Mac QVariant | Mac typed | x86_64 QVariant | x86_64 typed |
|---|---|---|---|---|
| `QList` fill | 120.3 | 110.3 | 142.6 | 116.3 |
| `QList` replace | 70.1 | 80.9 | 80.9 | 75.2 |
| `QList` drain | 121.6 | 109.9 | 149.3 | 123.9 |
| `QHash` fill | 150.6 | 137.4 | 194.5 | 176.9 |
| `QHash` replace | 88.3 | 91.7 | 119.4 | 116.1 |
| `QHash` drain | 181.4 | 176.4 | 246.6 | 227.6 |
| `QMap` fill | 249.2 | 242.3 | 347.5 | 319.4 |
| `QMap` replace | 182.2 | 189.9 | 237.2 | 231.1 |
| `QMap` drain | 226.4 | 226.1 | 309.1 | 284.6 |

Signal only, one connected slot, ns per emit (`quint32` key): QVariant 27.5, typed 20.4 on the Mac; QVariant 39.8,
typed 25.6 on x86_64. A 32-byte key: QVariant 35.3 and 51.2, typed 20.6 and 25.9.

Reading:

- On x86_64 the typed version is faster in all nine rows, by 3 to 18 percent, and in every size from 1,000 to 100,000
  entries (ratios 0.82 to 0.99).
- On the quiet Mac it is faster for fill and drain of a list and a hash (by 1 to 10 percent), equal for a map, and
  **slower for `replace`, by 4 to 15 percent at 1,000 entries and up to 21 percent for a list of 100 entries**. A
  replace emits one signal that no model listens to, and an emit with no receiver costs the typed signal about 10 ns
  (the unconnected-emit table above), against a few for moc. That is consistent with the 10 to 15 ns gap seen
  here, but I did not isolate it.
- So end to end the two designs are within roughly 10 percent of each other, and a change costs 70 to 320 ns in
  either. The difference worth having is the typed key, not the speed; the typed emit with a receiver is 25 to 35
  percent faster, which is a bare signal measurement, not a model.
- Size: 112 bytes against 24 per container (24 with static signals). Binary: 186,568 against 171,856 bytes on the Mac
  and 189,760 against 149,088 on x86_64. Compile, one translation unit: 4.54 s typed against 3.06 s plus 1.32 s for
  moc on the Mac, 9.83 s against 6.61 s plus 3.04 s on x86_64, so the total is within 4 percent either way.

## The regenerated spec in two bindings: moc with `QVariant` keys against typed signals

`tests/qt/measure/spec_bench/run.sh` builds one source against binding A and binding B of
[`examples/qt/spec_regen`](../examples/qt/spec_regen/README.md) through the same `obs::` API. **The two trees were written
by two different agents, so unlike the controlled twin above they do not share their model code.** A difference in a
batch or a reset row can come from the model code as well as from the signal transport; only the per-entry rows are
close to the earlier controlled comparison. The gate runs first: both bindings pass their 17 acceptance entries, and a
copy of each whose list model removes runs in ascending order is caught by `tst_list_model` (it crashes, because the
Qt bookkeeping is left unbalanced).

Seven runs, alternating the two binaries, median. ns per change for fill, replace and drain; ns per call for the
others (100 keys per batch, a 100-cell `setValues`). The x86_64 run is the manual `bench` workflow on a GitHub runner
(load 1.05, spread mostly under 5 percent). The Mac run is aarch64 in Docker on a **busy** machine (load 1.8 at the
start, 6.3 at the end), so read differences under about 10 percent there as noise.

| container | entries | operation | x86_64 A | x86_64 B | B / A | Mac A | Mac B | B / A |
|---|---|---|---|---|---|---|---|---|
| Matrix100x100 | 100 x 100 | setValues-block-100-cells | 9,681 | 9,983 | 1.03 | 7,872 | 7,453 | 0.95 |
| Matrix100x100 | 100 x 100 | setValues-diagonal-100-cells | 22,884 | 20,939 | 0.92 | 18,625 | 17,893 | 0.96 |
| QHash | 1,000 | drain | 11,226 | 11,654 | 1.04 | 10,436 | 12,253 | 1.17 |
| QHash | 1,000 | fill | 222 | 184 | 0.83 | 204 | 170 | 0.83 |
| QHash | 1,000 | insertMany-contiguous | 14,286 | 6,042 | 0.42 | 8,288 | 5,712 | 0.69 |
| QHash | 1,000 | insertMany-scattered | 23,229 | 14,993 | 0.65 | 22,192 | 15,700 | 0.71 |
| QHash | 1,000 | removeMany-contiguous | 44,969 | 40,053 | 0.89 | 35,560 | 36,231 | 1.02 |
| QHash | 1,000 | removeMany-scattered | 54,977 | 47,602 | 0.87 | 44,023 | 43,544 | 0.99 |
| QHash | 1,000 | replace | 152 | 122 | 0.80 | 153 | 119 | 0.78 |
| QHash | 10,000 | drain | 193,274 | 197,616 | 1.02 | 189,426 | 187,944 | 0.99 |
| QHash | 10,000 | fill | 208 | 175 | 0.84 | 187 | 178 | 0.95 |
| QHash | 10,000 | insertMany-contiguous | 26,058 | 15,335 | 0.59 | 21,811 | 16,297 | 0.75 |
| QHash | 10,000 | insertMany-scattered | 60,315 | 51,121 | 0.85 | 78,655 | 73,628 | 0.94 |
| QHash | 10,000 | removeMany-contiguous | 454,473 | 455,011 | 1.00 | 420,204 | 420,123 | 1.00 |
| QHash | 10,000 | removeMany-scattered | 487,240 | 488,066 | 1.00 | 457,063 | 458,043 | 1.00 |
| QHash | 10,000 | replace | 170 | 131 | 0.77 | 157 | 136 | 0.87 |
| QList | 1,000 | drain | 180 | 141 | 0.78 | 188 | 152 | 0.81 |
| QList | 1,000 | fill | 128 | 100 | 0.78 | 136 | 98 | 0.72 |
| QList | 1,000 | replace | 91 | 52 | 0.58 | 102 | 63 | 0.61 |
| QList | 10,000 | drain | 763 | 731 | 0.96 | 1,072 | 724 | 0.68 |
| QList | 10,000 | fill | 132 | 103 | 0.78 | 142 | 96 | 0.67 |
| QList | 10,000 | replace | 91 | 53 | 0.58 | 111 | 61 | 0.56 |
| QMap | 1,000 | insertMany-contiguous | 22,518 | 14,009 | 0.62 | 15,842 | 12,321 | 0.78 |
| QMap | 1,000 | insertMany-scattered | 35,963 | 27,214 | 0.76 | 31,618 | 26,872 | 0.85 |
| QMap | 1,000 | removeMany-contiguous | 53,526 | 48,896 | 0.91 | 44,581 | 42,797 | 0.96 |
| QMap | 1,000 | removeMany-scattered | 63,553 | 57,603 | 0.91 | 73,929 | 56,600 | 0.77 |
| QMap | 1,000 | resetWith | 20,679 | 19,302 | 0.93 | 22,079 | 27,157 | 1.23 |
| QMap | 10,000 | insertMany-contiguous | 34,051 | 22,968 | 0.67 | 26,598 | 20,275 | 0.76 |
| QMap | 10,000 | insertMany-scattered | 99,395 | 90,302 | 0.91 | 112,470 | 112,353 | 1.00 |
| QMap | 10,000 | removeMany-contiguous | 467,296 | 463,527 | 0.99 | 417,612 | 412,918 | 0.99 |
| QMap | 10,000 | removeMany-scattered | 508,700 | 508,080 | 1.00 | 481,380 | 488,517 | 1.01 |
| QMap | 10,000 | resetWith | 128,968 | 139,660 | 1.08 | 104,403 | 134,501 | 1.29 |

Reading, from x86_64 unless said:

- Per entry, binding B is faster: fill by 16 to 22 percent, replace by 20 to 42 percent, drain between 22 percent faster
  and 4 percent slower (the hash drain at 1,000 and 10,000 entries is within 4 percent).
- `insertMany` of 100 keys is 9 to 58 percent faster in binding B. I did not isolate how much of that is the boxing of
  100 keys into `QVariant` and how much is different model code.
- `removeMany` is equal at 10,000 entries (0.99 to 1.00) and 9 to 13 percent faster in B at 1,000. `resetWith` of a map
  is 7 percent faster in B at 1,000 entries and 8 percent slower at 10,000. On the busy Mac it was 23 and 29 percent
  slower; that did not repeat on x86_64, and I did not look for the cause.
- The matrix `setValues` rows are equal within 8 percent.
- **A finding about the spec, not the bindings.** Section 8.1 asks for a lazy key-to-row map that every change marks
  stale and the next lookup rebuilds. A single removal after a change therefore costs a rebuild: 11 microseconds per
  removal at 1,000 hash entries and 193 microseconds at 10,000, in both bindings, linear in the entry count. The row
  index of `observable_models` costs 167 to 473 nanoseconds per change, at 100,000 hash entries and 10,000 map entries. The row order differs (the
  spec sorts rows by key, `observable_models` keeps insertion order for a hash), so this is an indication, not a
  like-for-like comparison.
- Size: a container is 24 bytes in A and 144 in B; the matrix, with 26 signals, is 120 and 336 bytes. The benchmark
  binary is 20 percent larger in B on x86_64 (325,512 against 272,288 bytes) and 9 percent on the Mac. Compile of one translation unit:
  13.57 s for B against 7.97 s plus 4.86 s of moc for A on x86_64, so the totals are within 6 percent.
- Not measured: a view on top, queued connections, MSVC, Clang.

## How to repeat

    docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/proof.sh
    docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/observable_bench/run.sh
    docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/spec_bench/run.sh

Not measured here: x86_64, other compilers, other Qt versions.
