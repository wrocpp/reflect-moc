"""Every spelling rqt-migrate emits, in one place.

The spellings follow include/reflect_moc/qt/README.md. When the library's
syntax changes, change it here and regenerate the ported examples; no other
module spells reflect-moc C++ or CMake.
"""

from __future__ import annotations

# Two target styles. `annotations` rewrites a class into the mixin shape the
# rest of this module spells. `qtlike` leaves the class as Qt wrote it: the
# Q_OBJECT family stays in the source and compat.hpp, included after the Qt
# headers, redefines those macros to the reflect-moc versions; only each signal
# and slot is rewritten.
ANNOTATIONS = "annotations"
QTLIKE = "qtlike"
STYLES = (ANNOTATIONS, QTLIKE)

# The header every migrated header includes.
HEADER_INCLUDE = "#include <reflect_moc/qt/qt.hpp>"
COMPAT_INCLUDE = "#include <reflect_moc/compat.hpp>"

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


def forwarding_constructor(cls: str, direct_base: str) -> str:
    """Replaces `using Base::Base;` in a class with no constructor of its own.

    An inherited constructor has no body, so it cannot call bind(). The
    forwarding constructor takes every argument list the base accepts;
    direct_base is how the class spells its base (rqt::Object<B>, or a
    migrated class).
    """
    return (f"template <class... Args> requires rqt::forwardable<{cls}, Args...> "
            f"explicit {cls}(Args &&...args) : {direct_base}"
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
STATIC_META_OBJECT_DEFINITION = "RQT_STATIC_META_OBJECT({cls});"

SIGNAL = "[[=rqt::signal_function]]"
SLOT = "[[=rqt::slot]]"
INVOKABLE = "[[=rqt::invokable]]"


SIGNALS_MEMBERS = "members"
SIGNALS_BODIES = "bodies"
SIGNAL_MODES = (SIGNALS_MEMBERS, SIGNALS_BODIES)


def signal_member(types: list[str], name: str) -> str:
    """qtlike signal as a bodyless data member: `rqt::signal<void(int, QString)> name;`."""
    return f"rqt::signal<void({', '.join(types)})> {name};"


def signal_names(names: list[str]) -> str:
    """The annotation that keeps parameter names for QML handlers; unnamed ones are empty strings."""
    return "[[=rqt::names(" + ", ".join(f'"{n}"' for n in names) + ")]]"


def signal_body(arg_names: list[str], style: str = ANNOTATIONS) -> str:
    """The one-line body of a migrated signal declaration (rqt::emit, or rqt::activate in qtlike)."""
    call = "rqt::activate" if style == QTLIKE else "rqt::emit"
    return "{ " + call + "{this}(" + ", ".join(arg_names) + "); }"


def enum(is_flag: bool) -> str:
    """The annotation on the enum a Q_ENUM / Q_FLAG named.

    It goes BEFORE the name: `enum class [[=rqt::enum_]] Mode`. Q_FLAG(Options)
    names the QFlags alias, and the annotation goes on the enum behind it; the
    library has no field for the alias name.
    """
    return "[[=rqt::flag]]" if is_flag else "[[=rqt::enum_]]"


# The fields of rqt::property in declaration order (a designated initializer
# must keep it), and the Q_PROPERTY attributes that are booleans with their default.
PROPERTY_NAME_FIELDS = ("write", "notify", "reset", "name")
PROPERTY_FLAG_FIELDS = {"final": False, "constant": False, "required": False, "user": False,
                        "designable": True, "scriptable": True, "stored": True}


def property(fields: dict[str, str | bool | int]) -> str:
    """The annotation on a property's READ accessor or MEMBER data member.

    fields maps a field name to a string (written as a literal; the library stores
    it as rqt::short_text, at most 63 characters) or to a bool. Only fields that
    differ from the default are passed; they are emitted in the library's order.
    """
    inner = []
    for key in PROPERTY_NAME_FIELDS + tuple(PROPERTY_FLAG_FIELDS) + ("index",):
        if key in fields:
            value = fields[key]
            if isinstance(value, bool):
                inner.append(f".{key} = {str(value).lower()}")
            elif isinstance(value, int):
                inner.append(f".{key} = {value}")
            else:
                inner.append(f'.{key} = "{value}"')
    return f"[[=rqt::property{{{', '.join(inner)}}}]]"


def classinfo(name_literal: str, value_literal: str) -> str:
    """A class annotation from Q_CLASSINFO; both arguments are C++ literals as written."""
    return f"[[=rqt::classinfo{{{name_literal}, {value_literal}}}]]"


def interface_metacast(direct_base: str, interfaces: list[str]) -> list[str]:
    """Q_INTERFACES: the qt_metacast override moc generated, answering each interface's IID.

    Lines without indentation; the caller indents them.
    """
    lines = ["void *qt_metacast(const char *name) override", "{",
             f"    if (void *found = {direct_base}::qt_metacast(name))", "        return found;"]
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


# qtlike keeps Qt's keywords (signals, slots, emit): no QT_NO_KEYWORDS.
CMAKE_BLOCK_QTLIKE = CMAKE_BLOCK.replace("add_compile_definitions(QT_NO_KEYWORDS)\n", "")


def cmake_block(style: str) -> str:
    return CMAKE_BLOCK_QTLIKE if style == QTLIKE else CMAKE_BLOCK


def cmake_include_path(relative_or_absolute: str, is_absolute: bool) -> str:
    if is_absolute:
        return relative_or_absolute
    return "${CMAKE_CURRENT_SOURCE_DIR}/" + relative_or_absolute

# Added to qt_add_qml_module: there is no moc JSON to generate qmltypes from.
CMAKE_QML_NO_TYPES = "NO_GENERATE_QMLTYPES"
