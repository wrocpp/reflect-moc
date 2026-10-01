// Prints the part of a QMetaObject a class adds over its base, one fact per
// line, so a moc-built and a reflect-moc-built harness can be diffed.
#pragma once

#include <QtCore/QMetaClassInfo>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>

#include <cstdio>

namespace harness {

inline const char* method_kind(QMetaMethod const& m) {
  switch (m.methodType()) {
    case QMetaMethod::Signal: return "signal";
    case QMetaMethod::Slot: return "slot";
    case QMetaMethod::Method: return "method";
    case QMetaMethod::Constructor: return "constructor";
  }
  return "?";
}

inline const char* access_name(QMetaMethod const& m) {
  switch (m.access()) {
    case QMetaMethod::Private: return "private";
    case QMetaMethod::Protected: return "protected";
    case QMetaMethod::Public: return "public";
  }
  return "?";
}

inline void dump_meta(QMetaObject const* mo) {
  std::printf("meta %s : %s\n", mo->className(), mo->superClass() ? mo->superClass()->className() : "-");
  for (int i = mo->classInfoOffset(); i < mo->classInfoCount(); ++i)
    std::printf("  classinfo %s = %s\n", mo->classInfo(i).name(), mo->classInfo(i).value());
  for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
    QMetaMethod m = mo->method(i);
    std::printf("  %s %s %s %s(%s)\n", access_name(m), method_kind(m), m.typeName(), m.methodSignature().constData(),
                m.parameterNames().join(',').constData());
  }
  for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
    QMetaProperty p = mo->property(i);
    std::printf("  property %s %s%s%s%s%s notify=%s\n", p.typeName(), p.name(), p.isReadable() ? " read" : "",
                p.isWritable() ? " write" : "", p.isResettable() ? " reset" : "", p.isFinal() ? " final" : "",
                p.hasNotifySignal() ? p.notifySignal().methodSignature().constData() : "-");
  }
  for (int i = mo->enumeratorOffset(); i < mo->enumeratorCount(); ++i) {
    QMetaEnum e = mo->enumerator(i);
    std::printf("  enum %s%s keys=%d\n", e.name(), e.isFlag() ? " flag" : "", e.keyCount());
  }
  std::fflush(stdout);
}

}  // namespace harness
