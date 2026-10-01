// rqt::Object<B>: the one base a class derives from to get a meta-object,
// the per-class meta-object variable, rqt::emit and the opt-in for Qt's own
// templates.
#pragma once

#include "dispatch.hpp"

#include <QtCore/QMetaObject>
#include <QtCore/QObject>

#include <atomic>
#include <concepts>
#include <cstring>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace rqt {
namespace detail {

template <class D>
constexpr QMetaObject::SuperData super_of();

}  // namespace detail

// The QMetaObject of D. A variable template, so it exists for every reflected
// class without a line in the class.
template <class D>
inline constexpr QMetaObject static_meta_object = {
    {detail::super_of<D>(), detail::meta_content<D>.staticData.stringdata, detail::meta_content<D>.staticData.data,
     &detail::static_metacall<D>, nullptr, detail::meta_content<D>.relocatingData.metaTypes, nullptr}};

namespace detail {

// rqt::Object<B> is transparent to Qt: a class deriving from it directly has B
// as its Qt superclass; a class deriving from another reflected class has that
// class's meta-object.
template <class D>
constexpr QMetaObject::SuperData super_of() {
  using Base = typename[:object_base_of(^^D):];
  if constexpr (is_object_instance(^^Base))
    return QMetaObject::SuperData::link<Base::qt_base::staticMetaObject>();
  else
    return QMetaObject::SuperData::link<static_meta_object<Base>>();
}

struct class_info {
  QMetaObject const* mo;
  class_info const* parent;  // null when the base is rqt::Object<B>
  int methods;
  int properties;
};

template <class D>
constexpr class_info const* parent_info();

template <class D>
inline constexpr class_info info_for{&static_meta_object<D>, parent_info<D>(), static_cast<int>(method_total<D>),
                                     static_cast<int>(property_count<D>)};

template <class D>
constexpr class_info const* parent_info() {
  using Base = typename[:object_base_of(^^D):];
  if constexpr (is_object_instance(^^Base))
    return nullptr;
  else
    return &info_for<Base>;
}

inline std::unordered_map<std::type_index, class_info const*>& registry() {
  static std::unordered_map<std::type_index, class_info const*> r;
  return r;
}

}  // namespace detail

template <class B = QObject>
class Object : public B, public object_tag {
 public:
  using qt_base = B;
  using B::B;
  Object() = default;

  // E2: `Worker() : rqt::Object<QThread>(this) {}`. Self is complete in a mem-initializer.
  template <class Self, class... A>
    requires std::derived_from<Self, Object>
  explicit Object(Self*, A&&... a) : B(std::forward<A>(a)...), info_(&detail::info_for<Self>) {}

  // E3: `Worker() { bind(); }`. The most derived constructor body runs last.
  template <class Self>
  void bind(this Self& self) {
    static_cast<Object&>(self).info_.store(&detail::info_for<Self>, std::memory_order_release);
  }

  const QMetaObject* metaObject() const override {
    if (::QObject::d_ptr->metaObject) return ::QObject::d_ptr->dynamicMetaObject();
    auto const* ci = info();
    return ci ? ci->mo : &B::staticMetaObject;
  }

  void* qt_metacast(const char* name) override {
    if (!name) return nullptr;
    for (auto const* ci = info(); ci; ci = ci->parent)
      if (!std::strcmp(name, ci->mo->className())) return static_cast<void*>(this);
    return B::qt_metacast(name);
  }

  int qt_metacall(QMetaObject::Call c, int id, void** a) override {
    auto const* ci = info();
    return ci ? dispatch(ci, c, id, a) : B::qt_metacall(c, id, a);
  }

 private:
  static bool is_property_call(QMetaObject::Call c) {
    return c == QMetaObject::ReadProperty || c == QMetaObject::WriteProperty || c == QMetaObject::ResetProperty ||
           c == QMetaObject::BindableProperty || c == QMetaObject::RegisterPropertyMetaType;
  }

  // moc's per-class qt_metacall chain, walked from the root down.
  int dispatch(detail::class_info const* ci, QMetaObject::Call c, int id, void** a) {
    id = ci->parent ? dispatch(ci->parent, c, id, a) : B::qt_metacall(c, id, a);
    if (id < 0) return id;
    if (c == QMetaObject::InvokeMetaMethod || c == QMetaObject::RegisterMethodArgumentMetaType) {
      if (id < ci->methods) ci->mo->d.static_metacall(this, c, id, a);
      id -= ci->methods;
    } else if (is_property_call(c)) {
      if (id < ci->properties) ci->mo->d.static_metacall(this, c, id, a);
      id -= ci->properties;
    }
    return id;
  }

  // E4: look the dynamic type up when nothing bound it.
  detail::class_info const* info() const {
    auto const* ci = info_.load(std::memory_order_acquire);
    if (!ci) {
      auto const& r = detail::registry();
      if (auto it = r.find(typeid(*this)); it != r.end()) {
        ci = it->second;
        info_.store(ci, std::memory_order_release);
      }
    }
    return ci;
  }

  mutable std::atomic<detail::class_info const*> info_{nullptr};
};

// E4: register every rqt::Object class declared directly in a namespace.
template <meta::info Ns>
bool register_namespace() {
  template for (constexpr auto m : std::define_static_array(meta::members_of(Ns, detail::unchecked))) {
    if constexpr (meta::is_type(m) && meta::is_class_type(m) && meta::is_complete_type(m)) {
      using T = [:m:];
      if constexpr (std::derived_from<T, object_tag> && !detail::is_object_instance(m))
        detail::registry()[typeid(T)] = &detail::info_for<T>;
    }
  }
  return true;
}

// --- signal emission -----------------------------------------------------------------------

namespace detail {
consteval int signal_index(info f) {
  if (kind_of(f) == method_kind::signal_ && meta::is_class_member(f)) return entry_index(meta::parent_of(f), f);
  throw meta::exception("rqt::emit used outside a [[=rqt::signal]] member function", f);
}
}  // namespace detail

struct signal_id {
  int index;
  consteval signal_id(meta::info caller = meta::current_function()) : index(detail::signal_index(caller)) {}
};

// In a signal body `this` has the type of the class that declares the signal,
// so C names the right meta-object even under multi-level inheritance.
template <class C>
struct emit {
  C* self;
  signal_id id;
  emit(C* s, signal_id i = {}) : self(s), id(i) {}
  template <class... A>
  void operator()(A const&... a) const {
    void* args[] = {nullptr, const_cast<void*>(static_cast<void const*>(&a))...};
    QMetaObject::activate(self, &static_meta_object<C>, id.index, args);
  }
};

}  // namespace rqt

// Only for classes that opted in with their own staticMetaObject. For the
// others Qt's own static_assert is the right answer: T::staticMetaObject would
// silently find QObject's, and qobject_cast would succeed for any QObject.
namespace QtPrivate {
template <class D>
  requires(std::derived_from<D, rqt::object_tag> && rqt::detail::declares_static_meta_object(^^D))
struct HasQ_OBJECT_Macro<D> {
  enum { Value = 1 };
};
}  // namespace QtPrivate

// Tier B, outside the class: QMetaObject const& T::staticMetaObject = rqt::static_meta_object<T>;
#define RQT_STATIC_META_OBJECT(T) QMetaObject const& T::staticMetaObject = ::rqt::static_meta_object<T>
