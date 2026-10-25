// The other half of limit_enum_from_type_macro_free: an enum of a macro-free class registered only by [[=rqt::enum_]] has
// no Q_ENUM friends, so Qt does not see it as a registered enum (QtPrivate::IsQEnumHelper is false and fromType<E>() does
// not compile), but the meta-object still lists it with the right keys. docs/moc-parity.md: D-05.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>

namespace {

constexpr int mode_key_count = 2;

class Gauge : public rqt::ObjectOf<> {
 public:
  enum class [[= rqt::enum_]] Mode { off, on };
};

static_assert(!QtPrivate::IsQEnumHelper<Gauge::Mode>::Value);

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Gauge gauge;
  QMetaObject const* meta = gauge.metaObject();
  QMetaEnum const mode = meta->enumerator(meta->indexOfEnumerator("Mode"));
  CHECK(mode.isValid());
  CHECK(mode.keyCount() == mode_key_count);
  CHECK(QByteArray{mode.key(0)} == "off");
  CHECK(QByteArray{mode.key(1)} == "on");
  CHECK(mode.isScoped());
  return rqt_test::finish("limit_parity_enum_macro_free");
}
