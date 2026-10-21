// Proxy edge cases, filtering and re-sorting a flat model: rows that do not pass a filter being inserted, removed and
// edited, a row turning from rejected to accepted, the recursive-filtering switch, string comparison (case, locale),
// a filter or invalidate() while a batch is being announced, a proxy made before and after the data, persistent
// proxy indexes through a re-sort, and a sort by a role that a replace changes.
// QAbstractItemModelTester runs in fatal mode on the model and every proxy.
// Catalogue: docs/qt-edge-cases/proxies-selection.md.
#include "proxy_support.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtCore/QStringListModel>

#include <algorithm>
#include <optional>

namespace {
using namespace edge_proxy;
using MapRig = Rig<QMap<int, QString>>;
using HashRig = Rig<QHash<int, QString>>;
using ListRig = Rig<QList<QString>>;

// The structural signals of a model, counted.
struct Counts {
  int inserted = 0, removed = 0, moved = 0, layout = 0, reset = 0, data = 0;
  explicit Counts(QAbstractItemModel& m) {
    using M = QAbstractItemModel;
    QObject::connect(&m, &M::rowsInserted, &m, [this] { ++inserted; });
    QObject::connect(&m, &M::rowsRemoved, &m, [this] { ++removed; });
    QObject::connect(&m, &M::rowsMoved, &m, [this] { ++moved; });
    QObject::connect(&m, &M::layoutChanged, &m, [this] { ++layout; });
    QObject::connect(&m, &M::modelReset, &m, [this] { ++reset; });
    QObject::connect(&m, &M::dataChanged, &m, [this] { ++data; });
  }
  bool quiet() const { return inserted + removed + moved + layout + reset + data == 0; }
};

QStringList shown(const QAbstractItemModel& m) { return column(m); }

// The rows of the model that contain `text`, in model order.
QStringList matching(const QAbstractItemModel& m, const QString& text, Qt::CaseSensitivity cs = Qt::CaseSensitive) {
  QStringList out;
  for (const QString& s : column(m))
    if (s.contains(text, cs)) out.append(s);
  return out;
}

// a row that fails the filter is invisible to the proxy: inserting, editing and removing it makes no signal on
// the proxy; an edit that makes it pass adds a row, and an edit that makes a visible row fail removes it.
void hidden_rows_come_and_go() {
  MapRig rig;
  ProxyRig pr(rig.model.get());
  QSortFilterProxyModel& proxy = *pr.proxy;
  proxy.setFilterFixedString("a");
  CHECK(rig.source.insert(1, "ax") && rig.source.insert(2, "bx") && rig.source.insert(3, "ay"));
  CHECK(shown(proxy) == QStringList({"ax", "ay"}));
  Counts quiet(proxy);
  CHECK(rig.source.insert(4, "bz"));
  CHECK(rig.source.replace(4, "bq"));
  CHECK(rig.source.remove(2));
  CHECK(rig.source.removeMany({4}) == 1);
  CHECK(shown(proxy) == QStringList({"ax", "ay"}));
  CHECK(quiet.inserted == 0 && quiet.removed == 0 && quiet.layout == 0);

  CHECK(rig.source.insert(5, "bb"));
  CHECK(rig.source.replace(5, "now-a"));  // rejected -> accepted
  CHECK(shown(proxy) == QStringList({"ax", "ay", "now-a"}));
  CHECK(quiet.inserted == 1);
  CHECK(rig.source.editValue(1, [](QString& v) { v = "zz"; }));  // accepted -> rejected
  CHECK(shown(proxy) == QStringList({"ay", "now-a"}));
  CHECK(quiet.removed == 1);
  CHECK(mapping_is_consistent(proxy));
}

// with dynamicSortFilter off the proxy does not re-evaluate an edited row: an edit that would add a row or
// remove one leaves the proxy as it was, with the new data in the rows it keeps; invalidate() brings it up to date.
void a_static_proxy_waits_for_invalidate() {
  MapRig rig;
  ProxyRig pr(rig.model.get());
  QSortFilterProxyModel& proxy = *pr.proxy;
  proxy.setDynamicSortFilter(false);
  proxy.setFilterFixedString("a");
  CHECK(rig.source.insert(1, "ax") && rig.source.insert(2, "bx"));
  CHECK(rig.source.replace(2, "ba"));  // would be accepted
  CHECK(shown(proxy) == QStringList({"ax"}));
  CHECK(rig.source.replace(1, "zz"));  // would be rejected: the row stays, with its new text
  CHECK(shown(proxy) == QStringList({"zz"}));
  CHECK(mapping_is_consistent(proxy));
  proxy.invalidate();
  CHECK(shown(proxy) == QStringList({"ba"}));
}

// the roles an edit announces do not decide whether the proxy looks again: a change announced for a role that
// the filter does not read is re-filtered anyway (Qt 6.10), so the proxy follows the data.
void the_proxy_refilters_whatever_roles_were_announced() {
  MapRig rig;
  ProxyRig pr(rig.model.get());
  QSortFilterProxyModel& proxy = *pr.proxy;
  proxy.setFilterFixedString("a");
  CHECK(rig.source.insert(1, "ax") && rig.source.insert(2, "bx"));
  CHECK(rig.source.editValue(2, [](QString& v) { v = "ba"; }, {Qt::UserRole + 9}));
  CHECK(shown(proxy) == QStringList({"ax", "ba"}));
  CHECK(rig.source.editValue(2, [](QString& v) { v = "bq"; }, {Qt::UserRole}));
  CHECK(shown(proxy) == QStringList({"ax"}));
}

// recursive filtering is about children, and a flat model has none: switching it on changes nothing, through
// the same changes as a proxy that has it off.
template <class RigT>
void recursive_filtering_changes_nothing_in(RigT& rig) {
  ProxyRig plain(rig.model.get());
  ProxyRig recursive(rig.model.get());
  plain.proxy->setFilterFixedString("a");
  recursive.proxy->setFilterFixedString("a");
  recursive.proxy->setRecursiveFilteringEnabled(true);
  const auto same = [&] {
    CHECK(shown(*plain.proxy) == shown(*recursive.proxy));
    CHECK(mapping_is_consistent(*recursive.proxy));
  };
  CHECK(rig.source.insert(1, "ax") && rig.source.insert(2, "bx"));
  same();
  CHECK(rig.source.replace(2, "ba"));
  same();
  CHECK(rig.source.remove(1));
  same();
  rig.source.resetWith({});
  same();
  recursive.proxy->setRecursiveFilteringEnabled(false);
  same();
}

void recursive_filtering_is_irrelevant_for_flat_models() {
  MapRig map;
  recursive_filtering_changes_nothing_in(map);
  HashRig hash;
  recursive_filtering_changes_nothing_in(hash);
}

// filtering is case-insensitive or not as the proxy says, and sorting compares strings as the proxy says:
// case-sensitive by code unit, case-insensitive, and locale-aware (QString::localeAwareCompare in the default locale).
void string_comparison_follows_the_proxy() {
  const QStringList words{"b", "A", "a", "B", "ä", "z", "Z"};
  MapRig rig;
  ProxyRig pr(rig.model.get());
  QSortFilterProxyModel& proxy = *pr.proxy;
  int key = 0;
  for (const QString& w : words) CHECK(rig.source.insert(key++, w));

  proxy.setSortCaseSensitivity(Qt::CaseSensitive);
  proxy.setSortLocaleAware(false);
  proxy.sort(0);
  QStringList expected = words;
  std::sort(expected.begin(), expected.end(), [](const QString& a, const QString& b) { return a < b; });
  CHECK(shown(proxy) == expected);  // A B Z a b z ä

  proxy.setSortCaseSensitivity(Qt::CaseInsensitive);
  const QStringList folded = shown(proxy);
  for (int i = 1; i < folded.size(); ++i) CHECK(folded[i - 1].compare(folded[i], Qt::CaseInsensitive) <= 0);

  const QLocale saved;
  QLocale::setDefault(QLocale(QLocale::Swedish));
  proxy.setSortLocaleAware(true);
  const QStringList aware = shown(proxy);
  for (int i = 1; i < aware.size(); ++i) CHECK(QString::localeAwareCompare(aware[i - 1], aware[i]) <= 0 || aware[i - 1].compare(aware[i], Qt::CaseInsensitive) == 0);
  QLocale::setDefault(saved);
  proxy.setSortLocaleAware(false);
  CHECK(mapping_is_consistent(proxy));

  // The filter's case switch.
  proxy.setFilterFixedString("a");
  proxy.setFilterCaseSensitivity(Qt::CaseSensitive);
  CHECK(shown(proxy).size() == matching(*rig.model, "a", Qt::CaseSensitive).size());
  proxy.setFilterCaseSensitivity(Qt::CaseInsensitive);
  CHECK(shown(proxy).size() == matching(*rig.model, "a", Qt::CaseInsensitive).size());
  CHECK(shown(proxy).size() == 2 + 0 && shown(proxy).contains("A") && shown(proxy).contains("a"));
}

// a filter change or an invalidate() made from a slot of the model, between the runs of a batch (at the "done"
// signal of a run, when the model holds the rows announced so far and the container already holds all of them): the
// proxy ends up showing exactly the rows that pass.
enum class Reaction { filter, invalidate, regex, sortRole, descending };

void react(QSortFilterProxyModel& proxy, Reaction reaction) {
  switch (reaction) {
    case Reaction::filter: proxy.setFilterFixedString("1"); break;
    case Reaction::invalidate: proxy.invalidate(); break;
    case Reaction::regex: proxy.setFilterRegularExpression("1|2"); break;
    case Reaction::sortRole: proxy.setSortRole(kKeyRole); break;
    case Reaction::descending: proxy.sort(0, Qt::DescendingOrder); break;
  }
}

template <class Make>
void a_proxy_change_between_runs(Reaction reaction, bool onRemoval) {
  MapRig rig;
  ProxyRig pr(rig.model.get());
  QSortFilterProxyModel& proxy = *pr.proxy;
  proxy.setSortRole(Qt::DisplayRole);
  proxy.sort(0);
  for (int key : {20, 40, 60, 80, 100, 120}) CHECK(rig.source.insert(key, QString::number(key)));
  int reactions = 0;
  const auto once = [&] {
    if (reactions++ == 0) react(proxy, reaction);
  };
  if (onRemoval) {
    QObject::connect(rig.model.get(), &QAbstractItemModel::rowsRemoved, rig.model.get(), once);
    // Runs from the bottom: 120, then 80 and 60, then 20.
    CHECK(rig.source.removeMany({20, 60, 80, 120}) == 4);
  } else {
    QObject::connect(rig.model.get(), &QAbstractItemModel::rowsInserted, rig.model.get(), once);
    QMap<int, QString> batch;
    for (int key : {10, 30, 90, 110, 130}) batch.insert(key, QString::number(key));
    CHECK(rig.source.insertMany(batch) == 5);
  }
  CHECK(reactions >= 2);  // more than one run was announced
  if (reaction == Reaction::filter)
    CHECK(proxy.rowCount() == static_cast<int>(matching(*rig.model, "1").size()));
  else if (reaction != Reaction::regex)
    CHECK(proxy.rowCount() == rig.model->rowCount());
  CHECK(mapping_is_consistent(proxy));
  proxy.invalidate();
  CHECK(mapping_is_consistent(proxy));
}

void a_proxy_change_between_the_runs_of_a_batch() {
  for (const Reaction reaction : {Reaction::filter, Reaction::invalidate, Reaction::regex, Reaction::sortRole, Reaction::descending}) {
    a_proxy_change_between_runs<void>(reaction, false);
    a_proxy_change_between_runs<void>(reaction, true);
  }
}

// a filter change right after a batch, and after resetWith and clear, is the same as a filter on the final model.
void a_filter_after_a_batch() {
  MapRig rig;
  ProxyRig pr(rig.model.get());
  QSortFilterProxyModel& proxy = *pr.proxy;
  proxy.setFilterFixedString("1");
  QMap<int, QString> batch;
  for (int key : {10, 11, 12, 20, 21, 30}) batch.insert(key, QString::number(key));
  CHECK(rig.source.insertMany(batch) == 6);
  CHECK(shown(proxy) == QStringList({"10", "11", "12", "21"}));
  CHECK(rig.source.removeMany({10, 21, 30}) == 3);
  CHECK(shown(proxy) == QStringList({"11", "12"}));
  QMap<int, QString> content;
  content.insert(5, "15");
  content.insert(6, "26");
  rig.source.resetWith(content);
  CHECK(shown(proxy) == QStringList({"15"}));
  proxy.setFilterFixedString("");
  CHECK(shown(proxy) == QStringList({"15", "26"}));
  CHECK(rig.source.clear() && proxy.rowCount() == 0);
}

// a proxy made before the data and one made after it show the same rows, and keep showing the same rows.
void a_proxy_before_and_after_the_data() {
  MapRig rig;
  ProxyRig before(rig.model.get());
  before.proxy->setSortRole(Qt::DisplayRole);
  before.proxy->sort(0);
  CHECK(rig.source.insert(3, "c") && rig.source.insert(1, "a") && rig.source.insert(2, "b"));
  ProxyRig after(rig.model.get());
  after.proxy->setSortRole(Qt::DisplayRole);
  after.proxy->sort(0);
  const auto same = [&] {
    CHECK(shown(*before.proxy) == shown(*after.proxy));
    CHECK(mapping_is_consistent(*before.proxy) && mapping_is_consistent(*after.proxy));
  };
  same();
  CHECK(shown(*after.proxy) == QStringList({"a", "b", "c"}));
  CHECK(rig.source.replace(1, "d") && rig.source.insert(9, "0"));
  same();
  CHECK(rig.source.insertMany(QMap<int, QString>{{20, "m"}, {21, "A"}}) == 2);
  same();
  rig.source.resetWith(QMap<int, QString>{{1, "q"}});
  same();
  // A proxy over a model with rows, made last, sees them at once, sorted.
  ProxyRig late(rig.model.get());
  late.proxy->sort(0, Qt::DescendingOrder);
  CHECK(shown(*late.proxy) == QStringList({"q"}));
}

// a persistent index of the proxy follows its entry through a re-sort: an edit that moves the entry makes the
// proxy emit layoutChanged, and the index then has the entry's new row and still reads its data.
void persistent_proxy_indexes_through_a_resort() {
  MapRig rig;
  ProxyRig pr(rig.model.get());
  QSortFilterProxyModel& proxy = *pr.proxy;
  proxy.setSortRole(Qt::DisplayRole);
  proxy.sort(0);
  CHECK(rig.source.insert(1, "b") && rig.source.insert(2, "d") && rig.source.insert(3, "f"));
  QPersistentModelIndex tracked = proxy.index(0, 0);  // "b"
  QPersistentModelIndex last = proxy.index(2, 0);     // "f"
  Counts counts(proxy);
  CHECK(rig.source.replace(1, "z"));  // moves to the end
  CHECK(counts.layout >= 1);
  CHECK(tracked.isValid() && tracked.row() == 2 && tracked.data().toString() == "z");
  CHECK(last.isValid() && last.row() == 1 && last.data().toString() == "f");
  CHECK(shown(proxy) == QStringList({"d", "f", "z"}));
  CHECK(rig.source.remove(1));
  CHECK(!tracked.isValid() && last.isValid() && last.row() == 1);
  CHECK(rig.source.insert(7, "a") && last.row() == 2);  // an insert before it shifts it
  rig.source.resetWith({});
  CHECK(!last.isValid());
  // A key-role sort does not move an entry for a replace: nothing is announced but the data.
  MapRig again;
  ProxyRig byKey(again.model.get());
  byKey.proxy->setSortRole(kKeyRole);
  byKey.proxy->sort(0);
  CHECK(again.source.insert(2, "x") && again.source.insert(1, "y"));
  Counts quiet(*byKey.proxy);
  CHECK(again.source.replace(1, "zz"));
  CHECK(quiet.layout == 0 && quiet.inserted == 0 && quiet.removed == 0 && quiet.reset == 0 && quiet.data == 1);
  CHECK(shown(*byKey.proxy) == QStringList({"zz", "x"}));
}

// a list has no key role, so a proxy sorted by it compares empty values: every row ties. The proxy stays a
// consistent view (every row once), and a sort by the display role is unaffected by the key-role proxy next to it.
void a_list_sorted_by_the_key_role() {
  ListRig rig;
  ProxyRig byKey(rig.model.get());
  ProxyRig byText(rig.model.get());
  byKey.proxy->setSortRole(kKeyRole);
  byKey.proxy->sort(0);
  byText.proxy->sort(0);
  for (const QString s : {"c", "a", "b"}) CHECK(rig.source.insert(rig.source.size(), s));
  CHECK(rig.source.insert(0, "z") && rig.source.move(0, 3) && rig.source.remove(1));
  CHECK(rig.source.insertRange(1, QList<QString>{"m", "k"}));
  CHECK(mapping_is_consistent(*byKey.proxy) && mapping_is_consistent(*byText.proxy));
  CHECK(byKey.proxy->rowCount() == rig.model->rowCount());
  QStringList sorted = shown(*rig.model);
  sorted.sort();
  CHECK(shown(*byText.proxy) == sorted);
  CHECK(!byKey.proxy->index(0, 0).data(kKeyRole).isValid());
}

int g_warnings = 0;
void count_warnings(QtMsgType, const QMessageLogContext&, const QString& message) {
  if (message.contains("inconsistent changes")) ++g_warnings;
}

// a proxy change made from a slot at an "about to be removed" or "about to be inserted" signal of the model is
// outside what Qt allows (the source is mid-change), and Qt's own list model gets the same treatment: the container
// model ends with the same rows, the same proxy rows and the same warnings from QSortFilterProxyModel as
// QStringListModel does for the same removal.
void a_proxy_change_at_an_about_signal_is_as_with_qts_model() {
  const auto react_once = [](QSortFilterProxyModel& proxy, Reaction reaction) { react(proxy, reaction); };
  for (const Reaction reaction : {Reaction::filter, Reaction::invalidate, Reaction::descending}) {
    QStringList refRows, ourRows, refProxy, ourProxy;
    int refWarnings = 0, ourWarnings = 0;
    const QtMessageHandler previous = qInstallMessageHandler(count_warnings);
    {
      QStringListModel ref(QStringList{"1", "2", "3", "4", "5", "6"});
      QSortFilterProxyModel p;
      p.setSourceModel(&ref);
      int n = 0;
      QObject::connect(&ref, &QAbstractItemModel::rowsAboutToBeRemoved, &ref, [&] { if (n++ == 0) react_once(p, reaction); });
      g_warnings = 0;
      ref.removeRows(4, 1);
      ref.removeRows(1, 2);
      refWarnings = g_warnings;
      refRows = column(ref);
      refProxy = column(p);
    }
    {
      MapRig rig;
      for (int k = 1; k <= 6; ++k) CHECK(rig.source.insert(k, QString::number(k)));
      QSortFilterProxyModel p;
      p.setSourceModel(rig.model.get());
      int n = 0;
      QObject::connect(rig.model.get(), &QAbstractItemModel::rowsAboutToBeRemoved, rig.model.get(), [&] { if (n++ == 0) react_once(p, reaction); });
      g_warnings = 0;
      CHECK(rig.source.removeMany({2, 3, 5}) == 3);
      ourWarnings = g_warnings;
      ourRows = column(*rig.model);
      ourProxy = column(p);
      CHECK(mapping_is_consistent(p));
    }
    qInstallMessageHandler(previous);
    CHECK(ourRows == refRows);
    CHECK(ourProxy == refProxy);
    CHECK(ourWarnings == refWarnings);
  }
}

// an edit made through a proxy reaches the container: setData on a sorted and filtered proxy replaces the entry
// of the source row, the proxy re-sorts and re-filters, a refused edit (the editor says no) changes nothing, and the
// editable flag passes through.
void editing_through_a_proxy() {
  using C = QMap<int, QString>;
  observable::ObservableContainer<C> source;
  observable::ContainerModel<C> model(
      &source, [](const int&, const QString& v) { return QVariant(v); },
      [](const QString&, const QVariant& edited) -> std::optional<QString> {
        if (edited.toString().isEmpty()) return std::nullopt;
        return edited.toString();
      });
  QAbstractItemModelTester modelTester(&model, kFatal);
  QSortFilterProxyModel proxy;
  proxy.setSourceModel(&model);
  proxy.setFilterRegularExpression("^[a-m]");
  proxy.sort(0);
  QAbstractItemModelTester proxyTester(&proxy, kFatal);
  for (const auto& [key, text] : {std::pair{1, "pear"}, {2, "apple"}, {3, "kiwi"}, {4, "fig"}}) CHECK(source.insert(key, text));
  CHECK(column(proxy) == QStringList({"apple", "fig", "kiwi"}));
  CHECK(proxy.flags(proxy.index(0, 0)).testFlag(Qt::ItemIsEditable));

  QPersistentModelIndex kiwi = proxy.index(2, 0);
  CHECK(proxy.setData(proxy.index(0, 0), "zucchini"));  // apple -> out of the filter
  CHECK(source.value(2) == "zucchini" && column(proxy) == QStringList({"fig", "kiwi"}));
  CHECK(kiwi.isValid() && kiwi.row() == 1 && kiwi.data().toString() == "kiwi");
  CHECK(proxy.setData(proxy.index(1, 0), "ant"));  // kiwi -> re-sorted to the top
  CHECK(column(proxy) == QStringList({"ant", "fig"}) && kiwi.row() == 0 && kiwi.data().toString() == "ant");
  CHECK(!proxy.setData(proxy.index(0, 0), ""));  // refused by the editor
  CHECK(!proxy.setData(proxy.index(0, 0), "x", Qt::UserRole));  // another role
  CHECK(!proxy.setData(QModelIndex(), "x"));
  CHECK(column(proxy) == QStringList({"ant", "fig"}) && source.value(3) == "ant");
  CHECK(source.remove(3));
  CHECK(!kiwi.isValid() && column(proxy) == QStringList({"fig"}));
  CHECK(!proxy.setData(kiwi, "x"));
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  hidden_rows_come_and_go();
  a_static_proxy_waits_for_invalidate();
  the_proxy_refilters_whatever_roles_were_announced();
  recursive_filtering_is_irrelevant_for_flat_models();
  string_comparison_follows_the_proxy();
  a_proxy_change_between_the_runs_of_a_batch();
  a_filter_after_a_batch();
  editing_through_a_proxy();
  a_proxy_change_at_an_about_signal_is_as_with_qts_model();
  a_proxy_before_and_after_the_data();
  persistent_proxy_indexes_through_a_resort();
  a_list_sorted_by_the_key_role();
  return rqt_test::finish("capability_edge_proxy_filter");
}
