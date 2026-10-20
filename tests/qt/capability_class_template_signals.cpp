// A class template that declares its own typed signals: one ObservableContainer<C> for a QHash or a
// QList, `rqt::signal<void(Key const&)>` with no QVariant key and no signals-only base class.
// moc rejects Q_OBJECT in a class template; a class built on the template is not a template and
// has signals of its own.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaMethod>
#include <QtCore/QObject>

#include <utility>

#ifdef RQT_HAVE_QTTEST
#include <QtTest/QSignalSpy>
#endif

namespace {
constexpr unsigned kKey = 42;
constexpr int kValue = 7;
}  // namespace

struct Sample {
  int value = 0;
};

template <class C>
struct KeyOf;
template <class K, class V>
struct KeyOf<QHash<K, V>> {
  using type = K;
};
template <class V>
struct KeyOf<QList<V>> {
  using type = qsizetype;
};
template <class C>
using KeyOf_t = typename KeyOf<C>::type;

template <class C>
struct ValueOf;
template <class K, class V>
struct ValueOf<QHash<K, V>> {
  using type = V;
};
template <class V>
struct ValueOf<QList<V>> {
  using type = V;
};
template <class C>
using ValueOf_t = typename ValueOf<C>::type;

template <class C>
class ObservableContainer : public QObject {
  RQT_OBJECT

 public:
  using Key = KeyOf_t<C>;

  void insert(Key const& key, ValueOf_t<C> value) {
    emit entryAboutToBeAdded(key);
    items_.insert(key, std::move(value));
    emit entryAdded(key);
  }
  qsizetype size() const { return items_.size(); }

 signals:
  rqt::signal<void(Key const&)> entryAboutToBeAdded;
  rqt::signal<void(Key const&)> entryAdded;

 private:
  C items_;
};

class SampleStore : public ObservableContainer<QHash<unsigned, Sample>> {
  RQT_OBJECT

 signals:
  rqt::signal<void()> unitChanged;
};

// A static signal costs no space; in a class template each specialization has its own.
template <class T>
class StaticBox : public QObject {
  RQT_OBJECT_STATIC

 public:
  void set(T const& v) { changed(v).from(this); }

 signals:
  static inline rqt::static_signal<void(T const&)> changed;
};

// The same signal type in a class that is not a template.
class PlainIndexed : public QObject {
  RQT_OBJECT

 signals:
  rqt::signal<void(qsizetype)> moved;
};

int main() {
  SampleStore store;
  QMetaObject const* const mo = store.metaObject();

  // The class name Qt sees is the plain class's own name; the base is the specialization.
  CHECK(QByteArray{mo->className()} == "SampleStore");
  CHECK(mo->superClass() != nullptr);
  CHECK(mo->methodOffset() == mo->superClass()->methodCount());
  CHECK(mo->methodCount() - mo->methodOffset() == 1);
  CHECK(mo->superClass()->methodCount() - mo->superClass()->methodOffset() == 2);

  int added = 0;
  unsigned seen = 0;
  QObject::connect(&store, &SampleStore::entryAdded, &store, [&](unsigned const& key) {
    ++added;
    seen = key;
  });
  store.insert(kKey, Sample{kValue});
  CHECK(added == 1);
  CHECK(seen == kKey);
  CHECK(store.size() == 1);

#ifdef RQT_HAVE_QTTEST
  QSignalSpy spy(&store, SIGNAL(entryAboutToBeAdded(unsigned const&)));
  store.insert(kKey + 1, Sample{kValue});
  CHECK(spy.count() == 1);
#endif
  using Hashed = ObservableContainer<QHash<unsigned, Sample>>;
  using Listed = ObservableContainer<QList<Sample>>;
  Hashed hashed;
  Listed listed;
  // The class name is the compiler's spelling of the specialization in Qt's normalized form: GCC writes
  // "unsigned int", a space in "> >" and one after the comma, Qt wants "uint", ">>" and none.
  CHECK(QByteArray{hashed.metaObject()->className()} == "ObservableContainer<QHash<uint,Sample>>");
  CHECK(QByteArray{listed.metaObject()->className()} == "ObservableContainer<QList<Sample>>");
  CHECK(hashed.metaObject() != listed.metaObject());
  CHECK(hashed.metaObject()->indexOfSignal("entryAdded(unsigned int)") >= 0);
  CHECK(listed.metaObject()->indexOfSignal("entryAdded(qlonglong)") >= 0);
  CHECK(qobject_cast<Hashed*>(&store) == &store);
  CHECK(qobject_cast<Listed*>(&store) == nullptr);
  PlainIndexed plain;
  CHECK(plain.metaObject()->indexOfSignal("moved(qlonglong)") >= 0);
  StaticBox<int> boxed;
  int heard = 0;
  QObject::connect(&boxed, &StaticBox<int>::changed, &boxed, [&](int const& v) { heard = v; });
  boxed.set(kValue);
  CHECK(heard == kValue);
  CHECK(sizeof(StaticBox<int>) == sizeof(QObject));
  return rqt_test::finish("capability_class_template_signals");
}
