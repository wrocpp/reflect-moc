// Spike 07: a NON-template base instead of CRTP, with C++23 deducing this.
//
// Question: without CRTP, how little must a user class write to get a Qt
// meta-object that stock Qt 6.10 accepts?
//
//   class rqt::Object : public ::QObject   (one class, no template parameter)
//
// overrides metaObject(), qt_metacall and qt_metacast ONCE. They dispatch
// through a per-instance pointer to a class_info that is generated per class
// from reflection (variable templates rqt::static_meta_object<T> and
// rqt::info_for<T>). The pointer is bound in one of three ways:
//   E2  Receiver() : rqt::Object(this) {}     base ctor template deduces Self
//   E3  Sub() { bind(); }                      deducing this, most-derived wins
//   E4  nothing in the class; rqt::register_namespace<^^app>() scans the
//       namespace once and metaObject() looks typeid(*this) up lazily
// Deducing this also carries the user-facing API (set<"name">, get<"name">),
// because it sees the static type at the call site.
//
// What cannot work without a per-class declaration is probed with -D macros:
//   SPIKE_QOBJECT_CAST_WORKER  qobject_cast<Worker*> (needs Worker::staticMetaObject)
//   SPIKE_PMF_CONNECT_WORKER   QObject::connect(&w, &Worker::sig, ...)
//   SPIKE_INLINE_SMO           the one-line staticMetaObject declared in-class
//
// Build (in the docker image): spikes/07-deducing-this/build.sh
#define QT_NO_KEYWORDS
#include <meta>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtCore/QVariant>
#include <QtCore/qtmochelpers.h>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdio>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rqt {
namespace meta = std::meta;

struct signal_t {};
inline constexpr signal_t signal{};
struct slot_t {};
inline constexpr slot_t slot{};
struct invokable_t {};
inline constexpr invokable_t invokable{};
struct property {
  bool notify = false;
};

class Object;

template <class Tag>
consteval bool has(meta::info r) {
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) return true;
  return false;
}

template <class Tag>
consteval Tag get_annotation(meta::info r) {
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) return meta::extract<Tag>(a);
  throw meta::exception("missing annotation", r);
}

// --- reflection tables (as in spike 06b, per class level) -------------------------

consteval std::vector<meta::info> methods_of(meta::info cls) {
  std::vector<meta::info> sigs, slots_, invs;
  for (auto m : meta::members_of(cls, meta::access_context::unchecked())) {
    if (!meta::is_function(m)) continue;
    if (has<signal_t>(m))
      sigs.push_back(m);
    else if (has<slot_t>(m))
      slots_.push_back(m);
    else if (has<invokable_t>(m))
      invs.push_back(m);
  }
  sigs.insert(sigs.end(), slots_.begin(), slots_.end());
  sigs.insert(sigs.end(), invs.begin(), invs.end());
  return sigs;
}

consteval std::size_t signal_count(meta::info cls) {
  std::size_t n = 0;
  for (auto m : methods_of(cls)) n += has<signal_t>(m);
  return n;
}

consteval std::vector<meta::info> properties_of(meta::info cls) {
  std::vector<meta::info> out;
  for (auto m : meta::nonstatic_data_members_of(cls, meta::access_context::unchecked()))
    if (has<property>(m)) out.push_back(m);
  return out;
}

consteval std::string_view param_name(meta::info p) {
  return meta::has_identifier(p) ? meta::identifier_of(p) : std::string_view{};
}

consteval std::vector<std::string_view> strings_of(meta::info cls) {
  std::vector<std::string_view> out{meta::identifier_of(cls), ""};
  auto add = [&out](std::string_view s) {
    if (std::ranges::find(out, s) == out.end()) out.push_back(s);
  };
  for (auto m : methods_of(cls)) {
    add(meta::identifier_of(m));
    for (auto p : meta::parameters_of(m)) add(param_name(p));
  }
  for (auto p : properties_of(cls)) add(meta::identifier_of(p));
  return out;
}

consteval uint string_index(meta::info cls, std::string_view s) {
  auto all = strings_of(cls);
  return static_cast<uint>(std::ranges::find(all, s) - all.begin());
}

