# reflect-moc feasibility spikes

Target: GCC 16.2.0 (`g++-16 -std=c++26 -freflection -Wall -Wextra`, header `<meta>`).
Comparison: clang-p2996 on Compiler Explorer (`clang_bb_p2996`,
`-std=c++26 -freflection-latest -stdlib=libc++`, header `<experimental/meta>`),
through `spikes/ce-clang.py`. Dated 2026-09-30.

Every spike file builds and exits 0 on GCC 16.2 with zero warnings. The
variants that do not work sit behind `-DSPIKE_*` probe macros, and
`spikes/run.sh` compiles each one and prints its first error.

## Verdicts

| # | Spike | GCC 16.2 | clang-p2996 |
|---|-------|----------|-------------|
| 01 | define_aggregate, signals_of\<T\> | works, from 2 trigger sites | works (same rules) |
| 02 | annotations on every entity kind | works | works (after a shim, see "Shared differences") |
| 03 | member functions, parameters, void** call | works; parameter names are fragile | works, same name behaviour |
| 04 | fixed_string -> member, CRTP set/get | works | works (after the shim) |
| 05 | signal forms (a) current_function, (b) descriptor | both work | (a) blocked, (b) works |
| 06 | real Qt 6.10 without moc | works: moc-free meta-object from reflection, QML included (4 workarounds) | not tried |
| 07 | non-template base + deducing this, no CRTP | works; qobject_cast / PMF connect / qmlRegisterType need a 2-line opt-in | not tried |

## Shared differences

- **std::meta::exception** exists on GCC 16.2, and `throw meta::exception(msg, r)`
  from consteval code becomes a compiler error with `msg` in it:
  `error: uncaught exception of type 'std::meta::exception'; 'what()': 'no member named 'widht' in Widget'`.
  clang-p2996 has no `std::meta::exception`
  (`no member named 'exception' in namespace 'std::meta'`). The spikes use a
  `SPIKE_THROW` macro. On clang it calls a non-constexpr function, which also
  stops constant evaluation, but the diagnostic does not carry the message.
- **std::meta::current_function()** is GCC only
  (`no member named 'current_function' in namespace 'std::meta'` on clang).
- **Lambdas inside `template for`** (verified by team-lead on g162 and CE): a
  captureless `[]` lambda that splices `obj.[:m:]` builds and runs on GCC 16.2
  and clang. An explicit `[m]` capture fails on GCC with
  `error: splice argument must be an expression of type 'std::meta::info' [-Wtemplate-body]`.
  GCC 16.1 also rejects captureless lambdas stored in a `std::function`, because
  it escalates them to consteval:
  `call to consteval function 'std::__invoke_r<...>' is not a constant expression`.
  The pointer-to-member NTTP helper (spike 03) does build on 16.1, so it is the
  fallback if 16.1 ever matters. We target 16.2 and prefer captureless lambdas.
  The spikes avoid `template for` entirely: every expansion is an
  `index_sequence` fold whose splice operand is `f(^^T)[I]`. That shape builds
  on both compilers and on both GCC versions.

## 01 define_aggregate

Works:
- `consteval { define_aggregate(^^S, {data_member_spec(^^int, {.name = "x"})}); }`
  at namespace scope.
- `signals_of<T>`: one member `signal_ref<FieldType> <field>Changed` per
  `[[=property{.notify=true}]]` field, from a namespace-scope consteval block
  written after T. The member names are ordinary members (`sig.widthChanged(7)`).
  The name is built at compile time as a `std::string` and passed to
  `data_member_spec(..., {.name = name})`.
- An alias template works: `template<class T> using signals_of = signals_of_t<T>;`.
- **Lazy definition works through a nested holder.** A class template
  `signals_holder<T>` declares `struct type;` and completes it from a consteval
  block in its own body. `template<class T> using lazy_signals_of = typename signals_holder<T>::type;`
  then defines the aggregate on first use, with no per-class line. The block
  and the target share a scope, so no scope intervenes (see below).
- **From a CRTP base, but only inside a member function body.**
  `auto& signals() { static lazy_signals_of<D> s{}; return s; }` works, because
  the body is instantiated on first call, when D is complete.

