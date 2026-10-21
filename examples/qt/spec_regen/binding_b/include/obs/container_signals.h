// Binding B has no signal base class (SPEC 4.2): obs::Container declares its own typed signals. This header
// keeps only the listener shortcuts of SPEC 7.3, written against any source that has those signals.
#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <memory>
#include <utility>

namespace obs {

// f() runs once after EVERY structural change; never after a data change and never on an "about".
// One callable is shared by the six connections, so a stateful f sees every call.
template <class Source, class F>
void onStructureChanged(const Source * source, const QObject * context, F f)
{
    auto shared = std::make_shared<F>(std::move(f));
    auto call = [shared] { (*shared)(); };
    QObject::connect(source, &Source::entryAdded, context, call);
    QObject::connect(source, &Source::entryRemoved, context, call);
    QObject::connect(source, &Source::entriesAdded, context, call);
    QObject::connect(source, &Source::entriesRemoved, context, call);
    QObject::connect(source, &Source::cleared, context, call);
    QObject::connect(source, &Source::resetCompleted, context, call);
}

// f(const QList<Key> &) runs after entryAdded (one key) and after entriesAdded (the batch).
template <class Source, class F>
void onKeysAdded(const Source * source, const QObject * context, F f)
{
    using Key = typename Source::Key;
    auto shared = std::make_shared<F>(std::move(f));
    QObject::connect(source, &Source::entryAdded, context, [shared](const Key & key) { (*shared)(QList<Key>{key}); });
    QObject::connect(source, &Source::entriesAdded, context, [shared](const QList<Key> & keys) { (*shared)(keys); });
}

}  // namespace obs
