#include "obs/traits.h"
#include "sel/helpers.h"
#include "sel/roles.h"
#include "sel/tracker.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QtTest>

#include <array>
#include <map>
#include <optional>
#include <vector>

namespace {

enum class E : int { A = Qt::UserRole + 10, B, C };

constexpr std::array<int, 2> kGoodInts{Qt::UserRole + 1, Qt::UserRole + 2};
constexpr std::array<int, 2> kDuplicated{Qt::UserRole + 1, Qt::UserRole + 1};
constexpr std::array<int, 1> kBuiltIn{Qt::DisplayRole};
constexpr std::array<int, 1> kUserRoleItself{Qt::UserRole};
constexpr std::array<int, 0> kEmpty{};
constexpr std::array<E, 3> kEnums{E::A, E::C, E::B};

template <auto R>
concept CanTrack = requires { typename sel::Tracker<sel::Mode::Single, R>; };

// AT-1.1
static_assert(obs::SequentialContainer<QList<int>>);
static_assert(obs::SequentialContainer<std::vector<QString>>);
static_assert(obs::SequentialContainer<QStringList>);
static_assert(!obs::SequentialContainer<QHash<int, int>>);
static_assert(!obs::SequentialContainer<QSet<int>>);
static_assert(!obs::SequentialContainer<std::array<int, 3>>);
static_assert(!obs::SequentialContainer<int>);

// AT-1.2
static_assert(obs::AssociativeContainer<QHash<int, QString>>);
static_assert(obs::AssociativeContainer<QMap<QString, double>>);
static_assert(obs::AssociativeContainer<std::map<int, int>>);
static_assert(!obs::AssociativeContainer<QMultiHash<int, int>>);
static_assert(!obs::AssociativeContainer<QMultiMap<int, int>>);
static_assert(!obs::AssociativeContainer<std::multimap<int, int>>);
static_assert(!obs::AssociativeContainer<QSet<int>>);
static_assert(!obs::AssociativeContainer<QList<int>>);

// AT-1.3
static_assert(std::is_same_v<obs::KeyOf<QList<double>>, qsizetype>);
static_assert(std::is_same_v<obs::KeyOf<QMap<QString, int>>, QString>);
static_assert(std::is_same_v<obs::ValueOf<QMap<QString, int>>, int>);
static_assert(std::is_same_v<obs::ValueOf<std::vector<char>>, char>);

// AT-1.4
static_assert(sel::ValidRoleList<kGoodInts>);
static_assert(sel::ValidRoleList<kEnums>);
static_assert(!sel::ValidRoleList<kBuiltIn>);
static_assert(!sel::ValidRoleList<kUserRoleItself>);
static_assert(!sel::ValidRoleList<kDuplicated>);
static_assert(!sel::ValidRoleList<kEmpty>);
static_assert(!sel::ValidRoleList<Qt::UserRole + 1>);

// AT-1.5
static_assert(CanTrack<kGoodInts>);
static_assert(!CanTrack<kDuplicated>);

// AT-1.6
static_assert(sel::Tracker<sel::Mode::Single, kGoodInts>::selectedRole() == Qt::UserRole + 3);
static_assert(sel::Tracker<sel::Mode::Multi, kEnums>::selectedRole() == Qt::UserRole + 13);

// AT-1.7
static_assert(sel::IsTracker<sel::Tracker<sel::Mode::Single, kGoodInts>>);
static_assert(sel::IsTracker<sel::Tracker<sel::Mode::Multi, kGoodInts>>);
static_assert(!sel::IsTracker<std::vector<char>>);
static_assert(!sel::IsTracker<std::optional<int>>);
static_assert(!sel::IsTracker<int>);

// AT-1.8
static_assert(obs::HashableKey<QString>);
static_assert(obs::HashableKey<int>);
static_assert(obs::VariantKey<QString>);

// supporting facts for AT-1.4
static_assert(sel::is_std_array_v<std::array<int, 2>>);
static_assert(sel::is_std_array_v<const std::array<int, 2> &>);
static_assert(!sel::is_std_array_v<int>);
static_assert(!sel::is_std_array_v<int[2]>);
static_assert(!sel::is_std_array_v<QList<int>>);
static_assert(!sel::is_std_array_v<std::vector<int>>);

}  // namespace

class tst_traits : public QObject {
    Q_OBJECT
private slots:
    void at_1_1_sequentialContainer() { QVERIFY(obs::SequentialContainer<QList<int>>); }
    void at_1_2_associativeContainer() { QVERIFY((!obs::AssociativeContainer<QMultiHash<int, int>>)); }
    void at_1_3_keyAndValueOf() { QVERIFY((std::is_same_v<obs::KeyOf<QList<double>>, qsizetype>)); }
    void at_1_4_validRoleList() { QVERIFY(sel::ValidRoleList<kEnums>); }
    void at_1_5_trackerRejectsDuplicates() { QVERIFY(!CanTrack<kDuplicated>); }

    void at_1_6_selectedRole()
    {
        QCOMPARE((sel::Tracker<sel::Mode::Single, kGoodInts>::selectedRole()), int(Qt::UserRole) + 3);
        QCOMPARE((sel::Tracker<sel::Mode::Multi, kEnums>::selectedRole()), int(Qt::UserRole) + 13);
    }

    void at_1_7_isTracker() { QVERIFY((sel::IsTracker<sel::Tracker<sel::Mode::Multi, kGoodInts>>)); }
    void at_1_8_hashableAndVariantKeys() { QVERIFY((obs::HashableKey<int> && obs::VariantKey<QString>)); }

    void at_2_1_contiguousRuns()
    {
        using Runs = std::vector<std::pair<int, int>>;
        const std::vector<int> many{0, 1, 2, 5, 7, 8};
        QVERIFY((obs::contiguousRuns(many.begin(), many.end()) == Runs{{0, 2}, {5, 5}, {7, 8}}));
        const std::vector<int> none;
        QVERIFY(obs::contiguousRuns(none.begin(), none.end()).empty());
        const std::vector<int> one{4};
        QVERIFY((obs::contiguousRuns(one.begin(), one.end()) == Runs{{4, 4}}));
    }

    void at_2_2_validIndex()
    {
        const std::vector<int> v{1, 2, 3};
        QVERIFY(obs::validIndex(v, 0));
        QVERIFY(obs::validIndex(v, std::size_t{2}));
        QVERIFY(!obs::validIndex(v, 3));
        QVERIFY(!obs::validIndex(v, -1));
    }
};

QTEST_APPLESS_MAIN(tst_traits)
#include "tst_traits.moc"
