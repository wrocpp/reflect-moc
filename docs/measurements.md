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

## How to repeat

    docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/proof.sh

Not measured here: x86_64, other compilers, other Qt versions.
