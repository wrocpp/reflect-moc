// container_signals.h: not a template, so moc accepts its Q_OBJECT. It declares the signals and holds no data.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

class ContainerSignals : public QObject {
  Q_OBJECT

 signals:
  // One key type for every container: a list's index or a map's key, boxed.
  void entryAboutToBeAdded(const QVariant& key);
  void entryAdded(const QVariant& key);
};
