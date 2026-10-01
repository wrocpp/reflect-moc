// reflect_moc/compat.hpp: a class written with Q_OBJECT, Q_PROPERTY, Q_INVOKABLE, Q_ENUM,
// Q_FLAG and Q_CLASSINFO compiles unchanged; the same declarations through RQT_ENUM,
// RQT_FLAG and RQT_CLASSINFO give the same meta-object content.
#include "check.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>

#include <reflect_moc/compat.hpp>

namespace {
constexpr int kLeftTop = 5;
constexpr int kAdded = 4;
}  // namespace

class Painter : public QObject {
  Q_OBJECT
  Q_CLASSINFO("author", "Filip")
  Q_CLASSINFO("version", "1.0")
  Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
  Q_PROPERTY(int width MEMBER width_ NOTIFY widthChanged)

 public:
  enum class Mode { Fill, Stroke };
  Q_ENUM(Mode)
  enum Edge { Left = 1, Right = 2, Top = 4 };
  Q_DECLARE_FLAGS(Edges, Edge)
  Q_FLAG(Edges)

  explicit Painter(QObject* parent = nullptr) : QObject(parent) {}
  Mode mode() const { return mode_; }
  Q_INVOKABLE int add(int n) { return width_ + n; }

 public slots:
  [[= rqt::slot]] void setMode(Mode m) {
    if (m == mode_) return;
    mode_ = m;
    emit modeChanged(m);
  }

 signals:
  rqt::signal<void(Mode)> modeChanged;
  rqt::signal<void(int)> widthChanged;

 private:
  Mode mode_ = Mode::Fill;
  int width_ = 10;
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Painter painter;
  QMetaObject const* mo = painter.metaObject();

  // Q_CLASSINFO
  CHECK(mo->classInfoCount() - mo->classInfoOffset() == 2);
  CHECK(QByteArray{mo->classInfo(mo->indexOfClassInfo("author")).value()} == "Filip");
  CHECK(QByteArray{mo->classInfo(mo->indexOfClassInfo("version")).value()} == "1.0");

  // Q_ENUM and Q_FLAG
  QMetaEnum const mode = mo->enumerator(mo->indexOfEnumerator("Mode"));
  CHECK(mode.isValid() && mode.isScoped() && !mode.isFlag() && mode.keyCount() == 2);
  CHECK(QByteArray{mode.key(1)} == "Stroke");
  QMetaEnum const edges = mo->enumerator(mo->indexOfEnumerator("Edges"));
  CHECK(edges.isValid() && edges.isFlag() && edges.keyCount() == 3);
  CHECK(QByteArray{edges.enumName()} == "Edge");
  CHECK(edges.keysToValue("Left|Top") == kLeftTop);

  // Q_PROPERTY with an enum type, with MEMBER, and Q_INVOKABLE
  QMetaProperty const prop = mo->property(mo->indexOfProperty("mode"));
  CHECK(prop.isEnumType() && prop.hasNotifySignal());
  CHECK(prop.write(&painter, QVariant{QString{"Stroke"}}) && painter.mode() == Painter::Mode::Stroke);
  QMetaProperty const width = mo->property(mo->indexOfProperty("width"));
  CHECK(width.write(&painter, 20) && width.read(&painter).toInt() == 20);
  int sum = 0;
  CHECK(QMetaObject::invokeMethod(&painter, "add", Q_RETURN_ARG(int, sum), Q_ARG(int, kAdded)) && sum == 20 + kAdded);
  CHECK(qobject_cast<Painter*>(static_cast<QObject*>(&painter)) == &painter);
  return rqt_test::finish("capability_compat_header_moc_syntax");
}
