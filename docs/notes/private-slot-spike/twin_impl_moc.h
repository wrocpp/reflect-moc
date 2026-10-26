#pragma once
// the private class and the definitions, widget.cpp / widget_p.h in a real project
#include "twin.h"
class WidgetPrivate { public: int depth_ = 3; int depth() const { return depth_; } void setDepth(int d) { depth_ = d; } void onTick(int v) { ticks += v; } void reset() { ticks = 0; } int ticks = 0; };
Widget::Widget(QObject* p) : QObject(p), d_ptr(new WidgetPrivate) {}
Widget::~Widget() { delete d_ptr; }
