// Data edge cases, keys: what a QMap, QHash or QSet treats as one key, and whether the model's row order and key
// role agree with the container for it. The model keeps its own sorted copy of the keys of a map (std::lower_bound
// with operator<) and an insertion-order index over a QHash, so every key type is checked for rows in the
// container's order and for the round trip QVariant::fromValue(key).value<Key>().
// Catalogue: docs/qt-edge-cases/data-types.md (D-01 to D-15).
#include "data_support.hpp"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QTimeZone>
#include <QtCore/QUrl>

#include <cmath>
#include <limits>

namespace {
using edge_data::Rig;

// A key with operator<, operator== and qHash, and nothing registered with Q_DECLARE_METATYPE.
struct Point {
  int x = 0;
  int y = 0;
  friend bool operator<(const Point& a, const Point& b) { return a.x != b.x ? a.x < b.x : a.y < b.y; }
  friend bool operator==(const Point&, const Point&) = default;
};
size_t qHash(const Point& p, size_t seed = 0) { return qHashMulti(seed, p.x, p.y); }

// Every key hashes to the same bucket: lookups walk a chain, the answers are the same.
struct Colliding {
  int id = 0;
  friend bool operator<(const Colliding& a, const Colliding& b) { return a.id < b.id; }
  friend bool operator==(const Colliding&, const Colliding&) = default;
};
size_t qHash(const Colliding&, size_t seed = 0) { return seed; }

// A key without a default constructor: the traits and the index never default-construct a key.
struct Fixed {
  explicit Fixed(int v) : v(v) {}
  int v;
  friend bool operator<(const Fixed& a, const Fixed& b) { return a.v < b.v; }
  friend bool operator==(const Fixed&, const Fixed&) = default;
};
size_t qHash(const Fixed& k, size_t seed = 0) { return ::qHash(k.v, seed); }

const double kNaN = std::numeric_limits<double>::quiet_NaN();

// D-01 and D-02: -0.0 and 0.0 compare equal, so they are one key in a QMap (std::less) and in a QHash (qHash maps
// both to one hash). The key that was inserted first stays.
void negative_zero_is_zero() {
  Rig<QMap<double, int>> map;
  CHECK(map.source.insert(-0.0, 1));
  CHECK(!map.source.insert(0.0, 2));
  CHECK(map.source.size() == 1 && map.model.rowCount() == 1);
  CHECK(std::signbit(map.keys().first()));
  CHECK(map.source.contains(0.0) && map.source.contains(-0.0));
  CHECK(map.source.replace(0.0, 3) && map.source.value(-0.0) == 3);
  CHECK(map.source.remove(0.0) && map.model.rowCount() == 0);

  Rig<QHash<double, int>> hash;
  CHECK(hash.source.insert(0.0, 1));
  CHECK(!hash.source.insert(-0.0, 2));
  CHECK(hash.source.size() == 1 && hash.model.rowCount() == 1);
  CHECK(!std::signbit(hash.keys().first()));
  CHECK(hash.source.remove(-0.0) && hash.model.rowCount() == 0);
}

// D-05: a float key is compared exactly: the same float is one key, a double that rounds to it is the same float.
void float_keys_compare_exactly() {
  Rig<QMap<float, int>> map;
  CHECK(map.source.insert(0.1f, 1));
  CHECK(!map.source.insert(static_cast<float>(0.1), 2));
  CHECK(map.source.insert(std::nextafter(0.1f, 1.0f), 3));  // the next float is another key
  CHECK(map.keys() == QList<float>({0.1f, std::nextafter(0.1f, 1.0f)}));
  Rig<QHash<float, int>> hash;
  CHECK(hash.source.insert(-0.0f, 1) && !hash.source.insert(0.0f, 2));
  CHECK(hash.source.insert(0.5f, 3) && hash.model.rowCount() == 2);
  CHECK(hash.keys() == QList<float>({-0.0f, 0.5f}));
}

// D-06: in Qt 6 a null QString equals an empty one, so they are one key; the first one inserted stays.
void null_and_empty_string_are_one_key() {
  Rig<QMap<QString, int>> map;
  CHECK(map.source.insert(QString(), 1));
  CHECK(!map.source.insert(QString(""), 2));
  CHECK(map.source.contains(QString("")) && map.source.contains(QString()));
  CHECK(map.model.rowCount() == 1);
  const QVariant key = map.model.index(0, 0).data(Qt::UserRole);
  CHECK(key.metaType() == QMetaType::fromType<QString>());
  CHECK(key.value<QString>().isNull());

  Rig<QHash<QString, int>> hash;
  CHECK(hash.source.insert(QString(""), 1));
  CHECK(!hash.source.insert(QString(), 2));
  CHECK(!hash.keys().first().isNull() && hash.keys().first().isEmpty());
  Rig<QSet<QString>> set;
  CHECK(set.source.insert(QString()) && !set.source.insert(QString("")));
  CHECK(set.model.rowCount() == 1);
}

QString u(const char16_t* text, qsizetype size) { return QString::fromUtf16(text, size); }

// D-07: a QMap<QString> orders by UTF-16 code unit (QString::operator<), not by code point, not by case folding and not
// by locale; there is no normalisation, so a precomposed letter and its combining form are two keys. The model's
// sorted copy uses the same operator<, so its rows follow QMap::keys() whatever the default locale is.
void unicode_keys_follow_the_code_unit_order() {
  const QString precomposed = u(u"é", 1);
  const QString combining = u(u"é", 2);
  const QString astral = u(u"\U00010000", 2);  // U+10000 is the surrogate pair D800 DC00
  const QString fullwidthTilde = u(u"～", 1);
  const QStringList keys{"z", "a", "A", "B", precomposed, combining, astral, fullwidthTilde, "", "ab"};
  const QLocale saved;
  for (const QLocale::Language language : {QLocale::C, QLocale::Swedish, QLocale::Polish}) {
    QLocale::setDefault(QLocale(language));
    Rig<QMap<QString, int>> rig;
    for (const QString& key : keys) CHECK(rig.source.insert(key, 0));
    CHECK(rig.source.size() == keys.size());  // nothing folded together
    CHECK(rig.keys() == rig.source.items().keys());
    QStringList sorted = keys;
    std::sort(sorted.begin(), sorted.end(), [](const QString& a, const QString& b) { return a.compare(b) < 0; });
    CHECK(rig.keys() == QList<QString>(sorted.begin(), sorted.end()));
    // The surrogate pair sorts before U+FF5E, the reverse of the code point order.
    CHECK(rig.keys().indexOf(astral) < rig.keys().indexOf(fullwidthTilde));
    // 'e' + U+0301 sorts before U+00E9: it starts with the smaller unit.
    CHECK(rig.keys().indexOf(combining) < rig.keys().indexOf(precomposed));
    CHECK(rig.keys().indexOf(QString("B")) < rig.keys().indexOf(QString("a")));
  }
  QLocale::setDefault(saved);
}

// D-08: a key with an embedded NUL keeps its size in comparison and in the hash.
void embedded_nul_keys() {
  const QString withNul = u(u"a\0b", 3);
  Rig<QMap<QString, int>> map;
  CHECK(map.source.insert(QString("a"), 1) && map.source.insert(withNul, 2) && map.source.insert(u(u"a\0", 2), 3));
  CHECK(map.model.rowCount() == 3);
  CHECK(map.keys() == map.source.items().keys());
  CHECK(map.keys().first() == QString("a") && map.keys().last() == withNul);
  Rig<QHash<QString, int>> hash;
  CHECK(hash.source.insert(QString("a"), 1) && hash.source.insert(withNul, 2));
  CHECK(hash.source.remove(withNul) && hash.keys() == QList<QString>({QString("a")}));

  const QByteArray bytes("a\0b", 3);
  Rig<QMap<QByteArray, int>> byteMap;
  CHECK(byteMap.source.insert(QByteArray("a"), 1) && byteMap.source.insert(bytes, 2) && byteMap.source.insert(QByteArray("a\0", 2), 3));
  CHECK(byteMap.keys() == byteMap.source.items().keys());
  CHECK(byteMap.keys().last() == bytes && byteMap.keys().last().size() == 3);
}

// D-09: a QByteArray key; a null and an empty array are equal.
void byte_array_keys() {
  Rig<QMap<QByteArray, int>> map;
  for (const char* text : {"b", "a", "ab", "\xff", ""}) CHECK(map.source.insert(QByteArray(text), 0));
  CHECK(!map.source.insert(QByteArray(), 1));  // null equals empty
  CHECK(map.keys() == map.source.items().keys());
  CHECK(map.keys().last() == QByteArray("\xff"));  // compared as unsigned bytes
  Rig<QHash<QByteArray, int>> hash;
  CHECK(hash.source.insert(QByteArray("x"), 1) && hash.source.insert(QByteArray("y"), 2));
  CHECK(hash.keys() == QList<QByteArray>({QByteArray("x"), QByteArray("y")}));
}

// D-10: a QDateTime key is the instant: the same instant in two time zones is one key, every invalid value is one
// key, and the key role returns the one that was inserted first, with its zone.
void date_time_keys_are_instants() {
  const QDateTime utc(QDate(2026, 1, 1), QTime(12, 0), QTimeZone::UTC);
  const QDateTime oslo = utc.toTimeZone(QTimeZone("Europe/Oslo"));
  CHECK(utc == oslo && utc.timeZone() != oslo.timeZone());
  Rig<QMap<QDateTime, int>> map;
  CHECK(map.source.insert(oslo, 1));
  CHECK(!map.source.insert(utc, 2));
  CHECK(map.source.insert(QDateTime(), 3) && !map.source.insert(QDateTime(), 4));
  CHECK(map.source.insert(utc.addSecs(1), 5));
  CHECK(map.model.rowCount() == 3);
  CHECK(map.keys() == map.source.items().keys());
  CHECK(!map.keys().first().isValid());  // an invalid value sorts first
  CHECK(map.keys().at(1).timeZone() == oslo.timeZone());

  Rig<QHash<QDateTime, int>> hash;
  CHECK(hash.source.insert(utc, 1) && !hash.source.insert(oslo, 2));
  CHECK(hash.source.insert(QDateTime(), 3) && !hash.source.insert(QDateTime(), 4));
  CHECK(hash.model.rowCount() == 2 && hash.source.remove(oslo) && hash.model.rowCount() == 1);
}

// D-11: a QUrl key: the scheme and host are case-insensitive, the fragment counts.
void url_keys() {
  Rig<QMap<QUrl, int>> map;
  CHECK(map.source.insert(QUrl("http://a/x"), 1));
  CHECK(!map.source.insert(QUrl("HTTP://A/x"), 2));
  CHECK(map.source.insert(QUrl("http://a/x#f"), 3));
  CHECK(map.keys() == map.source.items().keys() && map.model.rowCount() == 2);
  Rig<QHash<QUrl, int>> hash;
  CHECK(hash.source.insert(QUrl("http://a/x"), 1) && !hash.source.insert(QUrl("HTTP://A/x"), 2));
  CHECK(hash.source.insert(QUrl("http://a/y"), 3));
  CHECK(hash.keys() == QList<QUrl>({QUrl("http://a/x"), QUrl("http://a/y")}));
  CHECK(hash.source.remove(QUrl("HTTP://A/x")) && hash.model.rowCount() == 1);
}

// D-12: a struct key with operator<, == and qHash: rows follow the container, the key role round-trips through
// QVariant, and Qt 6 needs no Q_DECLARE_METATYPE for it.
void custom_struct_keys() {
  CHECK(QMetaType::fromType<Point>().isValid());
  CHECK(QVariant::fromValue(Point{1, 2}).value<Point>() == (Point{1, 2}));
  Rig<QMap<Point, int>> map;
  for (const Point& p : {Point{2, 1}, Point{1, 9}, Point{1, 2}, Point{3, 0}}) CHECK(map.source.insert(p, p.x));
  CHECK(!map.source.insert(Point{1, 2}, 0));
  CHECK(map.keys() == QList<Point>({{1, 2}, {1, 9}, {2, 1}, {3, 0}}));
  CHECK(map.source.remove(Point{1, 9}) && map.keys() == map.source.items().keys());

  Rig<QHash<Point, int>> hash;
  for (const Point& p : {Point{2, 1}, Point{1, 9}, Point{1, 2}}) CHECK(hash.source.insert(p, 0));
  CHECK(hash.keys() == QList<Point>({{2, 1}, {1, 9}, {1, 2}}));  // the order they were added
  Rig<QSet<Point>> set;
  CHECK(set.source.insert(Point{1, 1}) && !set.source.insert(Point{1, 1}) && set.model.rowCount() == 1);
}

// D-13: a qHash that sends every key to one bucket changes the speed, not the answers or the row order.
void colliding_hash_keys() {
  Rig<QHash<Colliding, int>> rig;
  for (int id = 0; id < 200; ++id) CHECK(rig.source.insert(Colliding{id}, id));
  for (int id = 0; id < 200; id += 3) CHECK(rig.source.remove(Colliding{id}));
  CHECK(!rig.source.insert(Colliding{1}, 0));
  QList<Colliding> expected;
  for (int id = 0; id < 200; ++id)
    if (id % 3 != 0) expected.append(Colliding{id});
  CHECK(rig.keys() == expected);
}

// D-15: a key that cannot be default constructed works in a map and a hash: the traits, the index and the model only copy it.
void keys_without_a_default_constructor() {
  static_assert(!std::is_default_constructible_v<Fixed>);
  Rig<QMap<Fixed, int>> map;
  CHECK(map.source.insert(Fixed{3}, 1) && map.source.insert(Fixed{1}, 2) && !map.source.insert(Fixed{3}, 9));
  CHECK(map.source.replace(Fixed{1}, 5) && map.source.remove(Fixed{3}));
  CHECK(map.model.rowCount() == 1);
  const QVariant key = map.model.index(0, 0).data(Qt::UserRole);
  CHECK(key.metaType() == QMetaType::fromType<Fixed>());
  CHECK(key.constData() && static_cast<const Fixed*>(key.constData())->v == 1);
  Rig<QHash<Fixed, int>> hash;
  CHECK(hash.source.insert(Fixed{7}, 1) && hash.source.remove(Fixed{7}) && hash.model.rowCount() == 0);
}

// D-37: the key role gives back a key equal to the container's, for every key type, in every row.
template <class C, class... Keys>
void round_trip(Keys... keys) {
  Rig<C> rig;
  for (const auto& key : {keys...}) CHECK(rig.source.insert(key, {}));
  const auto stored = rig.keys();
  CHECK(stored.size() == static_cast<qsizetype>(sizeof...(Keys)));
  for (const auto& key : stored) {
    CHECK(rig.source.contains(key));
    CHECK(QVariant::fromValue(key).template value<typename Rig<C>::Key>() == key);
  }
}

void the_key_role_round_trips_for_every_key_type() {
  round_trip<QMap<int, int>>(3, -1, 2);
  round_trip<QHash<unsigned, int>>(3u, 1u, 2u);
  round_trip<QMap<qint64, int>>(Q_INT64_C(-5000000000), Q_INT64_C(5000000000));
  round_trip<QMap<QString, int>>(QString("b"), QString("a"));
  round_trip<QMap<QByteArray, int>>(QByteArray("b"), QByteArray("a\0b", 3));
  round_trip<QHash<QUrl, int>>(QUrl("http://a/"), QUrl("http://b/"));
  round_trip<QMap<QDateTime, int>>(QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC), QDateTime(QDate(2025, 1, 1), QTime(0, 0), QTimeZone::UTC));
  round_trip<QMap<double, int>>(1.5, -0.0, 2.5);
  round_trip<QHash<float, int>>(1.5f, 2.5f);
  round_trip<QMap<Point, int>>(Point{1, 1}, Point{0, 5});
  round_trip<QHash<Point, int>>(Point{1, 1}, Point{0, 5});
  round_trip<QSet<QString>>(QString("x"), QString("y"));
  round_trip<QMap<QChar, int>>(QChar('b'), QChar('a'));
  round_trip<QMap<bool, int>>(true, false);
}