Blocked (the probe macro and the exact error):
- `SPIKE_TEMPLATE_BLOCK_NAMESPACE_TARGET`: a consteval block in a class template
  that completes a namespace-scope class.
  GCC: `'define_signals_for<Model7>' intervenes between 'consteval' block 'define_aggregate' is evaluated from and 'signals_of_t<Model7>' scope`.
  clang: `cannot produce injected declaration of 'signals_of_t<Model7>' from declaration of '(consteval-block ...)': scope associated with the class 'define_signals_for<Model7>' does not enclose both declarations`.
- `SPIKE_FN_BLOCK`: a block-scope consteval block in a function template.
  `'void define_in_function() [with T = Model8]' intervenes between 'consteval' block ...`.
- `SPIKE_RETURN_TYPE`: naming `lazy_signals_of<D>&` in the declaration of a CRTP
  base member instantiates the holder with the base, while D is incomplete:
  `uncaught exception of type 'std::meta::exception'; 'what()': 'not a complete class type'`.
  Workaround: a deduced return type (`auto&`).
- `SPIKE_CRTP_BASE`: a consteval block in a CRTP base. D is incomplete:
  `'what()': 'not a complete class type'`.
- `SPIKE_IN_CLASS`: a consteval block inside T after its members. T is still
  incomplete: `'what()': 'not a complete class type'`.
- `SPIKE_LAZY_ALIAS`: an alias template `[:ensure_signals(^^T):]` that defines
  on demand: `'define_aggregate' not evaluated from 'consteval' block`.
- `SPIKE_CONSTEVAL_FN`: `define_aggregate` from a `constexpr` variable
  initializer. Same error. On GCC 16.2 a consteval block is the only
  "plainly constant-evaluated" context that works.

Design consequence: generated types come either from a namespace-scope
consteval block after the class (one line per class) or from a nested type of
a class template, reached through an alias and used where T is complete.

## 02 Annotations

Works, with the portable lookup (`annotations_of(r)`,
`remove_cvref(type_of(a)) == ^^Tag`, `extract<Tag>(a)`), on:
- classes (`struct [[=doc{...}]] [[=classinfo{...}]] Button`), several per entity;
- member functions (`[[=slot]] void reset()`), including two on one function;
- enumerators (`Fast [[=doc{...}]]`, after the identifier);
- static constexpr data members;
- aggregate values: `property{.notify = true, .read_only = false}`, with both
  fields read back;
- `char const*` from `std::define_static_string`, read back at compile time and
  at run time (`constexpr char const* text = annotation_of<doc>(^^Button).text`).

Difference: `SPIKE_STRING_LITERAL` puts a raw string literal in an annotation
(`[[=doc{"literal"}]]`). **GCC 16.2 accepts it, and clang rejects it**:
`C++26 annotation attribute requires an expression usable as a template argument`.
clang follows the rule, because a pointer to a string literal is not a valid
template argument. Always use `define_static_string`.

## 03 Member functions

Works on both compilers:
- `members_of(^^T, access_context::unchecked())` with `is_function`,
  `is_special_member_function`, `is_static_member`, `is_const`, `is_private`,
  `is_constructor`, `is_destructor`, `is_operator_function`, plus
  `return_type_of`. Overloads come back as separate members.
- The four special members of a class that declares a default ctor, copy ctor
  and dtor include the implicitly declared copy assignment.
- `parameters_of(fn)` with `type_of` and `has_default_argument`. The latter is
  what moc needs to emit cloned signatures for defaulted parameters.
- A qt_metacall-style call through `void** args` (args[0] is the return slot,
  args[1..] point to the params), with parameter types spliced from
  `type_of(parameters_of(Fn)[I])`.
- `&[:fn:]` as a pointer-to-member NTTP (`pmf_holder<&[:fn("value"):]>`), with
  type `int (Counter::*)() const`.

Parameter names come back, but **only while every reachable declaration agrees
on the name**:

