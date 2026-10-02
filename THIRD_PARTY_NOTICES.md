# Third-party notices

reflect-moc is MIT-licensed (see `LICENSE`). This file lists everything in the
repository that is not Filip Sajdak's own code, and the sources the design
learned from that are not included.

## Included: four Qt examples and the ports derived from them

The directories `examples/qt/<name>/original/` hold unmodified example sources
from Qt 6.10.3 (the files that came from upstream, plus a `harness.cpp` and a
harness target appended to each `CMakeLists.txt`, which are ours).
`examples/qt/<name>/ported/` holds copies of those files rewritten by
`tools/rqt-migrate`, so they are derivative works of the originals.

| example | upstream path (tag v6.10.3) | copyright notice in the files |
|---|---|---|
| `mandelbrot` | qtbase `examples/corelib/threads/mandelbrot` | Copyright (C) 2021 / 2022 The Qt Company Ltd. |
| `sliders` | qtbase `examples/widgets/widgets/sliders` | Copyright (C) 2016 / 2022 The Qt Company Ltd. |
| `queuedcustomtype` | qtbase `examples/corelib/threads/queuedcustomtype` | Copyright (C) 2016 / 2022 The Qt Company Ltd. |
| `birthdayparty` | qtdeclarative `examples/qml/tutorials/extending-qml-advanced/advanced6-Property-value-source` | Copyright (C) 2022 / 2023 The Qt Company Ltd. |

Upstream commits: qtbase `7ddbc87d8e14ce51d2957ea72d0a6077593d5ff4`,
qtdeclarative `3a714efe0672ae1ba06f09864f64957560c69b91`.

- **Upstream licence:** every upstream file carries
  `SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause`.
- **Licence used here:** BSD-3-Clause. The text is in
  `examples/qt/LICENSES/BSD-3-Clause.txt`, copied from qtbase's `LICENSES/`
  directory (a template with `<year> <owner>` placeholders, as in qtbase; the
  actual notices are the per-file headers).
- **Notices kept:** the copyright and SPDX headers are unchanged in the
  originals and in the ported copies.
- **Not upstream, and MIT like the rest of the repository:** the `harness.cpp`
  in each example (no Qt header), the harness target appended to each
  `CMakeLists.txt`, the generated `rqt_qml_types.hpp`, `examples/qt/common/`,
  the scripts in `examples/qt/`, `migration.diff`, `unmigrated.md` and
  `expected_output.txt`.

The BSD-3-Clause condition that the copyright notice, the conditions and the
disclaimer are retained applies to these files. Qt is a trademark of The Qt
Company Ltd.

## Not included: Qt itself

The headers and libraries of Qt are not part of this repository. The tests and
examples compile against an installed Qt (6.10.3 in the Docker image, which
downloads it with `aqtinstall` at image build time) under that installation's
own licence: Qt's open-source licences (LGPL-3.0 and others) or a commercial
licence. reflect-moc does not copy code from Qt's headers. It writes the
`QMetaObject` data and the `QtPrivate::FunctionPointer` specialization that
Qt's public and private headers describe, and the `moc` output used as the
oracle in `tests/qt/capability_differential_against_moc.cpp` is produced at
build time and not stored.

## Not included: references the design learned from

Nothing from these was copied. They are listed so the debt is visible.

- [Verdigris](https://github.com/woboq/verdigris) by Woboq: a constexpr `QMetaObject`
  built with macros, without moc.
- [moc-ng](https://github.com/woboq/moc-ng) by Woboq: a moc reimplemented on
  libclang.
- [CopperSpice](https://www.copperspice.com/): a set of C++ libraries that
  removed the meta-object compiler.
- The Qt wiki page "C++ reflection (P2996) and moc", the notes of the Qt
  Contributor Summit 2024 session "C++26 Reflection", and Volker Hilsheimer's
  post "C++26 Reflection and QRangeModel" on the Qt blog.

Searching the tree for copyright and licence headers
(`git grep -n -i -E "copyright|SPDX|derived from|adapted from"`) finds only the
Qt example files above and the repository's own `LICENSE`.
