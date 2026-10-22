// QMetaEnum::fromType<E>() and fromType<Flags>(): Q_ENUM declares a friend qt_getEnumMetaObject and qt_getEnumName per
// enum, which RQT_OBJECT now declares (once, for every enum the class registers, by annotation or by RQT_ENUM) and
// RQT_ENUM declares too for a class with no RQT_OBJECT. The QMetaEnum must equal moc's (name, enumName, keys, flag,
// scoped), and so must the enumerator() of an enum-typed property. docs/moc-parity.md: D-01, D-02.
#include "parity.hpp"
#include "parity_enum_from_type_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaEnum>
#include <QtCore/QObject>

namespace viarqt {

class Colored : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(Mode mode READ mode WRITE setMode)
  RQT_PROPERTY(Options options READ options WRITE setOptions)
  RQT_PROPERTY(Level level READ level)

 public:
  enum class [[= rqt::enum_]] Level : short { Low, High };  // annotated enums come first in the table
  enum Mode { Day, Night = 5 };
  RQT_ENUM(Mode)
  enum Option { Bold = 1, Italic = 2, Underline = 4 };
  Q_DECLARE_FLAGS(Options, Option)
  RQT_FLAG(Options)

  explicit Colored(QObject* parent = nullptr) : QObject(parent) {}
  Mode mode() const { return mode_; }
  void setMode(Mode m) { mode_ = m; }
  Options options() const { return options_; }
  void setOptions(Options o) { options_ = o; }
  Level level() const { return Level::High; }

 private:
  Mode mode_ = Day;
  Options options_;
};

}  // namespace viarqt

namespace {
QString describe(QMetaEnum const& e) {
  QStringList keys;
  for (int k = 0; k < e.keyCount(); ++k) keys << QString{"%1=%2"}.arg(QString::fromUtf8(e.key(k))).arg(e.value(k));
  return QString{"valid=%1 name=%2 enumName=%3 flag=%4 scoped=%5 scope=%6 keys=%7"}
      .arg(e.isValid())
      .arg(QString::fromUtf8(e.name()))
      .arg(QString::fromUtf8(e.enumName()))
      .arg(e.isFlag())
      .arg(e.isScoped())
      .arg(QString::fromUtf8(e.scope()).remove("viamoc::").remove("viarqt::"))
      .arg(keys.join(','));
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QMetaObject const* moc = &viamoc::Colored::staticMetaObject;
  QMetaObject const* ours = viarqt::Colored{}.metaObject();
  PARITY_SAME("Colored", moc, ours);

  CHECK(QMetaEnum::fromType<viarqt::Colored::Mode>().isValid());
  CHECK(describe(QMetaEnum::fromType<viarqt::Colored::Mode>()) == describe(QMetaEnum::fromType<viamoc::Colored::Mode>()));
  CHECK(describe(QMetaEnum::fromType<viarqt::Colored::Options>()) == describe(QMetaEnum::fromType<viamoc::Colored::Options>()));
  CHECK(describe(QMetaEnum::fromType<viarqt::Colored::Level>()) == describe(QMetaEnum::fromType<viamoc::Colored::Level>()));
  CHECK(QMetaEnum::fromType<viarqt::Colored::Options>().isFlag());
  CHECK(QMetaEnum::fromType<viarqt::Colored::Level>().isScoped());
  CHECK(QMetaEnum::fromType<viarqt::Colored::Mode>().valueToKey(5) == QByteArray{"Night"});

  for (char const* name : {"mode", "options", "level"}) {
    QMetaProperty const a = moc->property(moc->indexOfProperty(name));
    QMetaProperty const b = ours->property(ours->indexOfProperty(name));
    CHECK(b.isEnumType() && b.enumerator().isValid());
    CHECK(describe(a.enumerator()) == describe(b.enumerator()));
  }

  // a value of the type round trips through a QVariant by its key, which needs the enum to be known to Qt
  viarqt::Colored object;
  CHECK(object.setProperty("mode", "Night") && object.mode() == viarqt::Colored::Night);
  CHECK(object.setProperty("options", "Bold|Underline") && object.options() == (viarqt::Colored::Bold | viarqt::Colored::Underline));
  return rqt_test::finish("parity_enum_from_type");
}
