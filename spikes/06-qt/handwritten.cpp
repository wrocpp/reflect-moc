// Spike 06a: a Qt 6.10 QObject whose meta-object is written by hand, no moc.
//
// Question: is the moc output shape (QtMocHelpers::metaObjectData,
// qt_create_metaobjectdata, qt_static_metacall) something a library can
// produce itself? This file writes it by hand, following the shape moc 6.10
// printed for moc_probe.h, and checks that string-based connect,
// invokeMethod and QMetaProperty all work.
//
// Build (in the docker image): spikes/06-qt/build.sh
#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/qtmochelpers.h>

#include <cassert>
#include <cstdio>
#include <cstring>

class Counter : public QObject {
  Q_OBJECT

 public:
  int value() const { return value_; }
  void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    valueChanged(v);
  }
  int add(int n) {
    setValue(value_ + n);
    return value_;
  }
  void reset() { setValue(0); }
  void valueChanged(int value);

 private:
  int value_ = 0;
};

// --- what moc would have generated --------------------------------------------
namespace {
struct counter_tag {};
}  // namespace

template <>
constexpr inline auto Counter::qt_create_metaobjectdata<counter_tag>() {
  namespace QMC = QtMocConstants;
  QtMocHelpers::StringRefStorage qt_stringData{"Counter", "valueChanged", "", "value", "reset", "setValue", "v",
                                               "add",     "n"};
  QtMocHelpers::UintData qt_methods{
      QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{{QMetaType::Int, 3}}}),
      QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
      QtMocHelpers::SlotData<void(int)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{{QMetaType::Int, 6}}}),
      QtMocHelpers::MethodData<int(int)>(7, 2, QMC::AccessPublic, QMetaType::Int, {{{QMetaType::Int, 8}}}),
  };
  QtMocHelpers::UintData qt_properties{
      QtMocHelpers::PropertyData<int>(3, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet,
                                      0),
  };
  QtMocHelpers::UintData qt_enums{};
  return QtMocHelpers::metaObjectData<Counter, counter_tag>(QMC::MetaObjectFlag{}, qt_stringData, qt_methods,
                                                            qt_properties, qt_enums);
}

Q_CONSTINIT const QMetaObject Counter::staticMetaObject = {
    {QMetaObject::SuperData::link<QObject::staticMetaObject>(),
     qt_staticMetaObjectStaticContent<counter_tag>.stringdata, qt_staticMetaObjectStaticContent<counter_tag>.data,
     qt_static_metacall, nullptr, qt_staticMetaObjectRelocatingContent<counter_tag>.metaTypes, nullptr}};

void Counter::qt_static_metacall(QObject* o, QMetaObject::Call c, int id, void** a) {
  auto* t = static_cast<Counter*>(o);
  if (c == QMetaObject::InvokeMetaMethod) {
    switch (id) {
      case 0: t->valueChanged(*static_cast<int*>(a[1])); break;
      case 1: t->reset(); break;
      case 2: t->setValue(*static_cast<int*>(a[1])); break;
      case 3: {
        int r = t->add(*static_cast<int*>(a[1]));
        if (a[0]) *static_cast<int*>(a[0]) = r;
      } break;
      default: break;
    }
  }
  if (c == QMetaObject::IndexOfMethod) {
    if (QtMocHelpers::indexOfMethod<void (Counter::*)(int)>(a, &Counter::valueChanged, 0)) return;
  }
  if (c == QMetaObject::ReadProperty && id == 0) *static_cast<int*>(a[0]) = t->value();
  if (c == QMetaObject::WriteProperty && id == 0) t->setValue(*static_cast<int*>(a[0]));
}

const QMetaObject* Counter::metaObject() const {
  return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void* Counter::qt_metacast(const char* name) {
  if (!name) return nullptr;
  if (!std::strcmp(name, qt_staticMetaObjectStaticContent<counter_tag>.strings)) return static_cast<void*>(this);
  return QObject::qt_metacast(name);
}

int Counter::qt_metacall(QMetaObject::Call c, int id, void** a) {
  constexpr int methods = 4, properties = 1;
  id = QObject::qt_metacall(c, id, a);
  if (id < 0) return id;
  if (c == QMetaObject::InvokeMetaMethod) {
    if (id < methods) qt_static_metacall(this, c, id, a);
    id -= methods;
  }
  if (c == QMetaObject::RegisterMethodArgumentMetaType) {
    if (id < methods) *static_cast<QMetaType*>(a[0]) = QMetaType();
    id -= methods;
  }
  if (c == QMetaObject::ReadProperty || c == QMetaObject::WriteProperty || c == QMetaObject::ResetProperty ||
      c == QMetaObject::BindableProperty || c == QMetaObject::RegisterPropertyMetaType) {
    qt_static_metacall(this, c, id, a);
    id -= properties;
  }
  return id;
}

void Counter::valueChanged(int v) {
  void* args[] = {nullptr, &v};
  QMetaObject::activate(this, &staticMetaObject, 0, args);
}

// --- checks --------------------------------------------------------------------
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Counter a, b;

  bool ok = QObject::connect(&a, SIGNAL(valueChanged(int)), &b, SLOT(setValue(int)));
  assert(ok);
  a.setValue(5);
  assert(b.value() == 5);

  ok = QMetaObject::invokeMethod(&a, "setValue", Q_ARG(int, 7));
  assert(ok && a.value() == 7 && b.value() == 7);
  int sum = 0;
  ok = QMetaObject::invokeMethod(&a, "add", Q_RETURN_ARG(int, sum), Q_ARG(int, 3));
  assert(ok && sum == 10);

  QMetaObject const* mo = a.metaObject();
  QMetaProperty p = mo->property(mo->indexOfProperty("value"));
  assert(p.isValid() && p.read(&a).toInt() == 10);
  assert(p.write(&a, 12) && a.value() == 12 && b.value() == 12);
  assert(p.hasNotifySignal() && p.notifySignal().name() == "valueChanged");

  int seen = 0;
  QObject::connect(&a, &Counter::valueChanged, &a, [&](int v) { seen = v; });
  a.setValue(20);
  assert(seen == 20);

  QObject* o = &a;
  assert(qobject_cast<Counter*>(o) == &a);
  assert(std::strcmp(mo->className(), "Counter") == 0);

  std::puts("06a handwritten meta-object: SIGNAL/SLOT connect, invokeMethod, QMetaProperty, qobject_cast OK");
}
