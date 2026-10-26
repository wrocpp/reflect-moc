#pragma once
#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QVariant>
#include <cstdio>
#include "differential_dump.hpp"
// invokes the private slots and the private property through the meta-object, then dumps the meta-object
template <class W>
void probe(W& w) {
  QMetaObject const* mo = w.metaObject();
  std::printf("depth read: %d\n", w.property("depth").toInt());
  std::printf("depth write: %d\n", int(w.setProperty("depth", 9)));
  std::printf("depth read: %d\n", w.property("depth").toInt());
  std::printf("invoke onTick(5): %d\n", int(QMetaObject::invokeMethod(&w, "onTick", Q_ARG(int, 5))));
  std::printf("invoke reset(): %d\n", int(QMetaObject::invokeMethod(&w, "reset")));
  for (auto const& l : dump_class(mo)) std::printf("DUMP %s\n", qPrintable(l));
}
