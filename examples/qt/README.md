# Official Qt examples, ported to reflect-moc

Four examples from Qt 6.10.3, each kept twice:

- `<name>/original/`: the upstream sources, built with moc (`qt_standard_project_setup()` turns AUTOMOC on). This is the baseline.
- `<name>/ported/`: generated from `original/` by `tools/rqt-migrate`, built with AUTOMOC off. Do not edit it by hand: change the tool or the original and regenerate it.
- `<name>/migration.diff` and `<name>/unmigrated.md`: the tool's diff and its report of what it could not migrate.
- `<name>/expected_output.txt`: the baseline harness output. The ported harness must print exactly the same.

| example | upstream path (tag v6.10.3) | moc surface it exercises |
|---|---|---|
| `mandelbrot` | qtbase `examples/corelib/threads/mandelbrot` | QThread subclass, a queued `renderedImage(QImage, double)` signal, mutex and wait condition |
| `birthdayparty` | qtdeclarative `examples/qml/tutorials/extending-qml-advanced/advanced6-Property-value-source` | Q_PROPERTY READ/WRITE/NOTIFY (one NOTIFY shared by four properties), QML_ELEMENT / QML_ANONYMOUS / QML_UNCREATABLE / QML_ATTACHED, Q_CLASSINFO("DefaultProperty"), Q_INTERFACES, a private slot, QQmlListProperty, a QObject hierarchy three deep |
| `sliders` | qtbase `examples/widgets/widgets/sliders` | a QGroupBox subclass with a custom signal and six public slots, wired to spin boxes and check boxes (a signal is also the receiver of a pointer-to-member connect) |
| `queuedcustomtype` | qtbase `examples/corelib/threads/queuedcustomtype` | a custom metatype (`Block`) in a queued cross-thread signal, public and private slots, a slot overloaded with a non-slot |

Upstream commits: qtbase `7ddbc87d8e14ce51d2957ea72d0a6077593d5ff4`, qtdeclarative `3a714efe0672ae1ba06f09864f64957560c69b91`.

Results (proof that the ported builds run no moc and behave the same, migration cost, build time, size): [`RESULTS.md`](RESULTS.md).

## Licence

The upstream files carry `SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause`; they are used here under BSD-3-Clause (`LICENSES/BSD-3-Clause.txt`, copied from qtbase). Their copyright headers are kept unchanged, in the ported trees too.

Files that are not upstream: `harness.cpp` in each example, the harness target appended to each `CMakeLists.txt`, `common/meta_dump.hpp`, the scripts in this directory and the generated files.

## Harnesses

Each upstream app runs until its window closes, so each example has a `harness.cpp` that drives the same classes headless (`QT_QPA_PLATFORM=offscreen`) and exits. It prints the meta-object each class adds (methods with access and parameter names, properties with flags and NOTIFY, class info, enums), then a trace of the signals and state changes. Anything nondeterministic is left out: render timings, the party's start time, and the random block positions.

## Run, from a clean checkout

Everything runs in the `reflect-moc/gcc16-qt610` image (GCC 16.2, Qt 6.10.3, offscreen QPA), which has the moc the originals and the migration tool need. Build it once:

```sh
docker build -t reflect-moc/gcc16-qt610 docker
```

Then, from the repository root (each command mounts the checkout at `/src`):

```sh
RUN="docker run --rm -v $PWD:/src -w /src reflect-moc/gcc16-qt610"

$RUN examples/qt/check.sh original                 # build with moc, run the harnesses, compare with the baseline
$RUN examples/qt/check.sh ported                   # the same for the ported trees (fails on any moc line or warning)
$RUN examples/qt/check_no_moc.sh                   # rebuild each ported example and fail on any moc invocation
docker run --rm -e RQT_VARIANT=original -v $PWD:/src -w /src reflect-moc/gcc16-qt610 \
    examples/qt/check_no_moc.sh                    # positive control: must FAIL on every original
docker run --rm -v $PWD:/src:ro -w /src reflect-moc/gcc16-qt610 \
    sh examples/qt/check_moc_trapped.sh            # the strong proof: moc replaced by a failing trap, see below
$RUN examples/qt/migrate.sh                        # regenerate ported/, migration.diff and unmigrated.md
$RUN examples/qt/measure.sh                        # build time (3 runs per variant) and binary sizes
```

- `check.sh [--record] original|ported [example...]` builds each example's app and harness with `-Wall -Wextra`, diffs the harness output against `expected_output.txt` (`--record` rewrites it; use it on `original` only), counts moc lines and warnings in the build log, and runs the app for two seconds as a smoke test. `RQT_EXAMPLES_BUILD` sets the build directory.
- `migrate.sh [example...]` runs `tools/rqt-migrate`; `RQT_MIGRATE_FLAGS` picks the style (default `--style qtlike --signals members --macros rqt`; `--style annotations` is the mixin/annotation syntax recorded in `RESULTS.md`).
- `check_moc_trapped.sh` is the check that does not rely on reading build logs. It replaces every `moc` in the container with a trap that records the caller and fails, then builds the four ported examples, the two demos and all library tests. It passes only if nothing but the differential test (which runs real moc on purpose, as its oracle) needs moc; that test failing to build is the positive control. **It renames the real moc, so it refuses to run outside a container**; mount the repository read-only as shown. `RQT_TRAP_QUICK=1` skips the long library-test step. It has been run on aarch64 Linux in the image; whether it passes on x86_64 is reported by the CI job `test`.
- `measure.sh` honours `RQT_RUNS` (default 3) and `RQT_JOBS` (default 2).
- The tool's own unit tests need no Qt: `cd tools && python3 -m unittest discover -s tests -p 'test_*.py'`.

Run the builds from a snapshot of committed code if the library is being edited at the same time.
