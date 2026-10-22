// Pinned: value(key) returns a reference into the container, and it does not follow the entry. While a copy of
// items() is held (a snapshot for a view, a worker or an undo step), the next change detaches the container: it
// copies its storage, and the reference from before points into the snapshot, which keeps the old value. The
// model reads by key at each call and is not affected; a caller that keeps the reference is.
// The test compares addresses and reads the old value through a reference that the snapshot keeps alive; it never
// dereferences freed memory. items() itself returns the member by reference, so its address is stable and its
// content is whatever the latest change made it.
// A fix is a documented contract rather than code; this stays a limit_ test and says "valid until the next change".
// Catalogue: docs/qt-edge-cases/data-types.md (D-27).
#include "data_support.hpp"

#include <QtCore/QString>

namespace {
using observable::ObservableContainer;

void a_reference_is_not_followed_across_a_detach() {
  ObservableContainer<QMap<int, QString>> map;
  CHECK(map.insert(1, QString("old")));
  const QString* before = &map.value(1);
  const QString* beforeStable = &map.value(1);
  CHECK(map.insert(2, QString("other")));
  // Nothing shares the storage: the node stays where it is (a QMap node does not move on an insert).
  CHECK(&map.value(1) == beforeStable);

  const QMap<int, QString> snapshot = map.items();
  CHECK(map.replace(1, QString("new")));
  CHECK(&map.value(1) != before);        // the container moved to its own copy
  CHECK(*before == "old");               // the old reference reads the snapshot's value
  CHECK(map.value(1) == "new" && snapshot.value(1) == "old");

  ObservableContainer<QList<QString>> list;
  CHECK(list.insert(0, QString("a")));
  const QString* listBefore = &list.value(0);
  const QList<QString> listSnapshot = list.items();
  CHECK(list.replace(0, QString("b")));
  CHECK(&list.value(0) != listBefore);
  CHECK(*listBefore == "a" && list.value(0) == "b" && listSnapshot.at(0) == "a");

  // items() is one object for the container's life.
  const QMap<int, QString>* items = &map.items();
  map.resetWith({});
  CHECK(&map.items() == items && map.items().isEmpty());
}
}  // namespace

int main() {
  a_reference_is_not_followed_across_a_detach();
  return rqt_test::finish("limit_edge_data_value_reference");
}
