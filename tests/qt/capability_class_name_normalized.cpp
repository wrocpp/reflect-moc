// The class name Qt reports for a specialization is in the form Qt's own normalizer gives it. Qt registers a
// pointer to a class under className() + "*" and aborts on a name it would normalize differently, so a pointer
// to a specialization can be put in a QVariant, a signal argument or a QML property only if the name is
// normalized. normalize_type_name is compared with QMetaObject::normalizedType on the spellings a compiler
// writes for a specialization.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QVariant>

namespace {
template <class K>
class Keyed : public QObject {
  RQT_OBJECT

 signals:
  rqt::signal<void(const K&)> changed;
};
template <class A, class B>
class Pair : public QObject {
  RQT_OBJECT

 signals:
  rqt::signal<void()> done;
};

void agrees_with_qt(char const* spelling) {
  const QByteArray ours{rqt::detail::normalize_type_name(spelling).c_str()};
  const QByteArray qts = QMetaObject::normalizedType(spelling);
  if (ours != qts) std::fprintf(stderr, "'%s': ours '%s', Qt '%s'\n", spelling, ours.constData(), qts.constData());
  CHECK(ours == qts);
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  for (char const* spelling : {"A<B<int> >", "A<B<C<int> > >", "A<B<int> >*", "A<unsigned int>", "A<unsigned>",
                               "A<unsigned long>", "A<unsigned long long>", "A<long long>", "A<unsigned short>",
                               "A<unsigned char>", "QHash<unsigned int, Sample>", "ns::A<QList<ns::B> >",
                               "A<int, QList<unsigned int> >", "A<myunsigned int>", "unsigned_long<int>", "A<int>",
                               "A<short unsigned int>", "A<long unsigned int>", "A<long long int>", "A<long long unsigned int>",
                               "A<short int>", "A<long int>", "A<unsigned long int>", "A<unsigned short int>",
                               "A<unsigned long long int>", "A<long long unsigned>", "A<signed char>",
                               "A<long long unsigned int, long int>"})
    agrees_with_qt(spelling);

  // A pointer to a specialization goes into a QVariant: the call that aborted with a name Qt would normalize.
  Keyed<unsigned> hashed;
  Keyed<QList<unsigned long long>> nested;
  Pair<QHash<unsigned, int>, QList<QList<int>>> pair;
  CHECK(QVariant::fromValue(&hashed).value<Keyed<unsigned>*>() == &hashed);
  CHECK(QVariant::fromValue(&nested).value<Keyed<QList<unsigned long long>>*>() == &nested);
  CHECK(QVariant::fromValue(&pair).isValid());

  CHECK(QByteArray{hashed.metaObject()->className()} == QMetaObject::normalizedType(hashed.metaObject()->className()));

  // The integer types GCC spells in another word order ("short unsigned int", "long long int", ...).
  Keyed<unsigned short> narrow;
  Keyed<unsigned long> wide;
  Keyed<long long> signedWide;
  Keyed<unsigned long long> unsignedWide;
  for (const QObject* object : {static_cast<const QObject*>(&narrow), static_cast<const QObject*>(&wide),
                                static_cast<const QObject*>(&signedWide), static_cast<const QObject*>(&unsignedWide)}) {
    const QByteArray name = object->metaObject()->className();
    if (name != QMetaObject::normalizedType(name)) std::fprintf(stderr, "not normalized: %s\n", name.constData());
    CHECK(name == QMetaObject::normalizedType(name));
  }
  CHECK(QByteArray{narrow.metaObject()->className()}.contains("ushort"));
  CHECK(QByteArray{wide.metaObject()->className()}.contains("ulong"));
  CHECK(QByteArray{signedWide.metaObject()->className()}.contains("qlonglong"));
  CHECK(QByteArray{unsignedWide.metaObject()->className()}.contains("qulonglong"));
  return rqt_test::finish("capability_class_name_normalized");
}
