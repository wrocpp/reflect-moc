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

## How to repeat

    docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/proof.sh

Not measured here: x86_64, other compilers, other Qt versions.
