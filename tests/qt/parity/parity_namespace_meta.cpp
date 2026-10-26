// Q_NAMESPACE, Q_ENUM_NS and Q_FLAG_NS against RQT_NAMESPACE: the meta-object of a namespace (a nested one too), its enum
// class and its plain enum with a QFlags alias, and a class with properties of those types, must equal moc's row for row,
// and QMetaEnum::fromType, QMetaType::metaObject and QMetaProperty::enumerator must behave the same.
// docs/moc-parity.md: A-04, D-03.
#include "parity.hpp"
#include "parity_namespace_meta_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaProperty>
#include <QtCore/QVariant>

namespace viarqt {
namespace Shapes {
enum class [[= rqt::enum_]] Mode { off, on = 5 };
enum Edge { Left = 1, Right = 2, Top = 4 };
using Edges = QFlags<Edge>;
inline constexpr rqt::enum_decl edges_flag{^^Edges, true};
RQT_NAMESPACE

class Holder : public rqt::ObjectOf<> {
 public:
  [[= rqt::property{}]] Mode mode = Mode::off;
  [[= rqt::property{}]] Edges edges;
};

namespace Inner {
enum class [[= rqt::enum_]] Level { low, high };
RQT_NAMESPACE
}  // namespace Inner
}  // namespace Shapes
}  // namespace viarqt

template <class Mode, class Edges, class Holder>
void behaviour(QMetaObject const& ns, char const* what) {
  QMetaEnum const mode = QMetaEnum::fromType<Mode>();
  rqt_test::report(mode.isValid() && mode.isScoped() && QByteArray{mode.valueToKey(5)} == "on" && mode.enclosingMetaObject() == &ns, what,
                   __FILE__, __LINE__);
  QMetaEnum const edges = QMetaEnum::fromType<Edges>();
  rqt_test::report(edges.isValid() && edges.isFlag() && edges.keysToValue("Left|Top") == 5, what, __FILE__, __LINE__);
  rqt_test::report(QMetaType::fromType<Mode>().metaObject() == &ns, what, __FILE__, __LINE__);

  Holder holder;
  QMetaObject const& mo = *holder.metaObject();
  QMetaProperty const property = mo.property(mo.indexOfProperty("mode"));
  rqt_test::report(property.isEnumType() && property.enumerator().isValid() && QByteArray{property.enumerator().name()} == "Mode", what,
                   __FILE__, __LINE__);
  rqt_test::report(holder.setProperty("mode", QVariant{QString{"on"}}) && holder.mode == Mode::on, what, __FILE__, __LINE__);
  rqt_test::report(property.read(&holder).toString() == "on", what, __FILE__, __LINE__);
  QMetaProperty const flags = mo.property(mo.indexOfProperty("edges"));
  rqt_test::report(flags.isFlagType() && holder.setProperty("edges", QVariant{QString{"Left|Right"}}) &&
                       holder.edges == (Edges{} | Edges::enum_type::Left | Edges::enum_type::Right),
                   what, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  PARITY_SAME("Shapes", &viamoc::Shapes::staticMetaObject, &viarqt::Shapes::staticMetaObject);
  CHECK(parity::same_totals(&viamoc::Shapes::staticMetaObject, &viarqt::Shapes::staticMetaObject));
  PARITY_SAME("Shapes::Inner", &viamoc::Shapes::Inner::staticMetaObject, &viarqt::Shapes::Inner::staticMetaObject);
  CHECK(parity::same_totals(&viamoc::Shapes::Inner::staticMetaObject, &viarqt::Shapes::Inner::staticMetaObject));
  PARITY_SAME("Shapes::Holder", &viamoc::Shapes::Holder::staticMetaObject, &viarqt::Shapes::Holder::staticMetaObject);
  CHECK(parity::same_totals(&viamoc::Shapes::Holder::staticMetaObject, &viarqt::Shapes::Holder::staticMetaObject));
  behaviour<viamoc::Shapes::Mode, viamoc::Shapes::Edges, viamoc::Shapes::Holder>(viamoc::Shapes::staticMetaObject, "moc behaviour");
  behaviour<viarqt::Shapes::Mode, viarqt::Shapes::Edges, viarqt::Shapes::Holder>(viarqt::Shapes::staticMetaObject, "rqt behaviour");
  CHECK(QMetaEnum::fromType<viarqt::Shapes::Inner::Level>().isValid());
  return rqt_test::finish("parity_namespace_meta");
}
