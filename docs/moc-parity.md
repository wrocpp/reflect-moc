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
| A-03 | gadget | `Q_GADGET` | - | - | unsupported | - | `metaObjectData` is fed a QObject-shaped flag set and the dispatch assumes a QObject; an `rqt::Gadget<D>` base would carry it |
| A-04 | namespace meta-object | `Q_NAMESPACE`, `Q_ENUM_NS`, `Q_FLAG_NS` | - | - | unsupported | - | a namespace is never complete; `members_of(^^ns)` can enumerate it and an ADL hook can supply `qt_getEnumMetaObject` |
| A-05 | class name for a template | - | `RQT_CLASS_NAME(n)` | - | same | capability_class_name_override | beyond moc: moc cannot read a class template |
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
| B-09 | revision of a method | `Q_REVISION(1, 2)` | - | - | unsupported | - | no annotation, no flag in `function_data` |
| B-10 | scriptable method | `Q_SCRIPTABLE` | - | - | unsupported | - | same place |
| B-11 | private signal tag | `QPrivateSignal` | - | - | unsupported | - | `RQT_OBJECT` does not declare the tag type |
| B-12 | signal or slot outside its section | `Q_SIGNAL`, `Q_SLOT` | the annotation works anywhere | same | partial | - | the compat header does not map the keywords: without the annotation the member silently stops being a slot |
| B-13 | invokable constructor | `Q_INVOKABLE Foo(int)` | - | - | unsupported | - | `constructors{}` is hard-coded in `metadata.hpp`; annotated constructors can be reflected |
| B-14 | private slot | `Q_PRIVATE_SLOT(d, f())` | - | - | unsupported | - | the d-pointer class is usually incomplete in the header; hard |
| C-01 | property with accessors | `Q_PROPERTY(int v READ v WRITE setV NOTIFY vChanged RESET reset)` | `RQT_PROPERTY(...)` | - | same | capability_differential_against_moc | READ, WRITE, NOTIFY and RESET resolve to the first function of that name |
| C-02 | property on a member | `MEMBER m_` | `RQT_PROPERTY(... MEMBER m_ ...)` | - | same | capability_differential_against_moc | |
| C-03 | CONSTANT, FINAL, REQUIRED | same keywords | same keywords | - | same | capability_differential_against_moc | |
| C-04 | DESIGNABLE, SCRIPTABLE, STORED, USER as true or false | same keywords | same keywords | - | same | capability_differential_against_moc | |
| C-05 | the same keywords with a function | `DESIGNABLE isDesignable` | - | - | unsupported | - | stops the build with a message |
| C-06 | property revision | `REVISION(1, 2)` | parsed and **dropped without a message** | - | unsupported | limit_parity_revision | `annotation_from` has no field to copy it into |
| C-07 | bindable property | `BINDABLE bindableX` | stops the build | - | unsupported | - | resolve the named `QBindable` getter as WRITE is resolved and answer `BindableProperty` in `dispatch.hpp` |
| C-08 | bindable property members | `Q_OBJECT_BINDABLE_PROPERTY` | - | - | unsupported | - | Qt templates, not moc input; unusable until BINDABLE is |
| C-09 | private property | `Q_PRIVATE_PROPERTY(d, ...)` | - | - | unsupported | - | as B-14 |
| D-01 | enum | `Q_ENUM(E)` | `RQT_ENUM(E)` | - | partial | capability_differential_against_moc | the meta-object equals moc's; `QMetaEnum::fromType<E>()` needs `qt_getEnumMetaObject`, which is not declared |
| D-02 | flags | `Q_FLAG(Fs)` | `RQT_FLAG(Fs)` | - | partial | capability_differential_against_moc | as D-01 |
| D-03 | enum in a namespace | `Q_ENUM_NS`, `Q_FLAG_NS` | - | - | unsupported | - | as A-04 |
| D-04 | interfaces | `Q_DECLARE_INTERFACE`, `Q_INTERFACES(I)` | `Q_INTERFACES(I)` expands to nothing | same | same | capability_differential_against_moc | the interface ids of the direct bases are found by reflection |
| E-01 | plugin metadata | `Q_PLUGIN_METADATA(IID "x" FILE "x.json")` | - | - | unsupported | - | the JSON must be read at compile time (`#embed`) and turned into CBOR in constexpr code, with the entry points; a spike |
| F-01 | QML element | `QML_ELEMENT`, `QML_NAMED_ELEMENT`, `QML_SINGLETON`, `QML_UNCREATABLE`, `QML_ANONYMOUS`, `QML_ATTACHED`, `QML_FOREIGN`, `QML_EXTENDED` | `rqt::register_qml<T>()` at run time | - | partial | capability_register_qml | no annotations; registered by a call, not by the class |
| F-02 | metatypes JSON for qmltyperegistrar | `moc --output-json` | - | - | impossible | - | qmltyperegistrar reads the JSON, a build step; a program built from the same classes could print it |
| G-01 | `tr()` and its context | `Q_OBJECT` | `RQT_OBJECT` | - | same | capability_differential_against_moc | the context is the class name; lupdate scans source text and was not tried |
| G-02 | `qobject_cast` | `Q_OBJECT` | `RQT_OBJECT` | - | same | capability_qobject_cast_tier_b | the mixin form needs one more line |
| G-03 | class template | not allowed | `RQT_OBJECT` in a template | - | same | capability_class_template_signals | beyond moc |
| G-04 | multiple inheritance | `class D : public QObject, public I` | same | - | same | capability_differential_against_moc | |
| G-05 | virtual base | not allowed by moc either | - | - | unsupported | - | the owner anchor is computed with `offset_of`; moc rejects it too, so it is not a parity gap |
| G-06 | include for forward declarations | `Q_MOC_INCLUDE("x.h")` | not needed | - | not-needed | - | there is no generated file, so nothing to include |
