// Limit, pinned: an annotation on a constructor template is not read. GCC 16.2 throws from std::meta::annotations_of on
// every template, so the annotation cannot be found, and `[[=rqt::invokable]] template <class T> Factory(T, int)` is left
// out of the meta-object where moc lists it (moc's own generated code for it does not compile either, `Constructor(T, int)`
// names a template parameter). A constructor template has no one signature to list. The test passes while that holds and is
// flipped when the compiler reads annotations on templates. docs/moc-parity.md: B-13.
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>

class Factory : public rqt::ObjectOf<> {
 public:
  [[= rqt::invokable]] explicit Factory(QObject* parent = nullptr) : ObjectOf(parent) {}
  template <class T>
  [[= rqt::invokable]] Factory(T, int, QObject* parent = nullptr) : ObjectOf(parent) {}
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QMetaObject const& mo = Factory::staticMetaObject;
  constexpr int kPlainRows = 2;  // Factory(QObject*) and its clone Factory(): the template has none
  CHECK(mo.constructorCount() == kPlainRows);
  return rqt_test::finish("limit_constructor_template_not_listed");
}
