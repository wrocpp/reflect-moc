# reflect-moc

reflect-moc builds a stock Qt `QMetaObject` from C++26 reflection, so a Qt class compiles with no `moc` run. It is a
header-only library for GCC 16.2 and Qt 6.10: you write `RQT_OBJECT` where `Q_OBJECT` was, annotate slots, and
declare signals as data members.

## Status

Experimental. It has been verified on GCC 16.2.0 and Qt 6.10.3, on aarch64 Linux in Docker with the
offscreen platform plugin (all measurements and baselines) and on x86_64 Linux in the GitHub Actions
job `test` (see `.github/workflows/ci.yml`). It has not been tested with other Qt versions, with clang or with MSVC.

## A class with no moc

This is the `Counter` from [`examples/demo/demo.cpp`](examples/demo/demo.cpp), which builds and runs in the
Docker image with no `moc` step:

```cpp
class Counter : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
  RQT_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)

 public:
  explicit Counter(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  QString label() const { return label_; }

 public slots:
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    emit valueChanged(v);
  }
  [[= rqt::slot]] void setLabel(QString l) {
    if (l == label_) return;
    label_ = l;
    emit labelChanged(l);
  }
  [[= rqt::slot]] void reset() {
    setValue(0);
    setLabel(QString());
  }
  [[= rqt::invokable]] int add(int n) {
    setValue(value_ + n);
    return value_;
  }

 signals:
  [[= rqt::names("value")]] rqt::signal<void(int)> valueChanged;
  [[= rqt::names("label")]] rqt::signal<void(QString)> labelChanged;

 private:
  int value_ = 0;
  QString label_;
};
```

Qt sees an ordinary `QMetaObject`: `connect` (pointer-to-member, lambda and string-based), `invokeMethod`,
`QMetaProperty`, queued connections across threads, `QSignalSpy` and QML bindings all work on it. The library's own
documentation, with every macro and annotation, is
[`include/reflect_moc/qt/README.md`](include/reflect_moc/qt/README.md).

## What has been checked

| What | Result |
|---|---|
| Four official Qt 6.10.3 examples (`mandelbrot`, `sliders`, `queuedcustomtype`, `birthdayparty`) ported by `tools/rqt-migrate` | Each harness prints the same output as the moc build (11, 27, 15 and 54 lines), built with `-Wall -Wextra` and no warnings. See [`examples/qt/RESULTS.md`](examples/qt/RESULTS.md) |
| `moc` never runs | `examples/qt/check_moc_trapped.sh` replaces every `moc` in the container with a trap, then builds the ported examples, the demos and the library tests: 0 moc invocations, apart from the differential test, which runs real `moc` on purpose as its oracle |
| Same meta-object as moc | `capability_differential_against_moc` compares the `QMetaObject` of real `moc` output with reflect-moc's for the same classes, static signals included |
| Object size (GCC 16.2, aarch64, `sizeof(QObject)` is 16) | A non-static signal costs 8 bytes and the class 8 bytes for the owner anchor: 1 signal 32 B, 3 signals 48 B. A static signal costs 0 bytes: 3 static signals are 24 B under `RQT_OBJECT` and 16 B under `RQT_OBJECT_STATIC` |
| ThreadSanitizer (`scripts/tsan-qt.sh`) | 3 threaded tests, 0 reports with `ignore_noninstrumented_modules=1` (Qt is not instrumented). A fourth, `capability_custom_metatype_queued`, is left out because it reports 8 races that come from Qt's uninstrumented `QSemaphore` |

Compile times and binary sizes against moc are in [`examples/qt/RESULTS.md`](examples/qt/RESULTS.md) and
[`docs/measurements.md`](docs/measurements.md). The timings were taken on a loaded machine and are noisy, so no ratio is
quoted here. Stripped executables were within 0.2 percent of the moc builds.

## Limits

- Overloaded signals cannot be data members (two members cannot share a name); use the function form `[[=rqt::signal_function]]`.
- `Q_GADGET`, `Q_NAMESPACE`, `Q_PLUGIN_METADATA` and `QML_ELEMENT` are not supported, and `BINDABLE` properties stop the build.
- A mixin class `rqt::Object<B>` needs `bind()` in its constructor and cannot have data-member signals.
- `rqt::connect` does not accept a static signal; use `QObject::connect`.
- A class template with signals fails to build.
- A lambda that captures `this` implicitly with `[=]` is an error under `-Werror` (C++20 deprecation); write `[this]`.
- `QT_NO_KEYWORDS` is untested with the compat `emit`.
- Private signals are not covered by the committed tests.
- clang-p2996 lacks `std::meta::current_function`, `std::meta::exception` and `annotations_of_with_type`, so the library is GCC only.
- The code reaches into `QtPrivate::FunctionPointer`, which follows its shape in Qt 6.10.3 and may change.