consteval meta::info notify_signal(meta::info prop) {
  std::string name{meta::identifier_of(prop)};
  name += "Changed";
  for (auto m : methods_of(meta::parent_of(prop)))
    if (has<signal_t>(m) && meta::identifier_of(m) == name) return m;
  throw meta::exception("property has notify=true but no signal named " + name, prop);
}

consteval uint notify_index(meta::info prop) {
  if (!get_annotation<property>(prop).notify) return uint(-1);
  auto all = methods_of(meta::parent_of(prop));
  return static_cast<uint>(std::ranges::find(all, notify_signal(prop)) - all.begin());
}

consteval std::span<char const* const> string_list(meta::info cls) {
  std::vector<char const*> v;
  for (auto s : strings_of(cls)) v.push_back(std::define_static_string(s));
  return std::define_static_array(v);
}

template <class D>
struct string_table {
  static constexpr std::span<char const* const> list = string_list(^^D);
  static constexpr int StringCount = static_cast<int>(list.size());
  static constexpr std::size_t StringSize = [] {
    std::size_t n = 0;
    for (auto s : list) n += std::string_view{s}.size() + 1;
    return n;
  }();
  constexpr void writeTo(uint (&offsets)[2 * StringCount], char (&data)[StringSize]) const {
    uint offset = 0;
    for (int i = 0; i < StringCount; ++i) {
      std::string_view s{list[i]};
      for (std::size_t j = 0; j <= s.size(); ++j) data[offset + j] = list[i][j];
      offsets[2 * i] = offset + sizeof(offsets);
      offsets[2 * i + 1] = static_cast<uint>(s.size());
      offset += static_cast<uint>(s.size() + 1);
    }
  }
};

template <class T>
consteval uint type_id() {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::is_void_v<U>)
    return QMetaType::Void;
  else if constexpr (QMetaTypeId2<U>::IsBuiltIn)
    return QMetaTypeId2<U>::MetaType;
  else
    static_assert(false, "spike: only built-in meta types");
}

template <class Data, class D, meta::info M>
constexpr Data function_data() {
  using R = [:meta::return_type_of(M):];
  using Params = typename Data::ParametersArray;
  constexpr Params params = []<std::size_t... P>(std::index_sequence<P...>) {
    return Params{{{type_id<typename[:meta::type_of(meta::parameters_of(M)[P]):]>(),
                    string_index(^^D, param_name(meta::parameters_of(M)[P]))}...}};
  }(std::make_index_sequence<meta::parameters_of(M).size()>{});
  return Data(string_index(^^D, meta::identifier_of(M)), string_index(^^D, ""), QtMocConstants::AccessPublic,
              type_id<R>(), params);
}

template <class D, std::size_t I>
constexpr auto method_data() {
  constexpr meta::info m = methods_of(^^D)[I];
  using F = [:meta::type_of(m):];
  if constexpr (has<signal_t>(m))
    return function_data<QtMocHelpers::SignalData<F>, D, m>();
  else if constexpr (has<slot_t>(m))
    return function_data<QtMocHelpers::SlotData<F>, D, m>();
  else
    return function_data<QtMocHelpers::MethodData<F>, D, m>();
}

template <class D, std::size_t I>
constexpr auto property_data() {
  constexpr meta::info p = properties_of(^^D)[I];
  using T = [:meta::type_of(p):];
  namespace QMC = QtMocConstants;
  return QtMocHelpers::PropertyData<T>(string_index(^^D, meta::identifier_of(p)), type_id<T>(),
                                       QMC::DefaultPropertyFlags | QMC::Writable, notify_index(p));
}

template <class D>
struct meta_tag {};

template <class D>
constexpr auto meta_content = [] {
  auto methods = []<std::size_t... I>(std::index_sequence<I...>) {
    return QtMocHelpers::UintData{method_data<D, I>()...};
  }(std::make_index_sequence<methods_of(^^D).size()>{});
  auto properties = []<std::size_t... I>(std::index_sequence<I...>) {
    return QtMocHelpers::UintData{property_data<D, I>()...};
  }(std::make_index_sequence<properties_of(^^D).size()>{});
  QtMocHelpers::UintData enums{};
  return QtMocHelpers::metaObjectData<D, meta_tag<D>>(QtMocConstants::MetaObjectFlag{}, string_table<D>{}, methods,
                                                      properties, enums);
}();

// --- dispatch ---------------------------------------------------------------------

