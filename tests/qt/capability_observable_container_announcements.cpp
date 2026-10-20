// Every change is announced before it happens and confirmed after it, and the container shows its old state
// at the announcement and its new state at the confirmation. A refused request announces nothing.
#include "check.hpp"

#include <container_model.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>

#include <utility>

namespace {
struct Sample {
  int value = 0;
};
constexpr int kFirst = 10;
constexpr int kSecond = 20;
constexpr int kThird = 30;
constexpr int kReplacement = 99;
}  // namespace

template <class C>
class Log {
 public:
  using Source = observable::ObservableContainer<C>;
  using Key = typename Source::Key;

  explicit Log(Source& source) : m_source(source) {
    const auto keyed = [this](const char* name) {
      return [this, name](const Key& key) { note(name, QString::number(static_cast<long long>(key))); };
    };
    connect(&Source::entryAboutToBeAdded, keyed("aboutToBeAdded"));
    connect(&Source::entryAdded, keyed("added"));
    connect(&Source::entryAboutToBeRemoved, keyed("aboutToBeRemoved"));
    connect(&Source::entryRemoved, keyed("removed"));
    connect(&Source::entryAboutToBeChanged, keyed("aboutToBeChanged"));
    connect(&Source::entryChanged, keyed("changed"));
    const auto moved = [this](const char* name) {
      return [this, name](const Key& from, const Key& to) {
        note(name, QString::number(static_cast<long long>(from)) + ">" + QString::number(static_cast<long long>(to)));
      };
    };
    connect(&Source::entryAboutToBeMoved, moved("aboutToBeMoved"));
    connect(&Source::entryMoved, moved("moved"));
    connect(&Source::aboutToBeCleared, [this] { note("aboutToBeCleared", ""); });
    connect(&Source::cleared, [this] { note("cleared", ""); });
  }

  QStringList take() { return std::exchange(m_lines, {}); }

 private:
  void connect(auto signal, auto&& handler) { QObject::connect(&m_source, signal, &m_source, handler); }
  // "<signal> <argument> size=<n> first=<value of the first entry or none>"
  void note(const char* name, const QString& argument) {
    m_lines.append(QStringLiteral("%1 %2 size=%3 first=%4")
                       .arg(QLatin1String(name), argument)
                       .arg(m_source.size())
                       .arg(first()));
  }
  QString first() const {
    if (m_source.size() == 0) return QStringLiteral("none");
    if constexpr (observable::ContainerTraits<C>::order == observable::Order::indexed)
      return QString::number(m_source.value(0).value);
    else
      return QStringLiteral("-");
  }

  Source& m_source;
  QStringList m_lines;
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  observable::ObservableContainer<QList<Sample>> list;
  Log<QList<Sample>> log(list);

  CHECK(list.insert(0, Sample{kFirst}));
  CHECK(log.take() == QStringList({"aboutToBeAdded 0 size=0 first=none", "added 0 size=1 first=10"}));

  CHECK(list.insert(1, Sample{kSecond}));
  CHECK(list.insert(2, Sample{kThird}));
  log.take();

  CHECK(list.replace(0, Sample{kReplacement}));
  CHECK(log.take() == QStringList({"aboutToBeChanged 0 size=3 first=10", "changed 0 size=3 first=99"}));

  CHECK(list.move(0, 2));
  CHECK(log.take() == QStringList({"aboutToBeMoved 0>2 size=3 first=99", "moved 0>2 size=3 first=20"}));

  CHECK(list.remove(0));
  CHECK(log.take() == QStringList({"aboutToBeRemoved 0 size=3 first=20", "removed 0 size=2 first=30"}));

  CHECK(list.clear());
  CHECK(log.take() == QStringList({"aboutToBeCleared  size=2 first=30", "cleared  size=0 first=none"}));

  // Refused requests change nothing and announce nothing.
  CHECK(!list.clear());
  CHECK(!list.remove(0));
  CHECK(!list.replace(0, Sample{kFirst}));
  CHECK(!list.move(0, 1));
  CHECK(!list.insert(1, Sample{kFirst}));
  CHECK(!list.insert(-1, Sample{kFirst}));
  CHECK(list.insert(0, Sample{kFirst}));
  log.take();
  CHECK(!list.move(0, 0));
  CHECK(!list.move(0, 1));
  CHECK(!list.replace(1, Sample{kFirst}));
  CHECK(log.take().isEmpty());
  CHECK(list.size() == 1);

  observable::ObservableContainer<QMap<int, Sample>> map;
  Log<QMap<int, Sample>> maplog(map);
  CHECK(map.insert(kSecond, Sample{kSecond}));
  CHECK(!map.insert(kSecond, Sample{kThird}));
  CHECK(!map.move(kSecond, kFirst));
  CHECK(maplog.take() == QStringList({"aboutToBeAdded 20 size=0 first=none", "added 20 size=1 first=-"}));
  CHECK(map.value(kSecond).value == kSecond);

  observable::ObservableContainer<QSet<unsigned>> set;
  CHECK(set.insert(1));
  CHECK(!set.insert(1));
  CHECK(!set.replace(1, 1));

  return rqt_test::finish("capability_observable_container_announcements");
}
