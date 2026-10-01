"""Every spelling rqt-migrate emits, in one place.

The spellings follow include/reflect_moc/qt/README.md. When the library's
syntax changes, change it here and regenerate the ported examples; no other
module spells reflect-moc C++ or CMake.
"""

from __future__ import annotations

# The header every migrated header includes.
HEADER_INCLUDE = "#include <reflect_moc/qt/qt.hpp>"

# Q_OBJECT also declared tr() with the class name as translation context. A
# class whose own code calls tr() gets this instead, so its context is kept.
TR_FUNCTIONS = "Q_DECLARE_TR_FUNCTIONS({cls})"
TR_INCLUDE = "#include <QtCore/qcoreapplication.h>"


def object_base(qt_base: str) -> str:
    """The base that replaces a Q_OBJECT class's first base (a mixin on the Qt base, not CRTP)."""
    return f"rqt::Object<{qt_base}>"


def object_base_using(qt_base: str) -> str:
    """Replaces `using QObject::QObject;` when the class has a constructor of its own."""
    return f"using {object_base(qt_base)}::Object;"


# A class is bound to its meta-object by a constructor body that calls bind()
# (README "Binding the class to the instance", E3). The most derived
# constructor body wins, so every migrated class needs one.
BIND = "bind();"


def forwarding_constructor(cls: str, qt_base: str) -> str:
    """Replaces `using Base::Base;` in a class with no constructor of its own.

    An inherited constructor has no body, so it cannot call bind(). The
    forwarding constructor takes every argument list the base accepts.
    """
    return (f"template <class... Args> explicit {cls}(Args &&...args) : {object_base(qt_base)}"
            f"(std::forward<Args>(args)...) {{ {BIND} }}")


def default_constructor(cls: str) -> str:
    """For a class with no constructor at all: the implicit one cannot call bind()."""
    return f"{cls}() {{ {BIND} }}"


# The optional tier B opt-in (README "Two tiers"): stock qobject_cast,
# pointer-to-member and functor QObject::connect, qmlRegisterType<T> and
# `T::staticMetaObject` need it. The first goes in the class body (public), the
# second follows the class; it is inline because a header's definition is
# compiled into every translation unit that includes it.
STATIC_META_OBJECT_DECLARATION = "static QMetaObject const &staticMetaObject;"
STATIC_META_OBJECT_DEFINITION = "inline RQT_STATIC_META_OBJECT({cls});"

SIGNAL = "[[=rqt::signal]]"
SLOT = "[[=rqt::slot]]"
INVOKABLE = "[[=rqt::invokable]]"


def signal_body(arg_names: list[str]) -> str:
    """The one-line body of a migrated signal declaration."""
    return "{ rqt::emit{this}(" + ", ".join(arg_names) + "); }"


def enum(is_flag: bool) -> str:
    """The annotation on the enum a Q_ENUM / Q_FLAG named.

    It goes BEFORE the name: `enum class [[=rqt::enum_]] Mode`. Q_FLAG(Options)
    names the QFlags alias, and the annotation goes on the enum behind it; the
    library has no field for the alias name.
    """
    return "[[=rqt::flag]]" if is_flag else "[[=rqt::enum_]]"


# The fields of rqt::property. Q_PROPERTY attributes outside this list (NAME
# differing from the accessor, FINAL, CONSTANT, REQUIRED, ...) have no field.
PROPERTY_FIELDS = ("read", "write", "notify", "reset")


def property(fields: list[tuple[str, str]]) -> str:
    """The annotation on a property's READ accessor or MEMBER data member.

    fields is an ordered list of (designator, text) pairs, e.g.
    [("write", "setValue"), ("notify", "valueChanged")]. The text is written as
    a string literal; the library stores it as rqt::name (at most 63 characters).
    """
    if not fields:
        return "[[=rqt::property{}]]"
    inner = ", ".join(f'.{name} = "{value}"' for name, value in fields)
    return f"[[=rqt::property{{{inner}}}]]"


def classinfo(name_literal: str, value_literal: str) -> str:
    """A class annotation from Q_CLASSINFO; both arguments are C++ literals as written."""
    return f"[[=rqt::classinfo{{{name_literal}, {value_literal}}}]]"


def interface_metacast(qt_base: str, interfaces: list[str]) -> list[str]:
    """Q_INTERFACES: the qt_metacast override moc generated, answering each interface's IID.

    Lines without indentation; the caller indents them.
    """
    lines = ["void *qt_metacast(const char *name) override", "{",
             f"    if (void *found = {object_base(qt_base)}::qt_metacast(name))", "        return found;"]
    for iface in interfaces:
        lines += [f"    if (!qstrcmp(name, qobject_interface_iid<{iface} *>()))",
                  f"        return static_cast<{iface} *>(this);"]
    return lines + ["    return nullptr;", "}"]


# Spellings QT_NO_KEYWORDS takes away.
FOREVER = "for (;;)"
FOREACH = "Q_FOREACH"

# QML registration that qmltyperegistrar generated from moc's JSON.
QML_HEADER = "rqt_qml_types.hpp"
QML_FUNCTION = "rqt_register_qml_types"


def qml_register(kind: str, cls: str, uri: str, major: int, minor: int, name: str, reason: str | None) -> str:
    if kind == "anonymous":
        return f'qmlRegisterAnonymousType<{cls}>("{uri}", {major});'
    if kind == "uncreatable":
        return f'qmlRegisterUncreatableType<{cls}>("{uri}", {major}, {minor}, "{name}", {reason});'
    return f'qmlRegisterType<{cls}>("{uri}", {major}, {minor}, "{name}");'


def qml_header(includes: list[str], registrations: list[str]) -> str:
    lines = [
        "// Generated by tools/rqt-migrate. With moc gone, qmltyperegistrar has no",
        "// metatypes to read, so the QML_ELEMENT family is registered here instead.",
        "#pragma once",
        "",
        "#include <QtQml/qqml.h>",
        "",
        *[f'#include "{inc}"' for inc in includes],
        "",
        f"inline void {QML_FUNCTION}()",
        "{",
        *[f"    {r}" for r in registrations],
        "}",
        "",
    ]
    return "\n".join(lines)


QML_CALL = f"{QML_FUNCTION}();"

# CMake: inserted after qt_standard_project_setup() (or find_package(Qt6)).
CMAKE_BLOCK = """\
# rqt-migrate: reflect-moc builds the meta-objects from C++26 reflection, so
# moc must not run.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(QT_NO_KEYWORDS)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-freflection>)
include_directories({include_path})
"""


def cmake_include_path(relative_or_absolute: str, is_absolute: bool) -> str:
    if is_absolute:
        return relative_or_absolute
    return "${CMAKE_CURRENT_SOURCE_DIR}/" + relative_or_absolute

# Added to qt_add_qml_module: there is no moc JSON to generate qmltypes from.
CMAKE_QML_NO_TYPES = "NO_GENERATE_QMLTYPES"