template <meta::info Fn, class C>
void call_with_args(C* self, void** args) {
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    using R = [:meta::return_type_of(Fn):];
    if constexpr (std::is_void_v<R>) {
      self->[:Fn:](*static_cast<std::remove_cvref_t<typename[:meta::type_of(meta::parameters_of(Fn)[I]):]>*>(
          args[I + 1])...);
    } else {
      R r = self->[:Fn:](*static_cast<std::remove_cvref_t<typename[:meta::type_of(meta::parameters_of(Fn)[I]):]>*>(
          args[I + 1])...);
      if (args[0]) *static_cast<R*>(args[0]) = std::move(r);
    }
  }(std::make_index_sequence<meta::parameters_of(Fn).size()>{});
}

template <meta::info P, class D>
void write_property(D* t, void* v) {
  using T = [:meta::type_of(P):];
  auto& value = *static_cast<T*>(v);
  if (t->[:P:] == value) return;
  t->[:P:] = value;
  if constexpr (get_annotation<property>(P).notify) {
    constexpr auto sig = notify_signal(P);
    if constexpr (meta::parameters_of(sig).size() == 0)
      t->[:sig:]();
    else
      t->[:sig:](t->[:P:]);
  }
}

template <class D>
void static_metacall(::QObject* o, QMetaObject::Call c, int id, void** a) {
  auto* t = static_cast<D*>(o);
  constexpr std::size_t methods = methods_of(^^D).size();
  constexpr std::size_t props = properties_of(^^D).size();
  if (c == QMetaObject::InvokeMetaMethod) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((id == int(I) ? (call_with_args<methods_of(^^D)[I]>(t, a), true) : false) || ...);
    }(std::make_index_sequence<methods>{});
  }
  // Empty packs are skipped with if constexpr: an empty || fold is a
  // statement with no effect (-Wunused-value), and wrapping the fold in
  // (void)(...) instead makes GCC 16.2 escalate the lambda to consteval
  // ("call to consteval function '<lambda closure object>...' is not a
  // constant expression", "'id' is not a constant expression").
  if constexpr (signal_count(^^D) > 0)
    if (c == QMetaObject::IndexOfMethod) {
      [&]<std::size_t... I>(std::index_sequence<I...>) {
        (QtMocHelpers::indexOfMethod<decltype(&[:methods_of(^^D)[I]:])>(a, &[:methods_of(^^D)[I]:], int(I)) || ...);
      }(std::make_index_sequence<signal_count(^^D)>{});
    }
  if constexpr (props > 0)
  if (c == QMetaObject::ReadProperty) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((id == int(I) ? (*static_cast<typename[:meta::type_of(properties_of(^^D)[I]):]*>(a[0]) =
                            t->[:properties_of(^^D)[I]:],
                        true)
                     : false) ||
       ...);
    }(std::make_index_sequence<props>{});
  }
  if constexpr (props > 0)
  if (c == QMetaObject::WriteProperty) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((id == int(I) ? (write_property<properties_of(^^D)[I]>(t, a[0]), true) : false) || ...);
    }(std::make_index_sequence<props>{});
  }
}

// --- per-class data, found without CRTP --------------------------------------------

// The single direct base of D, as a type reflection.
consteval meta::info base_of(meta::info cls) {
  return meta::type_of(meta::bases_of(cls, meta::access_context::unchecked())[0]);
}

template <class D>
constexpr QMetaObject::SuperData super_of();

template <class D>
inline constexpr QMetaObject static_meta_object = {
    {super_of<D>(), meta_content<D>.staticData.stringdata, meta_content<D>.staticData.data, &static_metacall<D>,
     nullptr, meta_content<D>.relocatingData.metaTypes, nullptr}};

// rqt::Object itself is transparent: a class deriving from it directly has
// QObject as its Qt superclass.
template <class D>
constexpr QMetaObject::SuperData super_of() {
  using B = [:base_of(^^D):];
  if constexpr (std::is_same_v<B, Object>)
    return QMetaObject::SuperData::link<::QObject::staticMetaObject>();
  else
    return QMetaObject::SuperData::link<static_meta_object<B>>();
}

struct class_info {
  QMetaObject const* mo;
  class_info const* parent;  // null when the base is rqt::Object
  int methods;
  int properties;
};

