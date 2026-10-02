// What a signal costs in the object. A non-static rqt::signal keeps an owner pointer (8 bytes per
// signal) and RQT_OBJECT adds an anchor (8 bytes with padding): one signal 32 bytes, three 48. An
// rqt::static_signal takes no space, and RQT_OBJECT_STATIC leaves out the anchor: a class with three
// static signals is exactly a QObject. A moc class is the reference (one QObject, no per-signal cost).
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>

namespace {
constexpr std::size_t kQObject = 16;
constexpr std::size_t kPointer = 8;
constexpr std::size_t kAnchorWithPadding = 8;
constexpr std::size_t kOneSignal = 32;
constexpr std::size_t kThreeSignals = 48;
constexpr std::size_t kStaticOnly = 16;
constexpr std::size_t kAnchorOnly = 24;
constexpr std::size_t kMixed = 32;
}  // namespace

class OneNonStatic : public QObject {
  RQT_OBJECT

 signals:
  rqt::signal<void(int)> a;
};

class ThreeNonStatic : public QObject {
  RQT_OBJECT

 signals:
  rqt::signal<void(int)> a;
  rqt::signal<void(int)> b;
  rqt::signal<void(int, int)> c;
};

class ThreeStatic : public QObject {
  RQT_OBJECT_STATIC

 signals:
  static inline rqt::static_signal<void(int)> a{};
  static inline rqt::static_signal<void(int)> b{};
  static inline rqt::static_signal<void(int, int)> c{};
};

// static signals under plain RQT_OBJECT: the anchor stays, the signals still cost nothing
class ThreeStaticWithAnchor : public QObject {
  RQT_OBJECT

 signals:
  static inline rqt::static_signal<void(int)> a{};
  static inline rqt::static_signal<void(int)> b{};
  static inline rqt::static_signal<void(int, int)> c{};
};

// one of each kind needs RQT_OBJECT: the anchor, the one pointer, no cost for the static ones
class Mixed : public QObject {
  RQT_OBJECT

 signals:
  rqt::signal<void(int)> a;
  static inline rqt::static_signal<void(int)> b{};
  static inline rqt::static_signal<void(int, int)> c{};
};

static_assert(sizeof(QObject) == kQObject);
static_assert(sizeof(OneNonStatic) == kOneSignal);
static_assert(sizeof(OneNonStatic) == kQObject + kAnchorWithPadding + kPointer);
static_assert(sizeof(ThreeNonStatic) == kThreeSignals);
static_assert(sizeof(ThreeNonStatic) == kQObject + kAnchorWithPadding + 3 * kPointer);
static_assert(sizeof(ThreeStatic) == kStaticOnly);
static_assert(sizeof(ThreeStatic) == sizeof(QObject));
static_assert(sizeof(ThreeStaticWithAnchor) == kAnchorOnly);
static_assert(sizeof(Mixed) == kMixed);

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  std::printf("sizeof: QObject %zu, 1 non-static %zu, 3 non-static %zu, 3 static (RQT_OBJECT_STATIC) %zu, "
              "3 static (RQT_OBJECT) %zu, mixed %zu\n",
              sizeof(QObject), sizeof(OneNonStatic), sizeof(ThreeNonStatic), sizeof(ThreeStatic),
              sizeof(ThreeStaticWithAnchor), sizeof(Mixed));
  CHECK(sizeof(OneNonStatic) == kOneSignal);
  CHECK(sizeof(ThreeNonStatic) == kThreeSignals);
  CHECK(sizeof(ThreeStatic) == kStaticOnly);
  CHECK(sizeof(ThreeStaticWithAnchor) == kAnchorOnly);
  CHECK(sizeof(Mixed) == kMixed);

  // a class whose static signals are all of its members still works as an object
  ThreeStatic object;
  int heard = 0;
  QObject::connect(&object, &ThreeStatic::a, &object, [&](int v) { heard = v; });
  ThreeStatic::a(kQObject).from(&object);
  CHECK(heard == static_cast<int>(kQObject));
  return rqt_test::finish("capability_static_signal_size");
}
