// rqt::class_name sets the name Qt reports: as an annotation on a class, or RQT_CLASS_NAME in the
// body, where a class template can choose it from its template argument. qobject_cast and
// qt_metacast agree with it because they compare className().
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>

namespace {
constexpr char kAnnotated[] = "Renamed";
}  // namespace

class [[= rqt::class_name{"Renamed"}]] ByAnnotation : public QObject {
  RQT_OBJECT
};

class ByMacro : public QObject {
  RQT_OBJECT
  RQT_CLASS_NAME("MacroName")
};

template <class T>
struct NameOf;
template <>
struct NameOf<int> {
  static constexpr char value[] = "IntBox";
};
template <>
struct NameOf<double> {
  static constexpr char value[] = "DoubleBox";
};

template <class T>
class Box : public QObject {
  RQT_OBJECT
  RQT_CLASS_NAME(NameOf<T>::value)

 signals:
  rqt::signal<void(T const&)> changed;
};

int main() {
  ByAnnotation annotated;
  ByMacro macro;
  Box<int> ints;
  Box<double> doubles;

  CHECK(QByteArray{annotated.metaObject()->className()} == kAnnotated);
  CHECK(QByteArray{macro.metaObject()->className()} == "MacroName");
  CHECK(QByteArray{ints.metaObject()->className()} == "IntBox");
  CHECK(QByteArray{doubles.metaObject()->className()} == "DoubleBox");
  CHECK(ints.metaObject() != doubles.metaObject());
  CHECK(qobject_cast<Box<int>*>(&ints) == &ints);
  CHECK(ints.inherits("IntBox"));
  CHECK(!ints.inherits("DoubleBox"));
  return rqt_test::finish("capability_class_name_override");
}
