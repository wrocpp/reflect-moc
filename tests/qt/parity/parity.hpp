// Helpers of the moc parity tests: compare the QMetaObject real moc generated with the one reflect-moc generated for the
// same construct, and say which line differs. The dump is tests/qt/differential_dump.hpp (own members of one class level).
#pragma once

#include "check.hpp"
#include "differential_dump.hpp"

#include <QtCore/QMetaObject>

#include <algorithm>
#include <cstdio>

namespace parity {

// Both meta-objects dump the same, line by line. The class names differ by the namespace (viamoc, viarqt), which the dump
// strips.
inline bool same_meta(char const* what, QMetaObject const* oracle, QMetaObject const* ours) {
  QStringList const expected = dump_class(oracle);
  QStringList const actual = dump_class(ours);
  bool const same = expected == actual;
  if (!same) {
    std::fprintf(stderr, "--- %s differs ---\n", what);
    for (qsizetype i = 0; i < std::max(expected.size(), actual.size()); ++i) {
      QString const e = i < expected.size() ? expected[i] : QString{"<missing>"};
      QString const a = i < actual.size() ? actual[i] : QString{"<missing>"};
      if (e != a) std::fprintf(stderr, "moc : %s\nrqt : %s\n", e.toUtf8().constData(), a.toUtf8().constData());
    }
  }
  return same;
}

// The same, as a CHECK that names the construct.
#define PARITY_SAME(what, oracle, ours) ::rqt_test::report(::parity::same_meta(what, oracle, ours), what, __FILE__, __LINE__)

// Totals Qt derives from the chain: both sides must agree on the counts, offsets and the superclass chain length.
inline bool same_totals(QMetaObject const* oracle, QMetaObject const* ours) {
  return oracle->methodCount() == ours->methodCount() && oracle->methodOffset() == ours->methodOffset() &&
         oracle->propertyCount() == ours->propertyCount() && oracle->enumeratorCount() == ours->enumeratorCount() &&
         oracle->classInfoCount() == ours->classInfoCount() && oracle->constructorCount() == ours->constructorCount();
}

}  // namespace parity
