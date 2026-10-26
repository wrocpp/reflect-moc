// the same class through reflect-moc: the header part (private class incomplete), then the private class, then the definitions
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>
#include <QtCore/QCoreApplication>
class WidgetPrivate;
class Widget : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int level READ level)
  RQT_PROPERTY(int depth READ depth WRITE setDepth)  // Q_PRIVATE_PROPERTY: accessors forward to the d-pointer
 public:
  explicit Widget(QObject* p = nullptr);
  ~Widget() override;
  int level() const { return 1; }
  [[= rqt::signal_function]] void changed() { rqt::emit{this}(); }
 private:
  [[= rqt::slot]] void onTick(int);   // Q_PRIVATE_SLOT(d_func(), void onTick(int)): a forwarder, defined next to the private class
  [[= rqt::slot]] void reset();
  int depth() const;
  void setDepth(int);
  WidgetPrivate* d_ptr;
  WidgetPrivate* d_func() const { return d_ptr; }
};
#include "twin_private.hpp"
Widget::Widget(QObject* p) : QObject(p), d_ptr(new WidgetPrivate) {}
Widget::~Widget() { delete d_ptr; }
void Widget::onTick(int v) { d_func()->onTick(v); }
void Widget::reset() { d_func()->reset(); }
int Widget::depth() const { return d_func()->depth(); }
void Widget::setDepth(int d) { d_func()->setDepth(d); }
#include "probe.hpp"
int main(int argc, char** argv) { QCoreApplication app(argc, argv); Widget w; probe(w); }