// D-38: the key role is Qt::UserRole, named "key"; the role right after it is empty, an edit through it is refused,
// and itemData (which lists only the roles below UserRole) does not carry it.
void the_key_role_sits_at_user_role() {
  Rig<QMap<QString, int>> rig;
  CHECK(rig.source.insert(QString("k"), 1));
  CHECK(rig.model.roleNames().value(Qt::UserRole) == "key");
  CHECK(rig.model.roleNames().contains(Qt::DisplayRole));
  const QModelIndex index = rig.model.index(0, 0);
  CHECK(index.data(Qt::UserRole).toString() == "k");
  CHECK(!index.data(Qt::UserRole + 1).isValid());
  // Qt's itemData lists only the roles below Qt::UserRole, so the key role is not in it (nor in a drag payload).
  CHECK(!rig.model.itemData(index).contains(Qt::UserRole));
  CHECK(!rig.model.setData(index, "other", Qt::UserRole));
  CHECK(rig.keys() == QList<QString>({QString("k")}));
  // A list has no key role at all.
  Rig<QList<int>> list;
  CHECK(!list.model.roleNames().contains(Qt::UserRole));
  CHECK(list.source.insert(0, 4) && !list.model.index(0, 0).data(Qt::UserRole).isValid());
}
}  // namespace

int main() {
  negative_zero_is_zero();
  float_keys_compare_exactly();
  null_and_empty_string_are_one_key();
  unicode_keys_follow_the_code_unit_order();
  embedded_nul_keys();
  byte_array_keys();
  date_time_keys_are_instants();
  url_keys();
  custom_struct_keys();
  colliding_hash_keys();
  keys_without_a_default_constructor();
  the_key_role_round_trips_for_every_key_type();
  the_key_role_sits_at_user_role();
  return rqt_test::finish("capability_edge_data_keys");
}
