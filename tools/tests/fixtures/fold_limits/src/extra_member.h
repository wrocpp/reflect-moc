// A base that holds data is not a signals-only base.
#pragma once
#include <QtCore/QObject>
#include <QtCore/QVariant>

class DataSignals : public QObject {
  Q_OBJECT
 public:
  int count() const { return m_count; }
 signals:
  void changed(const QVariant& key);

 private:
  int m_count = 0;
};

template <class T>
class DataHolder : public DataSignals {
 public:
  using Key = T;
};
