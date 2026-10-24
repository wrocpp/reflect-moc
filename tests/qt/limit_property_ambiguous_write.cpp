// Limit: a WRITE that names overloads none of which takes the property's type exactly and several of which it converts to is
// ambiguous, as the C++ call moc generates would be. This file must NOT compile with
//   rqt::property WRITE 'setValue': ambiguous, several overloads take the property's type
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>

class Gauge : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(short value READ value WRITE setValue)

 public:
  short value() const { return 0; }
  void setValue(long) {}
  void setValue(double) {}
};

QMetaObject const* meta = &Gauge::staticMetaObject;
