// An interface with a Q_DECLARE_INTERFACE id, shared by the moc and the reflect-moc side of the differential test.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

class DiffGreeter {
 public:
  virtual ~DiffGreeter() = default;
  virtual QString greet() const = 0;
};
Q_DECLARE_INTERFACE(DiffGreeter, "org.rqt.diff.Greeter/1.0")
