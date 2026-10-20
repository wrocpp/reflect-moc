// sample_store.h: a plain class built on a specialization, with RQT_OBJECT and a signal of its own.
#pragma once

#include "observable_container.h"
#include "sample.h"

#include <QtCore/QHash>

class SampleStore : public ObservableContainer<QHash<quint32, Sample>> {
  RQT_OBJECT

 public:
  int unit() const { return m_unit; }
  void setUnit(int unit) {
    if (unit == m_unit) return;
    m_unit = unit;
    emit unitChanged();
  }

 signals:
  rqt::signal<void()> unitChanged;

 private:
  int m_unit = 0;
};
