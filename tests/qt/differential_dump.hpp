// A full textual dump of one class level of a QMetaObject (own members only), used to compare the
// meta-object that moc generates with the one reflect-moc generates for the same class.
#pragma once

#include <QtCore/QMetaEnum>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QStringList>

inline QString strip_namespace(QString s) { return s.remove("viamoc::").remove("viarqt::"); }

inline QString join_names(QList<QByteArray> const& names) {
  QStringList out;
  for (auto const& n : names) out << QString::fromUtf8(n);
  return out.join('|');
}

inline QString parameter_type_names(QMetaMethod const& m) {
  QStringList names;
  for (int p = 0; p < m.parameterCount(); ++p) names << QString::fromUtf8(m.parameterTypeName(p));
  return names.join('|');
}

inline QStringList dump_methods(QMetaObject const* mo) {
  QStringList out;
  for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
    QMetaMethod const m = mo->method(i);
    out << strip_namespace(QString{"method %1 %2 access=%3 type=%4 attrs=%5 ret=%6 params=%7 names=%8 tag='%9' rev=%10"}
                               .arg(i - mo->methodOffset())
                               .arg(QString::fromUtf8(m.methodSignature()))
                               .arg(static_cast<int>(m.access()))
                               .arg(static_cast<int>(m.methodType()))
                               .arg(m.attributes())
                               .arg(QString::fromUtf8(m.typeName()))
                               .arg(parameter_type_names(m))
                               .arg(join_names(m.parameterNames()))
                               .arg(QString::fromUtf8(m.tag()))
                               .arg(m.revision()));
  }
  return out;
}

inline QStringList dump_properties(QMetaObject const* mo) {
  QStringList out;
  for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
    QMetaProperty const p = mo->property(i);
    QString flags;
    flags += p.isReadable() ? 'R' : '-';
    flags += p.isWritable() ? 'W' : '-';
    flags += p.isResettable() ? 'r' : '-';
    flags += p.isDesignable() ? 'D' : '-';
    flags += p.isScriptable() ? 'S' : '-';
    flags += p.isStored() ? 'T' : '-';
    flags += p.isUser() ? 'U' : '-';
    flags += p.isConstant() ? 'C' : '-';
    flags += p.isFinal() ? 'F' : '-';
    flags += p.isRequired() ? 'Q' : '-';
    flags += p.isEnumType() ? 'E' : '-';
    flags += p.isFlagType() ? 'G' : '-';
    flags += p.hasStdCppSet() ? 's' : '-';
    flags += p.isBindable() ? 'B' : '-';
    out << strip_namespace(QString{"property %1 %2 type=%3 flags=%4 notify=%5 rev=%6"}
                               .arg(i - mo->propertyOffset())
                               .arg(QString::fromUtf8(p.name()))
                               .arg(QString::fromUtf8(p.typeName()))
                               .arg(flags)
                               .arg(p.hasNotifySignal() ? p.notifySignalIndex() - mo->methodOffset() : -1)
                               .arg(p.revision()));
  }
  return out;
}

inline QStringList dump_enums(QMetaObject const* mo) {
  QStringList out;
  for (int i = mo->enumeratorOffset(); i < mo->enumeratorCount(); ++i) {
    QMetaEnum const e = mo->enumerator(i);
    QStringList keys;
    for (int k = 0; k < e.keyCount(); ++k) keys << QString{"%1=%2"}.arg(QString::fromUtf8(e.key(k))).arg(e.value(k));
    out << strip_namespace(QString{"enum %1 name=%2 enumName=%3 flag=%4 scoped=%5 keys=%6"}
                               .arg(i - mo->enumeratorOffset())
                               .arg(QString::fromUtf8(e.name()))
                               .arg(QString::fromUtf8(e.enumName()))
                               .arg(e.isFlag())
                               .arg(e.isScoped())
                               .arg(keys.join(',')));
  }
  return out;
}

inline QStringList dump_class_info(QMetaObject const* mo) {
  QStringList out;
  for (int i = mo->classInfoOffset(); i < mo->classInfoCount(); ++i)
    out << QString{"classinfo %1 %2=%3"}
               .arg(i - mo->classInfoOffset())
               .arg(QString::fromUtf8(mo->classInfo(i).name()))
               .arg(QString::fromUtf8(mo->classInfo(i).value()));
  return out;
}

// One class level: its name, its superclass and every own method, property, enum and class info.
inline QStringList dump_class(QMetaObject const* mo) {
  QStringList out;
  out << strip_namespace(QString{"class %1 super=%2"}
                             .arg(QString::fromUtf8(mo->className()))
                             .arg(QString::fromUtf8(mo->superClass() ? mo->superClass()->className() : "")));
  out << dump_class_info(mo) << dump_enums(mo) << dump_properties(mo) << dump_methods(mo);
  return out;
}
