// Limit: rqt::revision and rqt::scriptable on an invokable constructor are an error, not an annotation the constructor table
// leaves out. This file must NOT compile with
//   rqt::revision and rqt::scriptable are not supported on an invokable constructor
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>

class Factory : public rqt::ObjectOf<> {
 public:
  [[= rqt::invokable, = rqt::revision{1, 2}]] Factory(int) {}
};

QMetaObject const* meta = &Factory::staticMetaObject;
