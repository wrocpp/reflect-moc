// sample_store.h: a plain class again, so Q_OBJECT and signals of its own.
#pragma once

#include "observable_container.h"
#include "sample.h"

#include <QtCore/QHash>

class SampleStore : public ObservableContainer<QHash<quint32, Sample>> {
  Q_OBJECT

 public:
  int unit() const { return m_unit; }
  void setUnit(int unit) {
    if (unit == m_unit) return;
    m_unit = unit;
    emit unitChanged();
  }

 signals:
  void unitChanged();

 private:
  int m_unit = 0;
};