| declaration / definition | before the definition | after it |
|---|---|---|
| `setValue(int v)` / `(int newValue)` | `has_identifier` true, "v" | **false** |
| `rename(int)` / `(int to)` | false | true, "to" |

The outcome also depends on where the query is evaluated. For QML, which binds
signal-handler arguments by name, reflect-moc must either require consistent
names or diagnose `!has_identifier(p)` on signal parameters with a
`meta::exception`. Signals that are defined in the class body (form 05a) only
ever have one declaration.

## 04 Compile-time string -> member

Works on both: `template<class T, fixed_string N> consteval info member_named()`
finds a data member or member function by `identifier_of`.
`properties<D>::set<"width">(v)` assigns a data member or calls a setter
function, and `get<"height">()` works too, from a CRTP base member function
template. `w.template set<"width">(9)` works from a dependent context.
A misspelled name (`SPIKE_NO_MEMBER`) on GCC gives:
`error: uncaught exception of type 'std::meta::exception'; 'what()': 'no member named 'widht' in Widget'`.

## 05 Signal forms

Both forms work on GCC 16.2, each with a compile-time table of
`{char const* name, size_t arity}` records in declaration order
(`define_static_array`).

(a) `[[=rqt_a::signal]] void clicked(int x) { rqt_a::emit{this}(x); }`
- `emit` is a class template whose constructor is `emit(C* self, signal_id id = {})`.
  `signal_id` has a consteval constructor
  `signal_id(info caller = std::meta::current_function())` that turns the
  caller into a signal index and a name at compile time, and keeps only
  runtime-safe data. So `this` travels at run time and the caller reflection
  stays at compile time. CTAD deduces C from `this`.
- `current_function()` in that nested default argument resolves to the signal
  (`clicked`), not to `emit`'s constructor: checked at run time.
- `SPIKE_EMIT_OUTSIDE` (emit from a non-signal function):
  `error: uncaught exception of type 'std::meta::exception'; 'what()': 'rqt::emit used outside a [[=rqt::signal]] member function'`.
- Blocked on clang-p2996 (no `current_function`). The spike compiles form (a)
  out there.

(b) `static constexpr rqt_b::signal<int> valueChanged{};`
- Found with `is_variable` and `template_of(remove_cvref(type_of(m))) == ^^rqt_b::signal`.
  `remove_cvref` is needed because the static member's type is `const`.
- Identity: `object_of(member) == reflect_object(Sig)` at compile time
  (`rqt_b::emit<released>(this)`), or the address at run time
  (`valueChanged.emit(this, v)` searches a fold of `&[:signals_in(^^C)[I]:]`).
- Works on both compilers.

Naming: `rqt::signal` cannot be both the annotation object of form (a) and the
class template of form (b), so a design has to pick one name for one form.

## 06 Real Qt 6.10

Image: `reflect-moc/gcc16-qt610`, built from `docker/Dockerfile`: `gcc:16.2`
(native arm64) plus Qt **6.10.3** `linux_gcc_arm64` installed with aqtinstall.
It includes qtdeclarative and the offscreen QPA. No `--platform linux/amd64`
is needed on Apple Silicon.

    docker build -t reflect-moc/gcc16-qt610 docker
    docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 sh spikes/06-qt/build.sh

The shape of the moc 6.10 output (moc revision 69, from `moc_probe.h`):
`qt_create_metaobjectdata<Tag>()` returns
`QtMocHelpers::metaObjectData<Class, Tag>(flags, StringRefStorage{...}, UintData{SignalData<void(int)>(...), SlotData<...>, MethodData<...>}, UintData{PropertyData<int>(...)}, UintData{} /*enums*/)`.
`staticMetaObject` points at `qt_staticMetaObjectStaticContent<Tag>` and
`qt_staticMetaObjectRelocatingContent<Tag>`, which the `Q_OBJECT` macro
declares. `qt_static_metacall` switches on InvokeMetaMethod, IndexOfMethod,
ReadProperty and WriteProperty. A signal body is
`QMetaObject::activate(this, &staticMetaObject, local_index, args)`.

