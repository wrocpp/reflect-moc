// Model protocol, part 1: QAbstractItemModelTester in fatal mode through every kind of change, the degenerate ones
// that must change nothing, and the order of the rows. Every Fixture carries a fatal tester, so a protocol
// violation aborts the process and fails the test. Catalogue rows: docs/qt-edge-cases/protocol.md (P-01 to P-04).
#include "protocol_support.hpp"

namespace {
using namespace edge;

template <class Fx>
void edit_with_roles(Fx& fx) {
  if constexpr (Fx::assignable) {
    const auto key = fx.key_of(3);
    if (key) fx.source.editValue(*key, [](auto& value) { value = Fx::from_int(kEdited); }, QList<int>{Qt::UserRole + 1});
  }
}

template <class Fx>
void batches(Fx& fx) {
  if constexpr (Fx::keyed) {
    const int first = fx.next;
    fx.source.insertMany(Fx::content({first + 3, first + 1, first + 5}));
    fx.next = first + 6;
    fx.source.removeMany({Fx::named(first + 1), Fx::named(first + 3), Fx::named(first + 3)});
    const typename Fx::Container taken = fx.source.takeMany({Fx::named(first + 5), Fx::named(kUnknownRole)});
    fx.source.insertMany(taken);
  }
}

template <class Fx>
void lists(Fx& fx) {
  if constexpr (Fx::indexed) {
    fx.source.insertRange(1, {Sample{40}, Sample{41}, Sample{42}});
    fx.source.removeRange(0, 2);
    fx.source.move(0, fx.source.size() - 1);
    fx.source.move(fx.source.size() - 1, 0);
    fx.source.move(1, 2);
  }
}

// P-01
void tester_fatal_through_every_kind_of_change() {
  for_each_model([](auto& fx, const std::string& kind) {
    SignalLog log(fx.model.get());
    fx.fill(kSmallFill);
    KCHECK(fx.rows() == kSmallFill);
    fx.replace(2, kEdited);
    fx.remove(4);
    edit_with_roles(fx);
    lists(fx);
    batches(fx);
    fx.source.resetWith(fx.content({7, 8, 9}));
    fx.source.resetWith(fx.content({}));
    fx.source.clear();
    fx.fill(2);
    fx.source.clear();
    // no change of this library is a layout change, and none touches a column
    KCHECK(log.count("layoutAboutToBeChanged") == 0 && log.count("layoutChanged") == 0);
    KCHECK(log.count("columns") == 0 && log.count("headerDataChanged") == 0);
    KCHECK(log.count("modelAboutToBeReset") == log.count("modelReset"));
  });
}

// What a request that cannot be honoured leaves behind: nothing, and no signal.
template <class Fx>
void degenerate_requests(Fx& fx, const std::string& kind) {
  const std::vector<int> before = fx.shown();
  const qsizetype size = fx.source.size();
  SignalLog log(fx.model.get());
  KCHECK(!fx.source.remove(Fx::named(kUnknownRole)));
  KCHECK(!fx.source.replace(Fx::named(kUnknownRole), Fx::from_int(1)));
  if (size == 0) KCHECK(!fx.source.clear());
  if constexpr (Fx::indexed) {
    KCHECK(!fx.source.insert(-1, Sample{1}));
    KCHECK(!fx.source.insert(size + 1, Sample{1}));
    KCHECK(!fx.source.insertRange(0, {}));
    KCHECK(!fx.source.insertRange(-1, {Sample{1}}));
    KCHECK(!fx.source.insertRange(size + 1, {Sample{1}}));
    KCHECK(!fx.source.removeRange(0, 0));
    KCHECK(!fx.source.removeRange(0, -1));
    KCHECK(!fx.source.removeRange(-1, 1));
    KCHECK(!fx.source.removeRange(size, 1));
    KCHECK(!fx.source.removeRange(0, size + 1));
    KCHECK(!fx.source.move(0, 0));
    KCHECK(!fx.source.move(-1, 0));
    KCHECK(!fx.source.move(0, size));
  } else if constexpr (Fx::multi) {
    KCHECK(!fx.source.insert(typename Fx::Key{1, 1}, Sample{1}));  // a new entry is occurrence 0
    KCHECK(!fx.source.insert(typename Fx::Key{1, -1}, Sample{1}));
  } else {
    if (size > 0) KCHECK(!fx.source.insert(Fx::named(1), Fx::from_int(1)));  // the key is there
  }
  if constexpr (Fx::keyed) {
    KCHECK(fx.source.insertMany(fx.content({})) == 0);
    if (size > 0) KCHECK(fx.source.insertMany(fx.content({1})) == 0);  // already there
    KCHECK(fx.source.removeMany({}) == 0);
    KCHECK(fx.source.removeMany({Fx::named(kUnknownRole)}) == 0);
    KCHECK(fx.source.takeMany({Fx::named(kUnknownRole)}).isEmpty());
    KCHECK(fx.source.takeMany({}).isEmpty());
  }
  KCHECK(log.silent());
  KCHECK(fx.shown() == before && fx.source.size() == size);
}

// P-02
void degenerate_requests_change_nothing() {
  for_each_model([](auto& fx, const std::string& kind) {
    degenerate_requests(fx, kind);   // on an empty container
    fx.fill(kSmallFill);
    degenerate_requests(fx, kind);   // and on a full one
  });
}

// P-03
void model_made_over_a_container_that_has_entries() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        KCHECK(fx.rows() == kSmallFill && fx.source.size() == kSmallFill);
        KCHECK(fx.shows_as_set({1, 2, 3, 4, 5}));
        SignalLog log(fx.model.get());
        fx.add();                                    // the next entry lands where an empty-start model would put it
        KCHECK(fx.rows() == kSmallFill + 1);
        KCHECK(fx.shows_as_set({1, 2, 3, 4, 5, 6}));
        KCHECK(fx.shown().back() == kSmallFill + 1);
        KCHECK(log.has("rowsInserted 5 5"));
        fx.remove(1);
        KCHECK(fx.shows_as_set({2, 3, 4, 5, 6}));
      },
      false, kSmallFill);
  for_each_model([](auto& fx, const std::string& kind) {  // and over an empty one
    KCHECK(fx.rows() == 0 && !fx.index(0).isValid());
    KCHECK(!fx.model->hasChildren());
  });
}

// P-04: where a new row lands, by container kind
void new_row_lands_where_the_container_orders_it() {
  for_each_model([](auto& fx, const std::string& kind) {
    using Fx = std::remove_cvref_t<decltype(fx)>;
    fx.fill(4);
    fx.remove(2);
    SignalLog log(fx.model.get());
    fx.add_named(0);
    if constexpr (Fx::sorted) {                       // QMap, QMultiMap: key order
      KCHECK(fx.shows({0, 1, 3, 4}));
      KCHECK(log.has("rowsInserted 0 0"));
    } else {                                          // a list appends, a hash or set lists in the order added
      KCHECK(fx.shows({1, 3, 4, 0}));
      KCHECK(log.has("rowsInserted 3 3"));
    }
  });
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  tester_fatal_through_every_kind_of_change();
  degenerate_requests_change_nothing();
  model_made_over_a_container_that_has_entries();
  new_row_lands_where_the_container_orders_it();
  return rqt_test::finish("capability_edge_protocol_tester");
}
