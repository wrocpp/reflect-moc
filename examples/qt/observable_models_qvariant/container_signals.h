// The non-template base of the moc design: moc accepts Q_OBJECT here because this is not a template. It
// declares every signal of observable::ObservableContainer, with the key boxed in a QVariant because one
// base has to serve every container. It holds no data.
//
// This directory is a reconstruction, from the description in a reader's post, of the split that moc forces.
// It exists to be measured against examples/qt/observable_models; it is not a recommended design.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

class ContainerSignals : public QObject {
  Q_OBJECT

 public:
  using QObject::QObject;

 signals:
  void entryAboutToBeAdded(const QVariant& key);
  void entryAdded(const QVariant& key);
  void entryAboutToBeRemoved(const QVariant& key);
  void entryRemoved(const QVariant& key);
  void entryAboutToBeChanged(const QVariant& key);
  void entryChanged(const QVariant& key);
  void entryAboutToBeMoved(const QVariant& from, const QVariant& to);
  void entryMoved(const QVariant& from, const QVariant& to);
  void aboutToBeCleared();
  void cleared();
};
