// The oracle: a gadget with member and accessor properties and an invokable, and a derived gadget, processed by moc.
#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace viamoc {

struct Point {
  Q_GADGET
  Q_PROPERTY(int x MEMBER x)
  Q_PROPERTY(QString label READ label WRITE setLabel)

 public:
  int x = 0;
  QString label() const { return label_; }
  void setLabel(const QString& l) { label_ = l; }
  Q_INVOKABLE int manhattan(int y) const { return x + y; }

 private:
  QString label_;
};

struct Point3 : Point {
  Q_GADGET
  Q_PROPERTY(int z MEMBER z)

 public:
  int z = 0;
  Q_INVOKABLE int sum() const { return x + z; }
};

}  // namespace viamoc
