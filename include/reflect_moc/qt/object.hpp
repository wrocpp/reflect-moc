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

// The tier B opt-in as one line inside the class:
//   static inline QMetaObject const& staticMetaObject = rqt::meta_of<Worker>();   (RQT_META_OBJECT(Worker);)
// meta_of has a declared return type, so GCC instantiates its body at the end of
// the translation unit, when T is complete; a variable template named in the
// initializer instead would see the incomplete class.
template <class T>
QMetaObject const& meta_of();

template <class T>
QMetaObject const& meta_of() {
  return static_meta_object<T>;
}

namespace detail {

// rqt::Object<B> is transparent to Qt: a class deriving from it directly has B
// as its Qt superclass; a class deriving from another reflected class has that
// class's meta-object.
// A base that rqt generates (RQT_OBJECT or a mixin subclass) has its meta-object in the
// variable template; a Qt class (QObject, QWidget, a moc'd class) has its own staticMetaObject.
template <class D>
constexpr QMetaObject::SuperData super_of() {
  constexpr info base = super_type(^^D);
  using Base = typename[:base:];
  if constexpr (is_object_instance(base))
    return QMetaObject::SuperData::link<Base::qt_base::staticMetaObject>();
  else if constexpr (is_reflected_class(base))
    return QMetaObject::SuperData::link<static_meta_object<Base>>();
  else
    return QMetaObject::SuperData::link<Base::staticMetaObject>();
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
  constexpr info base = super_type(^^D);
  using Base = typename[:base:];
  if constexpr (is_object_instance(base) || !is_reflected_class(base))
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

  // There is no constructor that takes `this` (spike E2): it cannot be told
  // apart from a parent pointer to another reflected object, and a wrong guess
  // silently loses the parent.

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

// The constraint for a forwarding constructor template: it must not capture a
// copy or move of the class itself. Without it Base(A&&...) makes the class look
// move-constructible, and Qt's QMetaType for the class then instantiates a move
// that forwards a Base to QWidget(const QWidget&), which is deleted.
template <class Self, class... A>
concept not_copy_or_move = !(sizeof...(A) == 1 && (std::is_base_of_v<Self, std::remove_cvref_t<A>> && ...));

// ... and it must accept only arguments the base accepts. Otherwise QML sees a
// class constructible from anything (a QJSValue) and treats it as a value type.
template <class Self, class... A>
concept forwardable = not_copy_or_move<Self, A...> && std::constructible_from<Object<typename Self::qt_base>, A...>;

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
  throw meta::exception("rqt::activate used outside a [[=rqt::signal_function]] member function", f);
}
}  // namespace detail

struct signal_id {
  int index;
  consteval signal_id(meta::info caller = meta::current_function()) : index(detail::signal_index(caller)) {}
};

// In a signal body `this` has the type of the class that declares the signal,
// so C names the right meta-object even under multi-level inheritance.
#define RQT_DEFINE_ACTIVATOR(Name)                                                          \
  template <class C>                                                                        \
  struct Name {                                                                             \
    C* self;                                                                                \
    signal_id id;                                                                           \
    Name(C* s, signal_id i = {}) : self(s), id(i) {}                                        \
    template <class... A>                                                                   \
    void operator()(A const&... a) const {                                                  \
      void* args[] = {nullptr, const_cast<void*>(static_cast<void const*>(&a))...};         \
      QMetaObject::activate(self, &static_meta_object<C>, id.index, args);                  \
    }                                                                                       \
  };

RQT_DEFINE_ACTIVATOR(activate)
#ifdef QT_NO_KEYWORDS
RQT_DEFINE_ACTIVATOR(emit)  // the earlier spelling; with Qt's keywords on, `emit` is a macro
#endif
#undef RQT_DEFINE_ACTIVATOR

// --- RQT_OBJECT: what Q_OBJECT declares, defined through reflection ------------------------------------

// meta_of_class has a declared return type, so its body (reflection on the class) is compiled
// when the class is complete.
template <meta::info C>
QMetaObject const& meta_of_class();

template <meta::info C>
QMetaObject const& meta_of_class() {
  return static_meta_object<typename[:C:]>;
}

namespace detail {

inline bool is_property_call(QMetaObject::Call c) {
  return c == QMetaObject::ReadProperty || c == QMetaObject::WriteProperty || c == QMetaObject::ResetProperty ||
         c == QMetaObject::BindableProperty || c == QMetaObject::RegisterPropertyMetaType;
}

// moc's qt_metacall for one class, after its base has taken its share of the id.
template <class D>
int metacall_level(QObject* o, QMetaObject::Call c, int id, void** a) {
  constexpr int methods = static_cast<int>(method_total<D>);
  constexpr int properties = static_cast<int>(property_count<D>);
  if (c == QMetaObject::InvokeMetaMethod || c == QMetaObject::RegisterMethodArgumentMetaType) {
    if (id < methods) static_metacall<D>(o, c, id, a);
    id -= methods;
  } else if (is_property_call(c)) {
    if (id < properties) static_metacall<D>(o, c, id, a);
    id -= properties;
  }
  return id;
}

}  // namespace detail

namespace detail {

// What Q_INTERFACES adds to moc's qt_metacast, found by reflection: a direct base that is not the
// QObject base and has an interface id (Q_DECLARE_INTERFACE specializes qobject_interface_iid).
template <meta::info C, std::size_t I, class Self>
void interface_base(Self* self, char const* name, void*& found) {
  using B = typename[:meta::type_of(meta::bases_of(C, unchecked)[I]):];
  if constexpr (!std::is_base_of_v<QObject, B>) {
    char const* const iid = qobject_interface_iid<B*>();
    if (!found && iid && !std::strcmp(name, iid)) found = static_cast<void*>(static_cast<B*>(self));
  }
}

template <meta::info C, class Self>
void* interface_cast(Self* self, char const* name) {
  void* found = nullptr;
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (interface_base<C, I>(self, name, found), ...);
  }(std::make_index_sequence<meta::bases_of(C, unchecked).size()>{});
  return found;
}

}  // namespace detail

template <meta::info C, class Obj>
void* metacast_impl(Obj* self, char const* name) {
  using Self = typename[:C:];
  using Base = typename[:detail::super_type(C):];
  if (!name) return nullptr;
  if (!std::strcmp(name, static_meta_object<Self>.className())) return static_cast<void*>(self);
  if (void* const interface = detail::interface_cast<C>(self, name)) return interface;
  return self->Base::qt_metacast(name);
}

template <meta::info C, class Obj>
int metacall_impl(Obj* self, QMetaObject::Call c, int id, void** a) {
  using Self = typename[:C:];
  using Base = typename[:detail::super_type(C):];
  id = self->Base::qt_metacall(c, id, a);
  if (id < 0) return id;
  return detail::metacall_level<Self>(self, c, id, a);
}

template <meta::info C>
void static_call(QObject* o, QMetaObject::Call c, int id, void** a) {
  detail::static_metacall<typename[:C:]>(o, c, id, a);
}

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

// Tier B in one line, inside the class. Every class that wants qobject_cast and Qt's own connect
// writes its own line, including a class derived from another reflected class.
#define RQT_META_OBJECT(T) static inline QMetaObject const& staticMetaObject = ::rqt::meta_of<T>()

// The older two-line form (deprecated), outside the class:
//   inline QMetaObject const& T::staticMetaObject = rqt::static_meta_object<T>;
// inline, so the class can live in a header included by several translation units.
#define RQT_STATIC_META_OBJECT(T) inline QMetaObject const& T::staticMetaObject = ::rqt::static_meta_object<T>
