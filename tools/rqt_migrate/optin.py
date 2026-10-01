"""Which classes need the tier B opt-in (a `staticMetaObject` of their own).

Stock Qt reads `T::staticMetaObject` in a few places that reflect-moc's
mixin cannot answer by itself (README, "Two tiers"). Without the opt-in the
name still resolves, to QObject's meta-object, so a missing opt-in builds and
then misbehaves. These are found from the sources:

  * qobject_cast<T *>(...)
  * a pointer-to-member connect, `&T::signal`, or QSignalSpy / fromSignal on one
  * `T::staticMetaObject` written out
  * qmlRegister...<T> written by hand, and every class the QML_ELEMENT family
    registers (the generated registration calls qmlRegisterType<T>)
  * a class that appears in a property or in a method's parameter or return type
    (Qt takes its meta-object from `T::staticMetaObject` for T *)
"""

from __future__ import annotations

import re

_NAME = r"((?:\w+::)*\w+)"
_QOBJECT_CAST = re.compile(rf"\bqobject_cast\s*<\s*(?:const\s+)?{_NAME}\s*\*\s*>")
_STATIC_META = re.compile(rf"\b{_NAME}\s*::\s*staticMetaObject\b")
_QML_REGISTER = re.compile(rf"\bqml(?:Register\w*|AttachedPropertiesObject)\s*<\s*{_NAME}")
_MEMBER_POINTER = re.compile(rf"&\s*{_NAME}\s*::\s*(\w+)\b")
_QML_CLASSINFO = ("QML.Element", "QML.Attached", "QML.Extended", "QML.Foreign")


def _last(name: str) -> str:
    return name.split("::")[-1]


def _method_types(cls: dict):
    for key in ("signals", "slots", "methods"):
        for m in cls.get(key, []):
            yield m.get("returnType", "")
            for a in m.get("arguments", []):
                yield a.get("type", "")


def required(blanked_sources: list[str], classes: list[dict]) -> dict[str, str]:
    """class name -> why it needs the opt-in, for the classes among `classes` that do."""
    names = {c["className"] for c in classes if c.get("object")}
    signals = {s["name"] for c in classes for s in c.get("signals", [])}
    why: dict[str, str] = {}

    def need(name: str, reason: str) -> None:
        name = _last(name)
        if name in names:
            why.setdefault(name, reason)

    for text in blanked_sources:
        for m in _QOBJECT_CAST.finditer(text):
            need(m.group(1), "qobject_cast")
        for m in _STATIC_META.finditer(text):
            need(m.group(1), "staticMetaObject used")
        for m in _QML_REGISTER.finditer(text):
            need(m.group(1), "registered with QML")
        for m in _MEMBER_POINTER.finditer(text):
            if m.group(2) in signals:
                need(m.group(1), "pointer-to-member signal")
    for c in classes:
        if not c.get("object"):
            continue
        info = {ci["name"]: ci["value"] for ci in c.get("classInfos", [])}
        if "QML.Element" in info:
            need(c["className"], "QML type")
        for key in _QML_CLASSINFO[1:]:
            if key in info:
                need(info[key], "QML type")
        mentioned = [p.get("type", "") for p in c.get("properties", [])] + list(_method_types(c))
        for text in mentioned:
            for n in names:
                if re.search(rf"\b{re.escape(n)}\b", text):
                    need(n, f"used as a type in {c['className']}")
    return why