template <class D>
constexpr class_info const* parent_info();

template <class D>
inline constexpr class_info info_for{&static_meta_object<D>, parent_info<D>(),
                                     static_cast<int>(methods_of(^^D).size()),
                                     static_cast<int>(properties_of(^^D).size())};

template <class D>
constexpr class_info const* parent_info() {
  using B = [:base_of(^^D):];
  if constexpr (std::is_same_v<B, Object>)
    return nullptr;
  else
    return &info_for<B>;
}

inline std::unordered_map<std::type_index, class_info const*>& registry() {
  static std::unordered_map<std::type_index, class_info const*> r;
  return r;
}

// --- the base: one class, no template parameter --------------------------------------
class Object : public ::QObject {
 public:
  explicit Object(::QObject* parent = nullptr) : ::QObject(parent) {}

  // E2: `Derived() : rqt::Object(this) {}`. Self is complete in a mem-initializer.
  template <class Self>
    requires std::derived_from<Self, Object>
  explicit Object(Self*, ::QObject* parent = nullptr) : ::QObject(parent), info_(&info_for<Self>) {}

  // E3: `Derived() { bind(); }`. The most-derived constructor body runs last.
  template <class Self>
  void bind(this Self& self) {
    static_cast<Object&>(self).info_ = &info_for<Self>;
  }

  const QMetaObject* metaObject() const override {
    if (::QObject::d_ptr->metaObject) return ::QObject::d_ptr->dynamicMetaObject();
    auto const* ci = info();
    return ci ? ci->mo : &::QObject::staticMetaObject;
  }

  void* qt_metacast(const char* name) override {
    if (!name) return nullptr;
    for (auto const* ci = info(); ci; ci = ci->parent)
      if (!std::strcmp(name, ci->mo->className())) return static_cast<void*>(this);
    return ::QObject::qt_metacast(name);
  }

  int qt_metacall(QMetaObject::Call c, int id, void** a) override {
    auto const* ci = info();
    return ci ? dispatch(ci, c, id, a) : ::QObject::qt_metacall(c, id, a);
  }

 private:
  // moc's per-class qt_metacall chain, walked from the root down.
  int dispatch(class_info const* ci, QMetaObject::Call c, int id, void** a) {
    id = ci->parent ? dispatch(ci->parent, c, id, a) : ::QObject::qt_metacall(c, id, a);
    if (id < 0) return id;
    if (c == QMetaObject::InvokeMetaMethod) {
      if (id < ci->methods) ci->mo->d.static_metacall(this, c, id, a);
      id -= ci->methods;
    }
    if (c == QMetaObject::RegisterMethodArgumentMetaType) {
      if (id < ci->methods) *static_cast<QMetaType*>(a[0]) = QMetaType();
      id -= ci->methods;
    }
    if (c == QMetaObject::ReadProperty || c == QMetaObject::WriteProperty || c == QMetaObject::ResetProperty ||
        c == QMetaObject::BindableProperty || c == QMetaObject::RegisterPropertyMetaType) {
      if (id < ci->properties) ci->mo->d.static_metacall(this, c, id, a);
      id -= ci->properties;
    }
    return id;
  }

  // E4: look the dynamic type up once when nothing bound it.
  class_info const* info() const {
    if (!info_) {
      auto it = registry().find(typeid(*this));
      if (it != registry().end()) info_ = it->second;
    }
    return info_;
  }

  mutable class_info const* info_ = nullptr;