**06a handwritten.cpp: works.** It uses the `Q_OBJECT` macro and writes moc's
definitions by hand. String-based `SIGNAL/SLOT` connect, `invokeMethod` with
and without `Q_RETURN_ARG`, QMetaProperty read/write/notify, PMF connect and
`qobject_cast` all pass.

**06b reflected.cpp: works**, with no `Q_OBJECT` and no moc.
`struct Counter : rqt::QObjectBase<Counter>` (a CRTP base `template<class D, class B = QObject>`)
provides `staticMetaObject`, `metaObject()`, `qt_metacall`, `qt_metacast` and
`qt_static_metacall`, all generated from reflecting D. The same checks as 06a
pass, plus QML (QtQml `QQmlEngine`, context property): a binding to
`counter.value`, `Connections { function onValueChanged(value) }` that gets the
argument by NAME, a write from QML (`counter.value = 99`) that lands in the C++
member, and a call to a `[[=rqt::invokable]]` from QML. The method table, read
back from QMetaMethod:

    method 0: valueChanged(int) type=1(Signal) params=value
    method 1: reset()           type=2(Slot)
    method 2: setValue(int)     type=2(Slot)   params=v
    method 3: add(int)          type=0(Method) params=n

Workarounds that 06b needed:
1. **QT_NO_KEYWORDS is mandatory.** Qt's `emit` macro expands to nothing, so
   `struct emit` became `struct {`:
   `error: template class without a name` and
   `error: expected unqualified-id before '{' token`. The same applies to
   `signals` and `slots`.
2. **StringRefStorage cannot take reflection strings.** It needs
   `char const (&)[N]` with a known extent. `metaObjectData` only uses
   `Strings::StringCount`, `Strings::StringSize` and `writeTo(offsets, data)`,
   so a duck-typed `string_table<D>` built from `define_static_string`
   pointers replaces it.
3. **ParametersArray is per Data type.** Each `FunctionData` specialization
   nests its own `FunctionParameter`, so building `MethodData<F>::ParametersArray`
   and passing it to `SignalData<F>` fails:
   `no matching function for call to 'QtMocHelpers::SignalData<void(int)>::SignalData(const uint&, const uint&, uint, uint, const Params&)'`.
   Build the array from `typename Data::ParametersArray`.
4. **HasQ_OBJECT_Macro needs a specialization.** It checks that
   `&T::qt_metacall` is a member of T itself, and an override in a base does
   not count. Without the specialization (`-DSPIKE_NO_QOBJECT_SPECIALIZATION`):
   `qobject.h:250: static assertion failed: No Q_OBJECT in the class with the signal` (PMF connect) and
   `qobjectdefs.h:748: static assertion failed: qobject_cast requires the type to have a Q_OBJECT macro`.
   Fix: a constrained partial specialization of `QtPrivate::HasQ_OBJECT_Macro<D>`
   for every D derived from `rqt::object_tag`. This specializes a private Qt
   trait.
5. `type_id<T>()` covers only built-in metatypes
   (`QMetaTypeId2<T>::IsBuiltIn ? MetaType`). Anything else needs moc's
   `IsUnresolvedType | name-index` and a type-name string. That is not done
   yet (see the library requirements).

Which metaObjectData inputs come from reflection:

| moc input | reflection source |
|---|---|
| class name (string 0), `qt_metacast` name | `identifier_of(^^D)` |
| superdata | the base parameter B (`SuperData::link<B::staticMetaObject>()`) |
| method list and order | `members_of` + `[[=signal]]`/`[[=slot]]`/`[[=invokable]]`, signals first |
| method signature type `F` for SignalData\<F\> | `type_of(fn)` (function type, `const` included) |
| return / parameter metatypes | `return_type_of`, `type_of(parameters_of(fn)[i])` |
| parameter names | `identifier_of(param)` (see spike 03 for the consistency caveat) |
| access flags | `is_private` / `is_protected` |
| properties: name, type, flags, NOTIFY index | `[[=property]]` data members, `<name>Changed` signal |
| signal count (data[13]) | Qt computes it from the leading signal entries |
| static_metacall dispatch | `index_sequence` folds splicing `methods_of(^^D)[I]` |
| enums, classinfo, revisions, constructors | not tried; EnumData and ClassInfos accept the same reflected data |

