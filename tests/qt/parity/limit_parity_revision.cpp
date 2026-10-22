// Pinned: a property revision. moc writes REVISION(2, 3) into the property (0x203) and QMetaProperty::revision() returns
// it; reflect-moc parses the keyword and drops it without a message, so the revision reads 0 and a QML import version
// check would accept the property in a version it does not belong to. The fix threads the number through
// annotation_from and property_data; this file then becomes parity_revision (docs/moc-parity.md, C-06).
#include "limit_parity_revision_moc.h"
#include "parity.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>

namespace viarqt {

class Versioned : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged REVISION 515)
  RQT_PROPERTY(int plain READ plain)

 public:
  explicit Versioned(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  int plain() const { return 0; }
  [[= rqt::slot]] void setValue(int v) {
    value_ = v;
    emit valueChanged(v);
  }

 signals:
  [[= rqt::names("value")]] rqt::signal<void(int)> valueChanged;

 private:
  int value_ = 0;
};

}  // namespace viarqt

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QMetaObject const* moc = &viamoc::Versioned::staticMetaObject;
  QMetaObject const* ours = viarqt::Versioned{}.metaObject();
  int const valueIndex = moc->indexOfProperty("value");
  CHECK(valueIndex >= 0);
  CHECK(moc->property(valueIndex).revision() == (2 << 8 | 3));
  // the difference: reflect-moc keeps no revision
  CHECK(ours->property(ours->indexOfProperty("value")).revision() == 0);
  CHECK(moc->property(moc->indexOfProperty("plain")).revision() == 0);
  return rqt_test::finish("limit_parity_revision");
}