The longer list is in the "Limits" sections of the [library README](include/reflect_moc/qt/README.md).

## Static signals and one silent mistake

`rqt::signal<void(int)> s;` keeps an owner pointer in the object. The opt-in form
`static inline rqt::static_signal<void(int)> s{};` takes no space in the object, and it comes with one pitfall that
compiles without a warning. A static member is a single object for the whole class, so `emit other->sig(v)`, where
`other` is another instance of the same class, fires the signal on `this`. Qt's own signals would fire on `other`.
Write `sig(v).from(other);` instead. `tools/rqt-lint-emit.py` flags the wrong form, and the test
`limit_static_signal_other_instance` pins the behavior.

## Running it

Everything runs in a Docker image with GCC 16.2 and Qt 6.10.3, which the Dockerfile downloads with `aqtinstall`
(on Apple Silicon it builds natively for arm64). From the repository root:

```sh
docker build -t reflect-moc/gcc16-qt610 docker

# core and Qt tests, including the differential test and the build-failure tests
docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 \
  sh -c 'cmake --preset qt && cmake --build --preset qt -j4 && ctest --preset qt -j2'

# the four ported Qt examples: build with no moc, compare the output with the moc build
docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 examples/qt/check.sh ported

# the moc trap: moc is replaced by a failing stub. It renames the real moc, so
# it refuses to run outside a container; mount the checkout read-only
docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 sh examples/qt/check_moc_trapped.sh
```

The Qt preset builds and runs 70 tests (it took about two minutes to build and half a minute to run on an 8-core Apple Silicon Mac), and a GCC reflection build can use a few GB per job, so lower `-j4` if Docker has little memory.
`examples/qt/README.md` lists the other scripts (`check.sh original`, `check_no_moc.sh`, `migrate.sh`, `measure.sh`),
and `examples/demo/run.sh` runs the interactive demo.

## How signals work

C++26 reflection can add data members to a class but cannot add a function body, so a signal here is a bodyless data
member: `rqt::signal<void(int)> valueChanged;`. `RQT_OBJECT` declares a hidden first member whose constructor computes
the owner's address, and every signal constructed after it keeps that pointer and looks up its index in a table built
by reflection. Calling the signal then ends in Qt's own `QMetaObject::activate`, which is why stock Qt `connect`
accepts `&Counter::valueChanged`.

A series of posts at wrocpp.github.io describes the design, starting from 6 October 2026 with
[Qt's metaobject, built from reflection](https://wrocpp.github.io/posts/qt-metaobject-from-reflection/).

## Prior art

- [Verdigris](https://github.com/woboq/verdigris) from Woboq builds a constexpr `QMetaObject` with macros (`W_OBJECT`, `W_SIGNAL`). reflect-moc replaces the macro lists with reflection and keeps the text of `Q_PROPERTY` as it is.
- [moc-ng](https://github.com/woboq/moc-ng) reimplements moc on libclang, and [CopperSpice](https://www.copperspice.com/) is a set of C++ libraries that removed the meta-object compiler.
- The Qt wiki page [C++ reflection (P2996) and moc](https://wiki.qt.io/C++_reflection_(P2996)_and_moc) lays out what reflection might not do for moc, signals first.
- The [QtCS 2024 notes on C++26 reflection](https://wiki.qt.io/QtCS2024_C++26_Reflection) record the same worry.
- Volker Hilsheimer's [C++26 Reflection and QRangeModel](https://www.qt.io/blog/c26-reflection-qrangemodel) ends on moc.

None of their code is included; see [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## AI disclosure

The code, tests, documentation and the accompanying posts in this project were generated by AI (Claude, by
Anthropic) under the direction of Filip Sajdak, who reviews them and is responsible for them. The same policy applies
to everything published under [wro.cpp](https://wrocpp.github.io/ai/).

reflect-moc is not endorsed by or affiliated with The Qt Company. Qt is a trademark of The Qt Company Ltd.

## Licence

MIT, see [`LICENSE`](LICENSE). The four Qt examples under `examples/qt` are BSD-3-Clause, see
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