Emit uses form 05(a): `rqt::emit{this}(args...)` inside a `[[=rqt::signal]]`
body, which calls `QMetaObject::activate` with the reflected local index.

Not tried here: queued connections across QThread (spike 07 covers them), and
non-builtin metatypes. The CRTP base was rejected after this spike, and
spike 07 explores the alternative.

## 07 Non-template base with deducing this (no CRTP)

Files: `spikes/07-deducing-this/binding.cpp` (the non-Qt research by
team-lead, E1-E4, re-run in the image) and `qt_binding.cpp` (the same against
Qt 6.10.3). Build: `sh spikes/07-deducing-this/build.sh` in the image.

Shape: `class rqt::Object : public ::QObject` is ONE class with no template
parameter. It overrides `metaObject()`, `qt_metacall` and `qt_metacast` once,
and they dispatch through a per-instance `class_info const*` that points at
per-class data generated from reflection:
`rqt::static_meta_object<T>` (an `inline constexpr QMetaObject` variable template)
and `rqt::info_for<T>` (meta-object, parent info, method and property counts).
The superclass and parent come from `bases_of(^^T)`, and `rqt::Object` itself
is transparent (its direct subclasses get `QObject` as Qt superclass).
`qt_metacall` walks the class chain root-first, which is moc's offset
arithmetic, so multi-level inheritance (`Sub : Worker : rqt::Object`) works.
In a signal body, `this` has the declaring class's type, so
`rqt::emit{this}` picks the right meta-object and local index at every level.

Binding the per-instance pointer, all three verified against real Qt:

| binding | what the class writes | how |
|---|---|---|
| E2 | `Receiver() : rqt::Object(this) {}` | base ctor template deduces Self, which is complete in a mem-initializer |
| E3 | `Sub() { bind(); }` | deducing-this member; the most-derived ctor body runs last and wins |
| E4 | nothing | `rqt::register_namespace<^^app>()` scans `members_of(namespace)` once into a `typeid -> class_info` map; `metaObject()` looks `typeid(*this)` up lazily |

E4 does not see `QQmlPrivate::QQmlElement<T>`, the subclass that
`qmlRegisterType<T>` instantiates (`typeid` gives
`N11QQmlPrivate11QQmlElementI6GadgetEE`). Classes created by QML therefore need
E2 or E3, and the spike's QML type uses E3.

E1 (team-lead): deducing this sees the static type at the call site, so it
serves the user-facing API, not Qt's virtual calls through `QObject*`. Verified
in Qt: `w.set<"progress">(7)` writes the property and emits its NOTIFY, and
`w.get<"progress">()` reads it.

What works with stock Qt 6.10.3 and NO per-class declaration (Worker, E4):

| check | result |
|---|---|
| `QObject::connect(SIGNAL(...), SLOT(...))`, incl. an inherited slot | works |
| `QMetaObject::invokeMethod`, own + inherited, `Q_RETURN_ARG` from a const method | works |
| `QMetaProperty` read/write/notify through the chain | works |
| `inherits("Worker")`, `className()`, `superClass()` | works |
| queued connection across a `QThread`, string connect | works (slot ran on the worker thread) |
| `rqt::connect(&w, &Worker::sig, &r, &Receiver::slot, Qt::QueuedConnection)` (PMFs -> indexes -> `QMetaObject::connect`) | works, queued included |
| QML context property: binding, `function onProgressChanged(progress)`, invokable call | works |
| `rqt::cast<T>(obj)` (= `static_meta_object<T>.cast(obj)`) | works |
| `qobject_cast<Worker*>` | **impossible**: `qobjectdefs.h:748: static assertion failed: qobject_cast requires the type to have a Q_OBJECT macro` |
| `QObject::connect(&w, &Worker::sig, ...)` (PMF / functor) | **impossible**: `qobject.h:250: static assertion failed: No Q_OBJECT in the class with the signal` |
| `qmlRegisterType<T>` | needs `T::staticMetaObject` |
| functor slot via an index | no public API (`QObject::connectImpl` is private, `QObjectPrivate::connect` is private API) |

