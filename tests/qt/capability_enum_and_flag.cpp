// Q_ENUM and Q_FLAG for nested enums: QMetaEnum content, enum-typed properties
// and signal arguments resolved by name.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaProperty>

namespace {
constexpr int kLeftTop = 5;
constexpr int kEnumCount = 2;
constexpr int kModeKeys = 2;
constexpr int kEdgeKeys = 3;
}  // namespace

struct Painter : rqt::Object<> {
  Painter() { bind(); }

  enum class [[= rqt::enum_]] Mode { Fill, Stroke };
  enum class [[= rqt::flag]] Edge { Left = 1, Right = 2, Top = 4 };

  [[= rqt::property{.write = "setMode", .notify = "modeChanged"}]] Mode mode() const { return mode_; }
  [[= rqt::signal]] void modeChanged(Mode mode) { rqt::emit{this}(mode); }
  [[= rqt::slot]] void setMode(Mode m) {
    if (m == mode_) return;
    mode_ = m;
    modeChanged(m);
  }

  Mode mode_ = Mode::Fill;
};

struct Listener : rqt::Object<> {
  Listener() { bind(); }
  Painter::Mode last = Painter::Mode::Fill;
  [[= rqt::slot]] void onMode(Painter::Mode m) { last = m; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Painter painter;
  QMetaObject const* mo = painter.metaObject();

  CHECK(mo->enumeratorCount() - mo->enumeratorOffset() == kEnumCount);
  QMetaEnum const mode = mo->enumerator(mo->indexOfEnumerator("Mode"));
  CHECK(mode.isValid() && !mode.isFlag() && mode.isScoped());
  CHECK(mode.keyCount() == kModeKeys && QByteArray{mode.key(1)} == "Stroke");
  CHECK(mode.keyToValue("Stroke") == static_cast<int>(Painter::Mode::Stroke));
  CHECK(QByteArray{mode.name()} == "Mode");

  QMetaEnum const edge = mo->enumerator(mo->indexOfEnumerator("Edge"));
  CHECK(edge.isValid() && edge.isFlag() && edge.keyCount() == kEdgeKeys);
  CHECK(edge.keysToValue("Left|Top") == kLeftTop);
  CHECK(edge.valueToKeys(kLeftTop) == "Left|Top");

  // an enum-typed property resolves to the enumerator by name
  QMetaProperty const prop = mo->property(mo->indexOfProperty("mode"));
  CHECK(prop.isEnumType() && QByteArray{prop.enumerator().name()} == "Mode");
  CHECK(prop.write(&painter, QVariant{QString{"Stroke"}}) && painter.mode_ == Painter::Mode::Stroke);
  CHECK(prop.read(&painter).toInt() == static_cast<int>(Painter::Mode::Stroke));

  // and as a signal argument, through a string connection
  Listener listener;
  CHECK(QObject::connect(&painter, SIGNAL(modeChanged(Mode)), &listener, SLOT(onMode(Mode))));
  painter.setMode(Painter::Mode::Fill);
  CHECK(listener.last == Painter::Mode::Fill);
  return rqt_test::finish("capability_enum_and_flag");
}
