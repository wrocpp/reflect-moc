// Limit: an invokable constructor that is not public is an error: the meta-call creates the object with new, which a private
// constructor does not allow, and moc's generated code would not compile either. This file must NOT compile with
//   rqt: an invokable constructor must be public
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>

class Factory : public rqt::ObjectOf<> {
 private:
  [[= rqt::invokable]] Factory(int) {}

 public:
  Factory() = default;
};

QMetaObject const* meta = &Factory::staticMetaObject;
