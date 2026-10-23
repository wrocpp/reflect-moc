# Binding B regeneration: report (agent text; host re-run pending at time of writing)

GCC 16.2, Qt 6.10.3, reflect-moc include dir (not SYSTEM), `-std=c++26 -freflection`, `-Wall -Wextra -Wpedantic`: 0 warnings.
ctest 17/17. No moc input: no Q_OBJECT, AUTOMOC OFF. Test classes use RQT_OBJECT with `private slots:` + `[[= rqt::slot]]`.
Example models (10.7) are moc-free too (RQT_OBJECT, RQT_PROPERTY, `[[= rqt::invokable]]`, `rqt::signal<void()>` host signals).
All AT ids passed. AT-1.8: the HashableKey half passes; the VariantKey half does not exist under B (4.2).
Mutation spot check by the agent (5 hand-made mutants): 4 killed; a swap of the about notification and the write in `Container::replace` survived, so the agent added `at_3_6_replaceWritesBetweenTheAboutAndTheTwin`, which kills it.

## Ambiguities and readings (B-specific unless noted)
1. 10.7 gives no constructors for the example models: `ColorListModel(QObject* parent = nullptr)`, empty; tests fill with `resetWith`.
2. 7.3/15.2: `onStructureChanged`/`onKeysAdded` take `const ContainerSignals*`, absent in B: `template <class Source, class F>`; `onKeysAdded` passes `const QList<Key>&`; `f` moved once into a shared_ptr shared by the connections.
3. 14: `container_signals.h` kept but holds only the two shortcuts; `matrix_signals.h` and `src/obs_signals.cpp` dropped; `obs_kit` is INTERFACE.
4. 8.1 `requires HashableKey` vs 15.4: `static_assert` in the constructor body.
5. checkIndex options `IndexIsValid | ParentIsInvalid`.
6. Same readings as A for 9.2 `clear()`, 9.4 order, NaN.
7. 7.2 batch key de-dup: QHash if hashable, linear `==` scan otherwise (std::map keys need not be hashable).
8. 10.5 `selectedValue`: a non-const `Container*` would pick the reference overload with `Container = X*`; added `requires (!std::is_pointer_v<Container>)`.
9. 14 vs 10.5/6.4: `sel/helpers.h` includes `obs/traits.h` for `contiguousRuns`.

## Spec places that do not apply to B as written
- 6.1 VariantKey and its uses (9, 15.3, 15.6).
- 7.3, 15.2: `const ContainerSignals*` parameters; `QList<QVariant>` in onKeysAdded.
- 14, G4: `matrix_signals.h`, `src/obs_signals.cpp`, AUTOMOC static `obs_kit`, "binding A, Q_OBJECT".
- 15.2/15.3/15.5/15.6: `ContainerSignals`/`MatrixSignals` and `Container : public ContainerSignals`.
- 10.6 item 2, 10.7: Q_OBJECT/Q_PROPERTY/Q_INVOKABLE/`signals:` become RQT_OBJECT/RQT_PROPERTY/`[[= rqt::invokable]]`/`rqt::signal` members.
- 12.1 AT-1.8 VariantKey half. 4.3: key payloads come back wrapped in QVariant in QSignalSpy.
- 16: B also needs the reflect-moc API.

## What reflect-moc made hard (nothing impossible)
- `#include <QtTest/QtTest>` fails under `-std=c++26` on this image before any reflect-moc header: `QtCore/q26numeric.h:34:12: error: 'saturate_cast' has not been declared in 'std'`. Use the per-class QtTest headers. Qt 6.10.3 against this libstdc++, not reflect-moc, but reflect-moc forces C++26.
- QtTest silently skips a test slot that is not annotated; the agent cross-checked the executable totals against the number of annotated functions.
- Size (aarch64): each non-static signal 8 bytes plus 8 for the anchor. `sizeof(QObject)` 16, `Container<QList<int>>` 160, `Container<QHash<QString,int>>` 144, `Matrix<QString,QString,double>` 336 (26 signals).
- Not tried: `rqt::static_signal`, emission cost, key types beyond int/quint32/qsizetype/QString, queued connections, threads, QML with typed keys.
- `RQT_OBJECT` must come first in the class. A derived Store with its own RQT_OBJECT and signal works.
- Unset `CMAKE_BUILD_TYPE` is made Debug so `Q_ASSERT` preconditions are live.

## Possible design mistakes in the spec (agent's opinion)
- 10.7 gives no constructor for the example models while 10.2 says a Multi tracker must be told about every row.
- 7.2 insertMany payload order is the iteration order of `entries`, unspecified for QHash.
- AT-1.8 and the AT/T sections are written for A; a short B table would save the translation.
- 14 says `sel_kit` does not depend on `obs_kit`, but 10.5 needs `contiguousRuns` from `obs` (6.4).
