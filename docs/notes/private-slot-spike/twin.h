#pragma once
#include <QObject>
class WidgetPrivate;
class Widget : public QObject {
  Q_OBJECT
  Q_PROPERTY(int level READ level)
  Q_PRIVATE_PROPERTY(Widget::d_func(), int depth READ depth WRITE setDepth)
public:
  explicit Widget(QObject* p = nullptr);
  ~Widget() override;
  int level() const { return 1; }
signals:
  void changed();
private:
  Q_PRIVATE_SLOT(d_func(), void onTick(int))
  Q_PRIVATE_SLOT(d_func(), void reset())
  WidgetPrivate* d_ptr;
  WidgetPrivate* d_func() const { return d_ptr; }
};
