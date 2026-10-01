// reflect-moc, Qt layer: a QMetaObject from C++26 reflection, no moc.
// See README.md in this directory.
#pragma once

#include "annotations.hpp"
#include "connect.hpp"
#include "object.hpp"

#if __has_include(<QtQml/qqml.h>)
#include "qml.hpp"
#endif
