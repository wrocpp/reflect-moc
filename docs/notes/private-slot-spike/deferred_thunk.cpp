// Can a header name d_func()->onTick(int) before WidgetPrivate is complete, and have it resolved later?
#include <cstdio>
class WidgetPrivate;
class Widget {
 public:
  Widget();
  ~Widget();
  // what a macro RQT_PRIVATE_SLOT(d_func(), onTick, void(int)) could expand to inside the class
  template <class W>
  static void thunk(W* w, void** a) { w->d_func()->onTick(*static_cast<int*>(a[1])); }
  static constexpr void (*fn)(Widget*, void**) = &thunk<Widget>;
  WidgetPrivate* d_func() const { return d_; }
 private:
  WidgetPrivate* d_;
};
class WidgetPrivate { public: int got = 0; void onTick(int v) { got = v; } };  // widget_p.h, included by widget.cpp
Widget::Widget() : d_(new WidgetPrivate) {}
Widget::~Widget() { delete d_; }
int main() { Widget w; int v = 7; void* a[] = {nullptr, &v}; Widget::fn(&w, a); std::printf("got=%d\n", w.d_func()->got); }