 public:
  // Deducing this: Self is the static type at the call site.
  template <std::size_t N>
  struct name {
    char data[N]{};
    consteval name(char const (&s)[N]) { std::copy_n(s, N, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
  };

  template <name N, class Self>
  static consteval meta::info property_named() {
    for (auto p : properties_of(^^Self))
      if (meta::identifier_of(p) == N.view()) return p;
    throw meta::exception("no rqt::property named " + std::string{N.view()}, ^^Self);
  }

  template <name N, class Self, class V>
  void set(this Self& self, V&& v) {
    constexpr auto p = property_named<N, Self>();
    typename[:meta::type_of(p):] value = std::forward<V>(v);
    write_property<p>(&self, &value);
  }

  template <name N, class Self>
  auto get(this Self const& self) {
    return self.[:property_named<N, Self>():];
  }
};

// E4: register every rqt::Object class declared directly in a namespace.
template <meta::info Ns>
bool register_namespace() {
  template for (constexpr auto m : std::define_static_array(meta::members_of(Ns, meta::access_context::unchecked()))) {
    if constexpr (meta::is_type(m) && meta::is_class_type(m) && meta::is_complete_type(m)) {
      using T = [:m:];
      if constexpr (std::derived_from<T, Object> && !std::is_same_v<T, Object>) registry()[typeid(T)] = &info_for<T>;
    }
  }
  return true;
}

// --- signal emission (spike 05 form (a)) ------------------------------------------------
consteval int signal_index(meta::info f) {
  if (meta::is_function(f) && meta::is_class_member(f) && has<signal_t>(f)) {
    auto all = methods_of(meta::parent_of(f));
    return static_cast<int>(std::ranges::find(all, f) - all.begin());
  }
  throw meta::exception("rqt::emit used outside a [[=rqt::signal]] member function", f);
}

struct signal_id {
  int index;
  consteval signal_id(meta::info caller = meta::current_function()) : index(signal_index(caller)) {}
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

// --- replacements for the Qt templates that need T::staticMetaObject ------------------
template <class T>
T* cast(::QObject* o) {
  return static_cast<T*>(static_meta_object<T>.cast(o));
}

template <class C, class F>
int method_index(F C::* pmf) {
  constexpr std::size_t n = methods_of(^^C).size();
  return [pmf]<std::size_t... I>(std::index_sequence<I...>) {
    int r = -1;
    (
        [&] {
          if constexpr (std::is_same_v<decltype(&[:methods_of(^^C)[I]:]), F C::*>)
            if (r < 0 && &[:methods_of(^^C)[I]:] == pmf) r = static_cast<int>(I);
        }(),
        ...);
    return r < 0 ? -1 : static_meta_object<C>.methodOffset() + r;
  }(std::make_index_sequence<n>{});
}

// The slot's parameters must be a prefix of the signal's. Index-based
// QMetaObject::connect does NOT check this (it connected finished() to
// setProgress(int) and returned a valid connection), so the check is ours.
template <class SF, class RF>
struct args_prefix : std::false_type {};
template <class... S, class... R, class SRet, class RRet>
struct args_prefix<SRet(S...), RRet(R...)> {
  static constexpr bool value = [] {
    if constexpr (sizeof...(R) > sizeof...(S))
      return false;
    else
      return []<std::size_t... I>(std::index_sequence<I...>) {
        using SL = std::tuple<std::remove_cvref_t<S>...>;
        return (std::is_same_v<std::tuple_element_t<I, SL>, std::remove_cvref_t<R>> && ...);
      }(std::index_sequence_for<R...>{});
  }();
};
template <class SF, class RF>
struct args_prefix<SF, RF const> : args_prefix<SF, RF> {};

template <class F>
struct strip_const_fn {
  using type = F;
};
template <class R, class... A>
struct strip_const_fn<R(A...) const> {
  using type = R(A...);
};

// Signal -> slot by index (QMetaObject::connect is public API). Functor slots
// have no public index-based entry point.
template <class S, class SF, class R, class RF>
QMetaObject::Connection connect(::QObject const* sender, SF S::* signal, ::QObject const* receiver, RF R::* slot,
                                Qt::ConnectionType type = Qt::AutoConnection) {
  static_assert(args_prefix<SF, typename strip_const_fn<RF>::type>::value,
                "rqt::connect: the slot's parameters must be a prefix of the signal's");
  return QMetaObject::connect(sender, method_index(signal), receiver, method_index(slot), type);
}

template <class D>
consteval bool declares_static_meta_object() {
  for (auto m : meta::members_of(^^D, meta::access_context::unchecked()))
    if (meta::has_identifier(m) && meta::identifier_of(m) == "staticMetaObject") return true;
  return false;
}
}  // namespace rqt

// Only for classes that opted in with their own staticMetaObject: for the
// others, Qt's own static_assert is the right answer, because
// T::staticMetaObject would silently find QObject's.
namespace QtPrivate {
template <class D>
  requires(std::derived_from<D, rqt::Object> && rqt::declares_static_meta_object<D>())
struct HasQ_OBJECT_Macro<D> {
  enum { Value = 1 };
};
}  // namespace QtPrivate

// --- user classes --------------------------------------------------------------------------
namespace app {
// E4: not one line of binding code.
struct Worker : rqt::Object {
  [[= rqt::property{.notify = true}]] int progress = 0;
  [[= rqt::signal]] void progressChanged(int progress) { rqt::emit{this}(progress); }
  [[= rqt::slot]] void setProgress(int p) {
    if (p == progress) return;
    progress = p;
    progressChanged(p);
  }
  [[= rqt::invokable]] int twice(int n) const { return 2 * n; }
};

// E3 and multi-level inheritance.
struct Sub : Worker {
  Sub() { bind(); }
  [[= rqt::signal]] void finished() { rqt::emit{this}(); }
  [[= rqt::slot]] void finish() { finished(); }
};

// E2.
struct Receiver : rqt::Object {
  Receiver() : rqt::Object(this) {}
  std::atomic<int> got{0};
  QThread* thread_seen = nullptr;
  QSemaphore done;
  [[= rqt::slot]] void onProgress(int p) {
    got = p;
    thread_seen = QThread::currentThread();
    done.release();
  }
};
}  // namespace app

static bool const app_registered = rqt::register_namespace<^^app>();

// The opt-in: one declaration in the class plus one definition after it.
struct Gadget : rqt::Object {
#ifdef SPIKE_INLINE_SMO
  static constexpr QMetaObject const& staticMetaObject = rqt::static_meta_object<Gadget>;
#else
  static QMetaObject const& staticMetaObject;
#endif
  Gadget() { bind(); }
  [[= rqt::property{.notify = true}]] int value = 0;
  [[= rqt::signal]] void valueChanged(int value) { rqt::emit{this}(value); }
  [[= rqt::slot]] void setValue(int v) {
    if (v == value) return;
    value = v;
    valueChanged(v);
  }
};
#ifndef SPIKE_INLINE_SMO
QMetaObject const& Gadget::staticMetaObject = rqt::static_meta_object<Gadget>;
#endif

static void check_core() {
  using namespace app;
  assert(app_registered);
  Worker w;
  Sub s;
  Receiver r;

  // E4 / E3 / E2 binding
  assert(std::strcmp(w.metaObject()->className(), "Worker") == 0);
  assert(std::strcmp(s.metaObject()->className(), "Sub") == 0);
  assert(s.metaObject()->superClass() == &rqt::static_meta_object<Worker>);
  assert(w.metaObject()->superClass() == &QObject::staticMetaObject);
  assert(std::strcmp(r.metaObject()->className(), "Receiver") == 0);
  assert(s.inherits("Worker") && s.inherits("QObject") && !w.inherits("Sub"));
  assert(s.metaObject()->methodCount() - QObject::staticMetaObject.methodCount() == 3 + 2);

  // string connect, including an inherited slot on a multi-level class
  Worker w2;
  bool ok = QObject::connect(&w, SIGNAL(progressChanged(int)), &s, SLOT(setProgress(int)));
  assert(ok);
  w.setProgress(5);
  assert(s.progress == 5);
  ok = QMetaObject::invokeMethod(&s, "finish");
  assert(ok);

  // invokeMethod, inherited and own, with a return value from a const method
  ok = QMetaObject::invokeMethod(&s, "setProgress", Q_ARG(int, 8));
  assert(ok && s.progress == 8);
  int twice = 0;
  ok = QMetaObject::invokeMethod(&s, "twice", Q_RETURN_ARG(int, twice), Q_ARG(int, 21));
  assert(ok && twice == 42);

  // QMetaProperty through the chain
  QMetaProperty p = s.metaObject()->property(s.metaObject()->indexOfProperty("progress"));
  assert(p.isValid() && p.write(&s, 11) && s.progress == 11 && p.read(&s).toInt() == 11);
  assert(p.hasNotifySignal() && p.notifySignal().name() == "progressChanged");

  // deducing this: set/get by name, with NOTIFY
  QObject::connect(&w, SIGNAL(progressChanged(int)), &w2, SLOT(setProgress(int)));
  w.set<"progress">(7);
  assert(w.get<"progress">() == 7 && w2.progress == 7);

  // QMetaObject::connect by index, via rqt::connect with PMFs (no Q_OBJECT check)
  Sub s2;
#ifdef SPIKE_INDEX_CONNECT_MISMATCH
  rqt::connect(&s, &Sub::finished, &s2, &Worker::setProgress);
#endif
  auto conn = rqt::connect(&w, &Worker::progressChanged, &s2, &Worker::setProgress);
  assert(conn);
  w.setProgress(13);
  assert(s2.progress == 13);

  // rqt::cast replaces qobject_cast
  QObject* o = &s;
  assert(rqt::cast<Sub>(o) == &s && rqt::cast<Worker>(o) == &s && rqt::cast<Receiver>(o) == nullptr);
#ifdef SPIKE_QOBJECT_CAST_WORKER
  (void)qobject_cast<Worker*>(o);
#endif
#ifdef SPIKE_PMF_CONNECT_WORKER
  QObject::connect(&w, &Worker::progressChanged, &w, [](int) {});
#endif

  // queued across a QThread: string connect, then rqt::connect
  QThread t;
  r.moveToThread(&t);
  t.start();
  auto c1 = QObject::connect(&w, SIGNAL(progressChanged(int)), &r, SLOT(onProgress(int)), Qt::QueuedConnection);
  assert(c1);
  w.setProgress(21);
  r.done.acquire();
  assert(r.got == 21 && r.thread_seen == &t);
  QObject::disconnect(c1);
  auto c2 = rqt::connect(&w, &Worker::progressChanged, &r, &Receiver::onProgress, Qt::QueuedConnection);
  assert(c2);
  w.setProgress(22);
  r.done.acquire();
  assert(r.got == 22 && r.thread_seen == &t);
  t.quit();
  t.wait();

  // the opt-in class: Qt's own templates work
  Gadget g;
  QObject* go = &g;
  assert(qobject_cast<Gadget*>(go) == &g);
  assert(qobject_cast<Gadget*>(o) == nullptr);
  int seen = 0;
  QObject::connect(&g, &Gadget::valueChanged, &g, [&](int v) { seen = v; });
  g.setValue(4);
  assert(seen == 4);
}

static void check_qml() {
  QQmlEngine engine;
  app::Worker worker;  // E4-bound, exposed as a context property
  engine.rootContext()->setContextProperty("worker", &worker);
  int type = qmlRegisterType<Gadget>("demo", 1, 0, "Gadget");
  assert(type >= 0);
  QQmlComponent component(&engine);
  component.setData(R"(
    import QtQml
    import demo
    QtObject {
      id: root
      property int seen: worker.progress
      property int last: -1
      property int gadgetSeen: -1
      property var watcher: Connections {
        target: worker
        function onProgressChanged(progress) { root.last = progress }
      }
      property var gadget: Gadget {
        value: 3
        onValueChanged: function(value) { root.gadgetSeen = value }
      }
      function callTwice(n) { return worker.twice(n) }
      function bumpGadget() { gadget.setValue(9) }
    }
  )",
                    QUrl());
  std::unique_ptr<QObject> root{component.create()};
  if (!root) {
    std::printf("QML errors: %s\n", component.errorString().toUtf8().constData());
    assert(false);
  }
  worker.setProgress(42);
  assert(root->property("seen").toInt() == 42 && root->property("last").toInt() == 42);
  QVariant r;
  QMetaObject::invokeMethod(root.get(), "callTwice", Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, 5));
  assert(r.toInt() == 10);
  auto* gadget = root->property("gadget").value<QObject*>();
  assert(gadget && rqt::cast<Gadget>(gadget) && rqt::cast<Gadget>(gadget)->value == 3);
  QMetaObject::invokeMethod(root.get(), "bumpGadget");
  assert(rqt::cast<Gadget>(gadget)->value == 9 && root->property("gadgetSeen").toInt() == 9);
  std::printf("  QML-created Gadget dynamic type: %s, className: %s\n", typeid(*gadget).name(),
              gadget->metaObject()->className());
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  check_core();
  std::puts(
      "07 non-template base: E2/E3/E4 binding, multi-level, string + index connect, queued across QThread, "
      "invokeMethod, QMetaProperty, set/get by name, rqt::cast, opt-in qobject_cast + PMF connect OK");
  check_qml();
  std::puts("07 QML: context property binding + onProgressChanged(progress), invokable, qmlRegisterType<Gadget> OK");
}
