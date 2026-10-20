// moc side of the signal-only benchmark: one non-template QObject, signals keyed by QVariant, as the moc
// design needs. The reflect-moc side is in signal.cpp.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

class QvSignals : public QObject {
  Q_OBJECT
 signals:
  void entryAboutToBeAdded(const QVariant&);
  void entryAdded(const QVariant&);
  void entryAboutToBeRemoved(const QVariant&);
  void entryRemoved(const QVariant&);
  void entryAboutToBeChanged(const QVariant&);
  void entryChanged(const QVariant&);
  void entryAboutToBeMoved(const QVariant&, const QVariant&);
  void entryMoved(const QVariant&, const QVariant&);
  void aboutToBeCleared();
  void cleared();
};
