// SPEC 12.1 and 12.2: traits, concepts and utilities, all at compile time apart from the runs and validIndex.
#include <obs/traits.h>
#include <sel/helpers.h>
#include <sel/roles.h>
#include <sel/tracker.h>

#include <QtTest/QTest>

#include <reflect_moc/qt/qt.hpp>

#include <array>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace {

enum class E : int { A = Qt::UserRole + 10, B, C };

constexpr std::array<int, 2> kValid{Qt::UserRole + 1, Qt::UserRole + 2};
constexpr std::array<int, 1> kBuiltIn{Qt::DisplayRole};
constexpr std::array<int, 1> kUserRoleItself{Qt::UserRole};
constexpr std::array<int, 2> kDuplicate{Qt::UserRole + 1, Qt::UserRole + 1};
constexpr std::array<int, 0> kEmpty{};
constexpr std::array<E, 3> kEnums{E::A, E::C, E::B};
constexpr std::array<E, 2> kEnumDuplicate{E::A, E::A};
constexpr int kScalar = Qt::UserRole + 1;

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
// SPEC 6.1: rejected as well
static_assert(!obs::AssociativeContainer<std::set<int>>);
static_assert(!obs::AssociativeContainer<std::unordered_map<int, int>>);
static_assert(!obs::SupportedContainer<QSet<int>>);
static_assert(obs::SupportedContainer<const QList<int> &>);

// AT-1.3
static_assert(std::is_same_v<obs::KeyOf<QList<double>>, qsizetype>);
static_assert(std::is_same_v<obs::KeyOf<QMap<QString, int>>, QString>);
static_assert(std::is_same_v<obs::ValueOf<QMap<QString, int>>, int>);
static_assert(std::is_same_v<obs::ValueOf<std::vector<char>>, char>);

// AT-1.4
static_assert(sel::ValidRoleList<kValid>);
static_assert(sel::ValidRoleList<kEnums>);
static_assert(!sel::ValidRoleList<kBuiltIn>);
static_assert(!sel::ValidRoleList<kUserRoleItself>);
static_assert(!sel::ValidRoleList<kDuplicate>);
static_assert(!sel::ValidRoleList<kEmpty>);
static_assert(!sel::ValidRoleList<kScalar>);
static_assert(!sel::ValidRoleList<kEnumDuplicate>);
static_assert(sel::is_std_array_v<std::array<int, 2>>);
static_assert(sel::is_std_array_v<const std::array<int, 2> &>);
static_assert(!sel::is_std_array_v<int>);
static_assert(!sel::is_std_array_v<int[2]>);
static_assert(!sel::is_std_array_v<QList<int>>);
static_assert(!sel::is_std_array_v<std::vector<int>>);

// AT-1.5
template <auto R>
concept SingleTrackerNamesAType = requires { typename sel::Tracker<sel::Mode::Single, R>; };
static_assert(SingleTrackerNamesAType<kValid>);
static_assert(!SingleTrackerNamesAType<kDuplicate>);

// AT-1.6
static_assert(sel::Tracker<sel::Mode::Single, kValid>::selectedRole() == Qt::UserRole + 3);
static_assert(sel::Tracker<sel::Mode::Multi, kEnums>::selectedRole() == Qt::UserRole + 13);

// AT-1.7
static_assert(sel::IsTracker<sel::Tracker<sel::Mode::Single, kValid>>);
static_assert(sel::IsTracker<sel::Tracker<sel::Mode::Multi, kValid>>);
static_assert(!sel::IsTracker<std::vector<char>>);
static_assert(!sel::IsTracker<std::optional<int>>);
static_assert(!sel::IsTracker<int>);

// AT-1.8. Under binding B the VariantKey concept does not exist (SPEC 4.2), so only the HashableKey half applies.
static_assert(obs::HashableKey<QString>);
static_assert(obs::HashableKey<int>);
static_assert(obs::HashableKey<quint32>);

}  // namespace

class TstTraits : public QObject {
    RQT_OBJECT

private slots:
    [[= rqt::slot]] void at_1_1_to_1_8_compileTimeChecksAreInTheTranslationUnit() { QVERIFY(true); }

    [[= rqt::slot]] void at_2_1_contiguousRuns()
    {
        using Runs = std::vector<std::pair<int, int>>;
        const std::vector<int> many{0, 1, 2, 5, 7, 8};
        QVERIFY((obs::contiguousRuns(many.begin(), many.end()) == Runs{{0, 2}, {5, 5}, {7, 8}}));
        const std::vector<int> none;
        QVERIFY(obs::contiguousRuns(none.begin(), none.end()).empty());
        const std::vector<int> one{4};
        QVERIFY((obs::contiguousRuns(one.begin(), one.end()) == Runs{{4, 4}}));
    }

    [[= rqt::slot]] void at_2_2_validIndex()
    {
        const std::vector<int> v{1, 2, 3};
        QVERIFY(obs::validIndex(v, 0));
        QVERIFY(obs::validIndex(v, std::size_t{2}));
        QVERIFY(!obs::validIndex(v, 3));
        QVERIFY(!obs::validIndex(v, -1));
        QVERIFY(!obs::validIndex(v, std::size_t{3}));
        QVERIFY(!obs::validIndex(v, qsizetype{-1}));
    }
};

QTEST_GUILESS_MAIN(TstTraits)
