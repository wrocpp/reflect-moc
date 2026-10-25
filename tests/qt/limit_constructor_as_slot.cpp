// Limit: a constructor annotated as a slot or a signal is an error, not a constructor the table would list as a slot. This
// file must NOT compile with
//   rqt: a constructor can only be rqt::invokable
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>

class Factory : public rqt::ObjectOf<> {
 public:
  [[= rqt::slot]] Factory(int) {}
};

QMetaObject const* meta = &Factory::staticMetaObject;
