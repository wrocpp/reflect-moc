// A property revision: REVISION(2, 3) becomes 0x203 in QMetaProperty::revision(), so a QML import version check can tell
// the version a property belongs to. The meta-object must equal moc's, for the keyword in RQT_PROPERTY (on an accessor and
// on a MEMBER) and for the annotation `.revision = {1, 2}`. docs/moc-parity.md: C-06.
#include "parity.hpp"
#include "parity_revision_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>

namespace viarqt {

class Versioned : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged REVISION(2, 3))
  RQT_PROPERTY(int plain READ plain)
  RQT_PROPERTY(int fourth MEMBER fourth_ REVISION(4, 0))

 public:
  explicit Versioned(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  int plain() const { return 0; }
  [[= rqt::property{.revision = {1, 2}}]] int third() const { return 0; }
  [[= rqt::slot]] void setValue(int v) {
    value_ = v;
    emit valueChanged(v);
  }

 signals:
  [[= rqt::names("value")]] rqt::signal<void(int)> valueChanged;

 private:
  int value_ = 0;
  int fourth_ = 0;
};

// The older spellings the parser also takes: the encoded number, bare or in parentheses.
class Spellings : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int pair READ get REVISION(2, 3))
  RQT_PROPERTY(int bare READ get REVISION 515)
  RQT_PROPERTY(int wrapped READ get REVISION(515))

 public:
  int get() const { return 0; }
};

}  // namespace viarqt

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::Versioned moc_object;
  viarqt::Versioned rqt_object;
  QMetaObject const* moc = moc_object.metaObject();
  QMetaObject const* ours = rqt_object.metaObject();
  PARITY_SAME("Versioned", moc, ours);
  CHECK(parity::same_totals(moc, ours));
  CHECK(moc->property(moc->indexOfProperty("value")).revision() == (2 << 8 | 3));
  CHECK(ours->property(ours->indexOfProperty("value")).revision() == (2 << 8 | 3));
  CHECK(ours->property(ours->indexOfProperty("plain")).revision() == 0);

  viarqt::Spellings spellings;
  QMetaObject const* s = spellings.metaObject();
  for (char const* name : {"pair", "bare", "wrapped"}) CHECK(s->property(s->indexOfProperty(name)).revision() == (2 << 8 | 3));
  return rqt_test::finish("parity_revision");
}
