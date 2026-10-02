# tools/rqt-migrate

Rewrites a moc-based Qt source tree to reflect-moc annotations. Python 3, stdlib only.

```sh
# in the Qt image (moc is needed unless --json-dir is given)
tools/rqt-migrate path/to/src --output path/to/ported --reflect-moc-include include \
  --diff migration.diff --report unmigrated.md
```

moc is the source of truth. The tool runs Qt's own `moc --output-json` on every file that contains `Q_OBJECT` or `Q_GADGET`, with Qt's include dirs so `Q_INTERFACES` and the QML macros resolve. From the JSON it takes the classes, bases, signals, slots, invokables, properties, enums and class info, and the line each one is on. It does not parse C++. It finds the exact span on moc's line with regexes over a copy of the file where comments and literal contents are blanked out, then edits that span. The source tree is never changed.

Every spelling it emits lives in `rqt_migrate/syntax.py`. The reflect-moc Qt syntax is provisional, so change it there and regenerate.

## Coverage

| construct | result |
|---|---|
| `Q_OBJECT` | removed. If the class's own code calls `tr()`, it becomes `Q_DECLARE_TR_FUNCTIONS(Class)`, which keeps the translation context |
| first base `QObject` / `QWidget` / ... | becomes `rqt::Object<Base>`. `using Base::Base;` and `: Base(...)` initializers are rewritten too, because both must name a direct base |
| `signals:` / `Q_SIGNALS:` | becomes `public:` in place, so declaration order is kept |
| signal declaration | `[[=rqt::signal]] void f(T a) { rqt::emit{this}(a); }`. Unnamed parameters get `argN` names, and multi-line declarations keep their line breaks |
| `public slots:`, `Q_SLOTS`, `Q_SLOT` | the keyword is dropped from the access specifier, and each slot gets `[[=rqt::slot]]` |
| `Q_INVOKABLE` | `[[=rqt::invokable]]` |
| `Q_PROPERTY` | an annotation on the READ accessor (or the MEMBER data member) carrying `.name` (when it differs), `.write`, `.notify`, `.reset`, `.constant`, `.final`, `.required` |
| `Q_ENUM` / `Q_FLAG` | an annotation on the enum (for a flag, the enum behind the `QFlags` alias, carrying the alias name). Reported **partial**: `QMetaEnum::fromType` needs the `qt_getEnumMetaObject` friend that `Q_ENUM` declared |
| `Q_CLASSINFO`, and the class info QML macros expand to | class annotations, in moc's order |
| `Q_GADGET` | `[[=rqt::gadget]]` |
| `Q_INTERFACES` | `[[=rqt::interface{^^I}]]`. Reported **partial**: `qt_metacast` must answer the IID |
| `QML_ELEMENT`, `QML_NAMED_ELEMENT`, `QML_ANONYMOUS`, `QML_UNCREATABLE` (+ `QML_ATTACHED`) | the macros stay, for the type aliases they declare. A generated `rqt_qml_types.hpp` registers the types under the `qt_add_qml_module` URI and version, and a call to it goes in before each `QQmlEngine` / `QQmlApplicationEngine` |
| `emit` / `Q_EMIT`, `forever`, `foreach` | dropped, or spelled the way `QT_NO_KEYWORDS` allows |
| `#include "moc_x.cpp"` / `"x.moc"` | deleted |
| CMake | AUTOMOC off, C++26, `QT_NO_KEYWORDS`, `-freflection`, the reflect-moc include dir, and `NO_GENERATE_QMLTYPES` on `qt_add_qml_module` |
| signal or slot with default arguments, overloaded signal, property attributes without a field (`DESIGNABLE`, `SCRIPTABLE`, `STORED`, `USER`, `BINDABLE`, `REVISION`), method `Q_REVISION` | migrated, reported **partial** |
| `Q_PRIVATE_SLOT`, `Q_PRIVATE_PROPERTY`, `Q_PLUGIN_METADATA`, `Q_REVISION`, `Q_SCRIPTABLE`, `Q_MOC_INCLUDE`, `Q_NAMESPACE` family, invokable constructors, a READ accessor not declared in the class, a template class (moc's own error), `QML_SINGLETON` / `QML_FOREIGN` / `QML_EXTENDED`, `qt_wrap_cpp`, unknown `Q_*` / `QML_*` macros | left in place, reported **manual** |

## Tests

```sh
cd tools && python3 -m unittest discover -s tests
```

The fixtures' moc JSON is saved under `tests/fixtures/<case>/moc`, so the tests need no Qt. After changing a fixture, regenerate the JSON with `tests/regen_fixtures.sh` in the image.

## tools/rqt-lint-emit.py

Flags `emit other->sig(v)` and `emit other.sig(v)` where `sig` is an `rqt::static_signal`. A static signal is one object for the whole class, so the line compiles and fires on `this`, not on `other`. The fix is `sig(v).from(other);`. Python 3, stdlib only; the names of the static signals are collected from every file scanned.

```sh
tools/rqt-lint-emit.py src include     # exit 1 if a line is flagged
cd tools && python3 -m unittest discover -s tests
```
