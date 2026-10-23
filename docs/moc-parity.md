# reflect-moc against moc: the conformance matrix

One row per construct that Qt's moc processes or generates. A verdict says how close reflect-moc is **today**, and
`scripts/check_moc_parity.py` (run by ctest as `capability_moc_parity_matrix_consistent`) fails on a blank cell, an unknown
verdict, a missing test file, a 'same' row without a test or a non-'same' row without a note.

How a row is proved: a test in `tests/qt/parity` (`parity_<construct>.cpp`) builds the construct twice, once for real moc in
a header named `parity_<construct>_moc.h` (AUTOMOC is on for that target only: the oracle runs moc on purpose) and once for
reflect-moc, and compares the two `QMetaObject`s (names, signatures, parameter names and types, access, flags, revisions,
clones, indexes, properties with every attribute, enums, class info) and the behaviour (`invokeMethod`, property round
trips, `connect`, `qobject_cast`). `limit_parity_<construct>.cpp` pins a known difference: it passes while the difference
exists and is flipped by the fix.

Verdicts: **same** (the meta-object and the behaviour equal moc's), **partial** (the note says what does not),
**unsupported** (does not build, or is ignored; the note says why and what would carry it), **impossible** (needs an
external file or build step, named in the note, or is not expressible in C++).

The macro-free column is the spelling with no macro at all; 'same' means the reflect-moc spelling already has no macro; '-' means there is none yet.

| id | construct | moc spelling | reflect-moc spelling | macro-free | verdict | test | note |
|---|---|---|---|---|---|---|---|
| A-01 | QObject class | `Q_OBJECT` | `RQT_OBJECT` | - | same | capability_differential_against_moc | must be the first line; a virtual base is not supported (G-05) |
| A-02 | class info | `Q_CLASSINFO("k", "v")` | `RQT_CLASSINFO("k", "v")` | - | same | capability_differential_against_moc | |
| A-03 | gadget | `Q_GADGET` | `struct P : rqt::Gadget<B>` | same | same | parity_gadget | the meta-object of a gadget and of a derived gadget equals moc's row for row; property code runs in the static meta-call (`PropertyAccessInStaticMetaCall`), so `readOnGadget`, `writeOnGadget`, `invokeOnGadget`, `QVariant::fromValue` and `QMetaType::metaObject` work with stock Qt. Not covered: signals (a gadget has none), enums in a gadget, QML registration of a value type |
| A-04 | namespace meta-object | `Q_NAMESPACE`, `Q_ENUM_NS`, `Q_FLAG_NS` | - | - | unsupported | - | a namespace is never complete; `members_of(^^ns)` can enumerate it and an ADL hook can supply `qt_getEnumMetaObject` |
| A-05 | class name for a template | - | `RQT_CLASS_NAME(n)` | - | same | capability_class_name_override | beyond moc: moc cannot read a class template |
| A-06 | QObject class, no macro | `Q_OBJECT` | `class D : public rqt::ObjectOf<B>` | same | same | parity_macro_free_class | `ObjectOf<B>` supplies the overrides, `staticMetaObject`, `tr` and the owner of data-member signals; B is `QObject` (`ObjectOf<>`), a Qt class or a reflected class. The class is not named: the default argument of the base is evaluated in the base clause, where `current_class()` is the class being defined, and it works for a class template, a nested class and a class in a namespace. The meta-object of a class and of a derived class equals moc's row for row. Enums, class info and the class name are `static constexpr` holders or annotations, see the macro audit below |
| B-01 | signal | `signals: void f(int v);` | `[[=rqt::names("v")]] rqt::signal<void(int)> f;` | same | same | capability_differential_against_moc | a data member carries the name and the owner anchor (8 bytes per signal) |
| B-02 | signal, static member | `signals:` | `static inline rqt::static_signal<void(int)> f;` | same | same | capability_differential_against_moc | beyond moc: no space in the object; `.from(this)` is required |
| B-03 | signal, function form | `signals:` | `[[=rqt::signal_function]] void f(int v) { rqt::activate{this}(v); }` | - | same | parity_overloads_function_form | |
| B-04 | overloaded signals, function form | `void f(int); void f(const QString&);` | two `[[=rqt::signal_function]]` functions | - | same | parity_overloads_function_form | the meta-object equals moc's row for row; connect with `QOverload` and by string both pick the right overload |
| B-15 | overloaded signals, one data member | - | `rqt::signal<void(int), void(const QString&)> f;` | - | unsupported | - | two data members cannot share a name; one member carrying a set of signatures is planned (W2) |
| B-05 | slot | `slots:` / `Q_SLOT` | `[[=rqt::slot]]` | same | same | capability_differential_against_moc | |
| B-06 | invokable method | `Q_INVOKABLE` | `[[=rqt::invokable]]` | same | same | capability_differential_against_moc | |
| B-07 | overloaded slots and invokables | two functions, one name | `[[=rqt::slot]]` / `[[=rqt::invokable]]` on each | same | same | parity_overloads_function_form | `invokeMethod` dispatches on the argument types; NOTIFY names the first overload in both builds. WRITE and RESET resolve to the first function of the name (see B-16) |
| B-16 | WRITE or NOTIFY with overloads | `WRITE setValue` picks by the property type through C++ overload resolution | first one-parameter function of the name | - | partial | - | an overloaded setter declared in the wrong order is bound to the wrong overload without an error; a test that shows it is planned (W2) |
| B-08 | default arguments (clones) | `void f(int, QString = {})` | same text | same | partial | capability_differential_against_moc | public functions only; a non-public function has no cloned rows |
| B-09 | revision of a method | `Q_REVISION(1, 2)` | `[[=rqt::revision{1, 2}]]` | same | same | parity_method_flags | on a slot, an invokable and a signal (function form and data member); the clones of a default argument keep the revision, as moc's do; `QMetaMethod::revision()` equals moc's |
| B-10 | scriptable method | `Q_SCRIPTABLE` | `[[=rqt::scriptable]]` | same | same | parity_method_flags | the `Scriptable` attribute on a slot, an invokable and a signal, clones included; an annotation on anything else stops the build (H-02) |
| B-11 | private signal tag | `QPrivateSignal` | `QPrivateSignal` as the last parameter | same | same | parity_private_signal | both signal forms; the tag is left out of the signature as in moc, `invokeMethod` and the property's notify call supply it. `RQT_OBJECT` declares the private struct, `rqt::Object<B>` a protected one (a class with neither declares `struct QPrivateSignal {};` itself: the library looks for a last parameter of that name) |
| B-12 | signal or slot outside its section | `Q_SIGNAL`, `Q_SLOT` | the annotation works anywhere | same | partial | - | the compat header does not map the keywords: without the annotation the member silently stops being a slot |
| B-13 | invokable constructor | `Q_INVOKABLE Foo(int)` | - | - | unsupported | - | `constructors{}` is hard-coded in `metadata.hpp`; annotated constructors can be reflected |
| B-14 | private slot | `Q_PRIVATE_SLOT(d, f())` | - | - | unsupported | - | the d-pointer class is usually incomplete in the header; hard |
| C-01 | property with accessors | `Q_PROPERTY(int v READ v WRITE setV NOTIFY vChanged RESET reset)` | `RQT_PROPERTY(...)` | - | same | capability_differential_against_moc | READ, WRITE, NOTIFY and RESET resolve to the first function of that name |
| C-02 | property on a member | `MEMBER m_` | `RQT_PROPERTY(... MEMBER m_ ...)` | - | same | capability_differential_against_moc | |
| C-03 | CONSTANT, FINAL, REQUIRED | same keywords | same keywords | - | same | capability_differential_against_moc | |
| C-04 | DESIGNABLE, SCRIPTABLE, STORED, USER as true or false | same keywords | same keywords | - | same | capability_differential_against_moc | |
| C-05 | the same keywords with a function | `DESIGNABLE isDesignable` | - | - | unsupported | limit_property_designable_function | stops the build with a message, never read as true (also SCRIPTABLE, STORED and USER: `limit_property_scriptable_function`, `limit_property_stored_function`, `limit_property_user_function`); moc evaluates the function at run time, which a flag in a constant table cannot |
| C-06 | property revision | `REVISION(1, 2)` | `REVISION(1, 2)` in `RQT_PROPERTY`, or `.revision = {1, 2}` on `rqt::property` | - | same | parity_revision | moc takes only the pair; the parser also takes the encoded number, `REVISION 515` and `REVISION(515)` (checked in the same test) |
| C-07 | bindable property | `BINDABLE bindableX` | stops the build | - | unsupported | limit_property_bindable_unsupported | resolve the named `QBindable` getter as WRITE is resolved and answer `BindableProperty` in `dispatch.hpp` |
| C-08 | bindable property members | `Q_OBJECT_BINDABLE_PROPERTY` | - | - | unsupported | - | Qt templates, not moc input; unusable until BINDABLE is |
| C-09 | private property | `Q_PRIVATE_PROPERTY(d, ...)` | - | - | unsupported | - | as B-14 |
| D-01 | enum | `Q_ENUM(E)` | `RQT_ENUM(E)` or `[[=rqt::enum_]]` on the enum | - | same | parity_enum_from_type | the meta-object equals moc's, `QMetaEnum::fromType<E>()` works and equals, and so does the `enumerator()` of an enum-typed property. `RQT_OBJECT` declares a friend template `qt_getEnumMetaObject` / `qt_getEnumName` that answers for every enum the class registers; annotated enums come first in the table, then `RQT_ENUM` ones (moc keeps declaration order). A class with no `RQT_OBJECT` needs `RQT_ENUM` (D-05) |
| D-02 | flags | `Q_FLAG(Fs)` | `RQT_FLAG(Fs)` or `[[=rqt::flag]]` | - | same | parity_enum_from_type | as D-01, `QMetaEnum::fromType<Fs>()` included |
| D-03 | enum in a namespace | `Q_ENUM_NS`, `Q_FLAG_NS` | - | - | unsupported | - | as A-04 |
| D-04 | interfaces | `Q_DECLARE_INTERFACE`, `Q_INTERFACES(I)` | `Q_INTERFACES(I)` expands to nothing | same | same | capability_differential_against_moc | the interface ids of the direct bases are found by reflection |
| D-05 | enum of a class with no `RQT_OBJECT` (the mixin on `rqt::Object<B>`) | `Q_ENUM(E)` | `RQT_ENUM(E)` declares the two friends | - | partial | - | an enum registered only by `[[=rqt::enum_]]` has no friend a base class could supply (argument-dependent lookup finds friends of the enum's own class); `RQT_ENUM` declares them, and `staticMetaObject` must be the class's own (the tier B line) |
| E-01 | plugin metadata | `Q_PLUGIN_METADATA(IID "x" FILE "x.json")` | - | - | unsupported | - | the JSON must be read at compile time (`#embed`) and turned into CBOR in constexpr code, with the entry points; a spike |
| F-01 | QML element | `QML_ELEMENT`, `QML_NAMED_ELEMENT`, `QML_SINGLETON`, `QML_UNCREATABLE`, `QML_ANONYMOUS`, `QML_ATTACHED`, `QML_FOREIGN`, `QML_EXTENDED` | `rqt::register_qml<T>()` at run time | - | partial | capability_register_qml | no annotations; registered by a call, not by the class |
| F-02 | metatypes JSON for qmltyperegistrar | `moc --output-json` | - | - | impossible | - | qmltyperegistrar reads the JSON, a build step; a program built from the same classes could print it |
| G-01 | `tr()` and its context | `Q_OBJECT` | `RQT_OBJECT` | - | same | capability_differential_against_moc | the context is the class name; lupdate scans source text and was not tried |
| G-02 | `qobject_cast` | `Q_OBJECT` | `RQT_OBJECT` | - | same | capability_qobject_cast_tier_b | the mixin form needs one more line |
| G-03 | class template | not allowed | `RQT_OBJECT` in a template | - | same | capability_class_template_signals | beyond moc |
| G-04 | multiple inheritance | `class D : public QObject, public I` | same | - | same | capability_differential_against_moc | |
| G-05 | virtual base | not allowed by moc either | - | - | unsupported | - | the owner anchor is computed with `offset_of`; moc rejects it too, so it is not a parity gap |
| G-06 | include for forward declarations | `Q_MOC_INCLUDE("x.h")` | not needed | - | not-needed | - | there is no generated file, so nothing to include |
| H-01 | compile-time errors of a property declaration | no READ or MEMBER; NOTIFY, READ, WRITE or RESET naming nothing | the same errors, with the name and the class in the message | - | same | limit_property_read_missing | both stop the build; no twin can exist (moc's twin would not compile either): `limit_property_read_missing`, `limit_property_read_typo`, `limit_property_notify_missing`, `limit_property_reset_typo`, `limit_rqt_property_write_missing`, `limit_property_write_typo`, `limit_notify_names_a_non_signal` |
| H-02 | no silent drop | - | what the parser accepts and the table builder would ignore stops the build | - | same | limit_method_two_kinds | beyond moc: a function annotated as two kinds, and `rqt::revision` or `rqt::scriptable` on something that is no signal, slot or invokable (`limit_revision_on_plain_member`, `limit_scriptable_on_plain_member`); REVISION is no longer dropped (C-06) |

## Macro audit

Every macro the library offers, with its spelling that needs none. `ObjectOf<B>` is the base of a class with no macro;
`tests/qt/capability_macro_free_object.cpp` and `capability_macro_free_qwidget.cpp` use no `RQT_` or `Q_` macro at all.

| macro | what it does | without the macro |
|---|---|---|
| `RQT_OBJECT`, `RQT_OBJECT_STATIC` | the overrides, `staticMetaObject`, `tr`, and the owner anchor of data-member signals | `rqt::ObjectOf<B>` as the base (`ObjectOf<>`, `ObjectOf<QWidget>`, `ObjectOf<Counter>`; the class is deduced); it adds no data member, so a class is as large as with `RQT_OBJECT_STATIC` |
| `RQT_PROPERTY(...)` | a property with the exact `Q_PROPERTY` text | `[[=rqt::property{...}]]` on the getter or the data member (already no macro) |
| `RQT_ENUM(E)`, `RQT_FLAG(F)` | a `static constexpr` holder that names the enum | `static constexpr rqt::enum_decl x{^^E, false};` (the macro is only shorter) |
| `RQT_CLASSINFO(k, v)` | a `static constexpr` holder | `static constexpr rqt::classinfo x{"k", "v"};` |
| `RQT_CLASS_NAME(n)` | the name Qt reports | `[[=rqt::class_name{"n"}]]` on the class |
| `Q_GADGET` | the meta-object of a value class | `rqt::Gadget<B>` as the base (`Gadget<>`, or `Gadget<Point>` for a derived gadget; B is `void` or another gadget) |
| `RQT_SELF` | the class, inside its own member | not used by `ObjectOf`; the class is the deduced second template argument |
| `emit`, `signals:`, `slots:` | Qt's keywords, empty macros | `sig(args)` for a data member, `rqt::activate{this}(args)` in a function signal, `[[=rqt::slot]]` |
