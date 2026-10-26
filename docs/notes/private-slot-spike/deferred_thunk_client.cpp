// a second translation unit that only has the public header (WidgetPrivate stays incomplete) and odr-uses the table entry
class WidgetPrivate;
class Widget {
 public:
  template <class W>
  static void thunk(W* w, void** a) { w->d_func()->onTick(*static_cast<int*>(a[1])); }
  static constexpr void (*fn)(Widget*, void**) = &thunk<Widget>;
  WidgetPrivate* d_func() const { return d_; }
 private:
  WidgetPrivate* d_;
};
void* use() { return reinterpret_cast<void*>(Widget::fn); }  // what any client does when it takes staticMetaObject
