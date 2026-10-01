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
| `sliders` | qtbase `examples/widgets/widgets/sliders` | a QGroupBox subclass with a custom signal and six public slots, wired to spin boxes and check boxes |
| `queuedcustomtype` | qtbase `examples/corelib/threads/queuedcustomtype` | a custom metatype (`Block`) in a queued cross-thread signal, public and private slots, a slot overloaded with a non-slot |

Upstream commits: qtbase `7ddbc87d8e14ce51d2957ea72d0a6077593d5ff4`, qtdeclarative `3a714efe0672ae1ba06f09864f64957560c69b91`.

## Licence

The upstream files carry `SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause`; they are used here under BSD-3-Clause (`LICENSES/BSD-3-Clause.txt`, copied from qtbase). Their copyright headers are kept unchanged, in the ported trees too.

Files that are not upstream: `harness.cpp` in each example, the harness target appended to each `CMakeLists.txt`, `common/meta_dump.hpp`, `check.sh` and the generated files.

## Harnesses

Each upstream app runs until its window closes, so each example has a `harness.cpp` that drives the same classes headless (`QT_QPA_PLATFORM=offscreen`) and exits. It prints the meta-object each class adds (methods with access and parameter names, properties with flags and NOTIFY, class info, enums), then a trace of the signals and state changes. Anything nondeterministic is left out: render timings, the party's start time, and the random block positions.

## Run

In the image from `docker/Dockerfile`:

```sh
docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 examples/qt/check.sh original
docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 examples/qt/check.sh ported
```

`check.sh` builds each example's app and harness, fails a ported build whose log shows a moc invocation, diffs the harness output against `expected_output.txt`, and runs the app for two seconds as a smoke test.
