// prints what a consumer can see through a namespace meta-object and a class property of a namespace enum
#pragma once
#include <QtCore/QCoreApplication>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaProperty>
#include <QtCore/QVariant>
#include <cstdio>
#include "differential_dump.hpp"
template <class Ns_E, class Holder>
void probe(QMetaObject const& ns, QMetaObject const& holder_mo, Holder& h, char const* flag_ns_name) {
  std::printf("ns: class=%s enums=%d classInfo=%d methods=%d props=%d super=%d\n", ns.className(), ns.enumeratorCount(), ns.classInfoCount(),
              ns.methodCount(), ns.propertyCount(), ns.superClass() != nullptr);
  for (int i = 0; i < ns.enumeratorCount(); ++i) {
    auto e = ns.enumerator(i);
    QString keys;
    for (int k = 0; k < e.keyCount(); ++k) keys += QString("%1=%2,").arg(e.key(k)).arg(e.value(k));
    std::printf(" enum[%d] %s enumName=%s scope=%s scoped=%d flag=%d keys=%s\n", i, e.name(), e.enumName(), e.scope(), e.isScoped(), e.isFlag(), qPrintable(keys));
  }
  auto fe = QMetaEnum::fromType<Ns_E>();
  std::printf("fromType<E>: valid=%d name=%s enumName=%s scope=%s b=%s\n", fe.isValid(), fe.name(), fe.enumName(), fe.scope(), fe.valueToKey(1));
  std::printf("QMetaType<E>.metaObject()==ns: %d\n", QMetaType::fromType<Ns_E>().metaObject() == &ns);
  std::printf("QMetaType<E>.name=%s\n", QMetaType::fromType<Ns_E>().name());
  int ip = holder_mo.indexOfProperty("e");
  QMetaProperty p = holder_mo.property(ip);
  std::printf("holder prop: type=%s isEnum=%d isFlag=%d enumerator.valid=%d enumerator.name=%s\n", p.typeName(), p.isEnumType(), p.isFlagType(), p.enumerator().isValid(), p.enumerator().name());
  QVariant v = p.read(&h);
  std::printf("read: %s typeName=%s\n", qPrintable(v.toString()), v.typeName());
  std::printf("write by key: %d\n", int(h.setProperty("e", QVariant("b"))));
  std::printf("read again: %s\n", qPrintable(p.read(&h).toString()));
  for (auto const& l : dump_class(&ns)) std::printf("DUMP %s\n", qPrintable(l));
  for (auto const& l : dump_class(&holder_mo)) std::printf("DUMP %s\n", qPrintable(l));
  (void)flag_ns_name;
}