Why the HasQ_OBJECT_Macro specialization must NOT be blanket here:
`QtPrivate::HasQ_OBJECT_Macro<T>` is
`sizeof(test(&Object::qt_metacall)) == sizeof(int)`, where the
`int (Object::*)(...)` overload only matches when `qt_metacall` is a member of
T itself. Forcing it true for a class without its own `staticMetaObject`
would make `qobject_cast<Worker*>` compile against the INHERITED
`QObject::staticMetaObject` and succeed for ANY QObject. PMF connect would
search QObject's methods for the signal. So the spike specializes it only for
classes that declare `staticMetaObject` themselves (checked by reflection), and
lets Qt's static_assert reject the others.

The opt-in for Qt's own templates (Gadget): two lines,

    struct Gadget : rqt::Object {
      static QMetaObject const& staticMetaObject;   // in the class
      Gadget() { bind(); }
      ...
    };
    QMetaObject const& Gadget::staticMetaObject = rqt::static_meta_object<Gadget>;  // after it

With it, `qobject_cast<Gadget*>`, PMF/functor `QObject::connect(&g, &Gadget::valueChanged, ...)`
and `qmlRegisterType<Gadget>("demo", 1, 0, "Gadget")` all work. The QML test
instantiates `Gadget { value: 3; onValueChanged: function(value) {...} }`, then
calls its slot from QML.

Blocked / traps:
- `SPIKE_INLINE_SMO`: the one-line in-class form
  `static constexpr QMetaObject const& staticMetaObject = rqt::static_meta_object<Gadget>;`
  instantiates the meta-object while Gadget is incomplete:
  `uncaught exception of type 'std::meta::exception'; 'what()': 'neither complete class type nor namespace'`.
  So the reference has to be defined out of class (2 lines).
- `SPIKE_INDEX_CONNECT_MISMATCH`: index-based `QMetaObject::connect` does NOT
  check argument compatibility. It returned a valid connection for
  `finished()` -> `setProgress(int)`, and the slot would read garbage.
  `rqt::connect` checks at compile time that the slot's parameters are a
  prefix of the signal's:
  `static assertion failed: rqt::connect: the slot's parameters must be a prefix of the signal's`.
- **GCC 16.2 immediate escalation**: an empty `||` fold over a pack in
  `static_metacall` warns `statement has no effect [-Wunused-value]`. Silencing
  it with `(void)(fold)` makes GCC escalate the enclosing lambda to consteval:
  `call to consteval function '<lambda closure object>rqt::static_metacall<Gadget>(...)::<lambda(...)>{id, t, a}...' is not a constant expression`,
  `'id' is not a constant expression`. Skip empty packs with `if constexpr`.
- QML warns `Parameter "value" is not declared. Injection of parameters into signal handlers is deprecated`
  for `onValueChanged: ... value`. The handler must use the
  `function(value) {...}` form. The parameter NAME still comes from our table.

Not done: alternative (3), Qt's per-instance dynamic meta-object
(`QAbstractDynamicMetaObject` in `QtCore/private/qobject_p.h`), which would
allow a plain `class Worker : public QObject` plus one `rqt::bind(this)`. It
is private API, and QML installs its own `QQmlVMEMetaObject` in the same slot
(`d_ptr->metaObject`) for objects with QML-declared properties, so the two
would collide. Spike 07's base already respects that slot first
(`metaObject()` returns `d_ptr->dynamicMetaObject()` when set). I did not
pursue it because (1) works.

Recommendation for the library (the lead's requirement (2) asks for "any Qt
base"): `template<class B = QObject> class rqt::Object : public B`, a mixin
templated on the BASE only, with this spike's dispatch. Bind with E3 (or E2)
by default, so QML-created subclasses work. Offer the 2-line `staticMetaObject`
opt-in for `qobject_cast`, PMF connect and `qmlRegisterType`, plus
`rqt::cast` and `rqt::connect` for classes that skip it.
