// The oracle: a QObject class and a gadget with invokable constructors (default arguments, a QObject* parameter, a class
// type parameter), processed by moc.
#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace viamoc {

class Widget : public QObject {
  Q_OBJECT

 public:
  Q_INVOKABLE explicit Widget(QObject* parent = nullptr) : QObject(parent) {}
  Q_INVOKABLE Widget(int a, int b = 3, QObject* parent = nullptr) : QObject(parent), sum(a + b) {}
  Q_INVOKABLE Widget(const QString& label, double scale) : label(label), sum(static_cast<int>(scale)) {}
  int sum = 0;
  QString label;
};

struct Point {
  Q_GADGET

 public:
  Q_INVOKABLE Point() = default;
  Q_INVOKABLE Point(int x, int y = 7) : x(x), y(y) {}
  int x = 0;
  int y = 0;
};

}  // namespace viamoc
