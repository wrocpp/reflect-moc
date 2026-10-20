// The signal path alone: emit one signal that has one connected slot, for three key types, with the key
// boxed in a QVariant (the moc design) or passed as its own type (rqt::signal, rqt::static_signal).
// Also prints sizeof of a container-shaped class (ten signals, a QHash member) for the three designs.
//
// The 32-byte key is the worst case for QVariant: it does not fit the variant's inline storage, so every
// boxing allocates. A quint32 or a QString does not, and that is what a real container uses.
//
// Output: signal,<design>,<key>,<ns per emit>   and   size,<design>,<bytes>
#include "signal_qv.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QString>

#include <chrono>
#include <cstdio>

namespace {
constexpr int kEmits = 5'000'000;

struct Fat {
  quint64 part[4]{};  // 32 bytes
};

template <class K>
class Typed : public QObject {
  RQT_OBJECT
 public:
  void run(int n, const K& key) {
    for (int i = 0; i < n; ++i) entryAdded(key);
  }
 signals:
  rqt::signal<void(const K&)> entryAdded;
};

template <class K>
class Static : public QObject {
  RQT_OBJECT_STATIC
 public:
  void run(int n, const K& key) {
    for (int i = 0; i < n; ++i) entryAdded(key).from(this);
  }
 signals:
  static inline rqt::static_signal<void(const K&)> entryAdded{};
};

// Container-shaped classes for the size table: ten signals and a QHash member, like ObservableContainer.
class TypedTen : public QObject {
  RQT_OBJECT
  QHash<quint32, int> m_items;
 signals:
  rqt::signal<void(const quint32&)> a, b, c, d, e, f;
  rqt::signal<void(const quint32&, const quint32&)> g, h;
  rqt::signal<void()> i, j;
};

class StaticTen : public QObject {
  RQT_OBJECT_STATIC
  QHash<quint32, int> m_items;
 signals:
  static inline rqt::static_signal<void(const quint32&)> a{}, b{}, c{}, d{}, e{}, f{};
  static inline rqt::static_signal<void(const quint32&, const quint32&)> g{}, h{};
  static inline rqt::static_signal<void()> i{}, j{};
};

class QvTen : public QvSignals {
  QHash<quint32, int> m_items;
};

template <class F>
double time_ns(F&& f) {
  const auto start = std::chrono::steady_clock::now();
  f();
  return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / kEmits;
}

template <class K>
void measure(const char* name, const K& key) {
  long sink = 0;
  {
    QvSignals qv;
    QObject::connect(&qv, &QvSignals::entryAdded, &qv, [&sink](const QVariant& v) { sink += v.isValid(); });
    std::printf("signal,qvariant,%s,%.1f\n", name, time_ns([&] {
                  for (int i = 0; i < kEmits; ++i) qv.entryAdded(QVariant::fromValue(key));
                }));
  }
  {
    Typed<K> typed;
    QObject::connect(&typed, &Typed<K>::entryAdded, &typed, [&sink](const K&) { ++sink; });
    std::printf("signal,rqt,%s,%.1f\n", name, time_ns([&] { typed.run(kEmits, key); }));
  }
  {
    Static<K> stat;
    QObject::connect(&stat, &Static<K>::entryAdded, &stat, [&sink](const K&) { ++sink; });
    std::printf("signal,rqt-static,%s,%.1f\n", name, time_ns([&] { stat.run(kEmits, key); }));
  }
  if (sink == 0) std::puts("unexpected: nothing was delivered");
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  std::printf("size,qvariant,%zu\n", sizeof(QvTen));
  std::printf("size,rqt,%zu\n", sizeof(TypedTen));
  std::printf("size,rqt-static,%zu\n", sizeof(StaticTen));
  measure("quint32", quint32{42});
  measure("QString", QString("a typical key"));
  measure("Fat32", Fat{});
  return 0;
}
