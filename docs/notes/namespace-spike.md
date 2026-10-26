# Namespace meta-objects (matrix rows A-04 and D-03): spike

Status: built (branch feat/namespace-meta, `include/reflect_moc/qt/namespace.hpp`, test `parity_namespace_meta`). The note below is the
spike as written; where the build differs, the matrix rows A-04 and D-03 and `namespace.hpp` are authoritative. Differences: the
scope key is `namespace_scope<Ns>` with `scope_of`, the ADL pair takes `E const&` and is constrained to enums the namespace
registers (and the enum behind a registered QFlags), and `check_namespace` also rejects enums in a nested namespace that has no
meta-object of its own.

**Verdict: build it, with the verdict `partial`.** Everything moc generates for `Q_NAMESPACE`, `Q_ENUM_NS` and `Q_FLAG_NS` can be
generated, and the result equals moc's, line by line. What does not carry over is the spelling: a namespace is never closed
for a header-only library, so the user writes three short lines in place of one macro, and the order of those lines
matters. Nothing is silently wrong if all three are written; with the third left out, an enum declared late is silently
missing.

Nothing in `include/` is changed on this branch. The experiment is a 83-line header next to the notes,
`docs/notes/namespace-spike/namespace_meta_experiment.hpp`, that reuses the library's consteval helpers.

## What the user would write

```cpp
namespace Outer::N {
enum class [[= rqt::enum_]] E { a, b };                       // Q_ENUM_NS(E)
enum Plain { x = 1, y = 2 };
using Plains = QFlags<Plain>;
inline constexpr rqt::enum_decl plains_flag{^^Plains, true};  // Q_FLAG_NS(Plains)

using rqt::qt_getEnumMetaObject;                              // 1: the ADL pair, BEFORE line 2
using rqt::qt_getEnumName;
inline QMetaObject const& staticMetaObject = rqt::namespace_meta<>;   // 2: Q_NAMESPACE
inline bool const rqt_namespace_checked = rqt::check_namespace<>();   // 3: catches an enum declared after line 2
}  // namespace Outer::N
```

Lines 1 to 3 could be one `RQT_NAMESPACE` macro in the compat header (a using-declaration and two variables can come
out of a macro); the macro-free spelling is the three lines.

## Questions asked, and the answers (each has a file in `namespace-spike/`)

| question | answer | evidence |
|---|---|---|
| Does GCC 16.2 reflect namespace members? | Yes. `members_of(^^Outer::N)` lists enums, classes, functions, variables and the other declarations. | `two_namespaces_one_tu.cpp` |
| Does `current_namespace()` as a default template argument give the use-site namespace? | Yes, also for a nested namespace (`Outer::N`) and for a second namespace in the same translation unit. A namespace reopened before the line is seen whole. | `two_namespaces_one_tu.cpp` prints `N: Outer::N 2 enums; M: Outer::M 1 enums` |
| Can a namespace-scope `QMetaObject` be built from `metaObjectData<void, tag>`? | Yes. `inline constexpr QMetaObject ns_meta<Ns>` with null super, null static meta-call, null extradata, class name `Outer::N`; enums as `EnumData` (`EnumIsScoped` for an `enum class`). The consteval helpers `make_enums`, `string_list`, `enum_real`, `enum_name` take `info` and needed no change. | `oracle_main.cpp` against `rqt_main.cpp`: the output, including `dump_class` of the namespace and of a class, is byte-identical |
| `QMetaEnum::fromType<N::E>()`? | Works: valid, name `E`, scope `Outer::N`, `valueToKey` right. | same run |
| `QMetaType::fromType<N::E>().metaObject()`? | Equals `&N::staticMetaObject`. | same run |
| A class property of type `N::E`? | `QMetaProperty::enumerator()` is valid and `isEnumType()` true only if the class's meta-object carries `extradata = { link<N::staticMetaObject>, nullptr }`, as moc writes it. Without it `enumerator().valid=0`, `isEnumType=0` and the dump differs in the property flags. With it the property reads as `a`, writes from the key `"b"`, and the dump equals moc's. The extradata array goes in the `relatedMetaObjects` slot of `QMetaObject::d` (the fifth), not in the last one. | `rqt_main.cpp` against the same file with `nullptr` (the `diff` shows only `enumerator().valid`, `isEnumType` and the `E` flag of the dump) |
| `Q_FLAG_NS` on a flags alias? | Identical to moc (`flag=1`, `enumName=Plain`) once `is_enum_declaration` accepts a namespace-scope `enum_decl` variable: `is_static_member` is false for a variable in a namespace, so `is_enum_declaration` needs `|| meta::is_namespace(meta::parent_of(m))`. A one-line change; tried on a scratch copy of the headers. `Q_FLAG_NS(Plain)` on a plain enum gives `EnumFlags{}` in moc (not `EnumIsFlag`), and `[[=rqt::enum_]]` on the plain enum gives the same (`flag=0` in both dumps); `[[=rqt::flag]]` on a plain enum would set `EnumIsFlag` (read from `enum_data_for`, not run), so the flags case is the alias. | `Plains` case in a scratch run |
| ODR across two translation units? | Same address of `staticMetaObject` in both, `-flto -Wodr -Wlto-type-mismatch` silent. | `odr_a.cpp`, `odr_b.cpp`, `odr_shared.hpp` |

## What does not work, or only in an order

1. **The using-declarations must come before the `staticMetaObject` line.** Building the meta-object instantiates
   `QMetaTypeInterfaceWrapper<E>` and with it `QtPrivate::IsQEnumHelper<E>`, which is evaluated once, at that moment. If
   the ADL pair is not yet visible, Qt's catch-all `qt_getEnumMetaObject(const T&)` wins and stays chosen: `QMetaEnum::fromType<E>()`
   then stops the build with Qt's own `static_assert` ("only works with enums declared as Q_ENUM, Q_ENUM_NS ..."). It is loud, not silent. The parameter form
   (`E` or `E const&`) does not matter. `using_line_order.cpp` has both orders.
2. **An enum declared after line 2 is silently missed.** `members_of` in the initializer sees what is declared before it.
   A variable template that is only named by a reference (`inline const`, not `constexpr`) does not defer this: GCC instantiates
   it at the point of the declaration that names it, so it saw the same prefix. Only a function body is instantiated at the end of
   the translation unit, and there `members_of` sees the whole namespace (`members_of_view.cpp`: the constexpr variable at the
   line counted 2 members, the lazy variable 3, the function body and `main` 7).
3. **That makes the third line the detector.** `rqt::check_namespace<>()` is a non-constexpr function template whose body
   `static_assert`s that the number of annotated enums now equals the number in the snapshot taken at line 2. Declared as
   the initializer of an `inline bool const`, its body is instantiated at the end of the translation unit and the
   build stops with `an enum annotated rqt::enum_ or rqt::flag is declared after the rqt namespace line`. Tested with and
   without the late enum (`late_enum_detected.cpp`); an unannotated enum after the line is correctly not an error. Without
   line 3 the late enum is invisible: `QMetaEnum::fromType<Late>()` compiles (the ADL function answers for every enum of the
   namespace) and returns an invalid `QMetaEnum` at run time (`late_enum_silent.cpp` prints `Late: valid=0 (ns lists 1 enums)`). A `requires` clause on the ADL pair
   that the enum is in the snapshot would turn that into the same loud failure as item 1; not tried.
4. **A class in the namespace** gets the extradata link only if the namespace's `staticMetaObject` is declared before the
   class. The link itself was tried by hand (`rqt_main.cpp`). Detecting the need is cheap in principle (`parent_of(^^D)` is a namespace that
   declares a member `staticMetaObject`), the order is the one a header already has (`Q_NAMESPACE` first), and a class declared before
   line 2 would not get the link; that detection is not built.
5. **Inline namespaces are not handled.** An enum in `namespace v1` (inline) inside `M` has `parent_of` = `v1`, not `M`;
   the ADL function would look for `v1::staticMetaObject` and the build stops in Qt's metatype code. A documented limit.
6. **Namespace class info.** `all<classinfo>(ns)` and `[[=rqt::classinfo]]` on a namespace were not tried (annotating a
   namespace-definition is accepted by the grammar; GCC's `annotations_of` on a namespace was not exercised here).
7. Not tried: functions in a namespace (moc ignores `Q_INVOKABLE` there too), `Q_NAMESPACE_EXPORT`, an anonymous namespace.

## What the library would have to change

Mechanical, and the same work as for any scope that is not a class:

- `tables.hpp`: `enum_list<D>`, `classinfo_list<D>`, `strings<D>`, `string_index<D>` are keyed on `template <class D>`.
  The helpers under them take `info`, so the key can become `template <info Scope>` with `^^D` for a class (the
  experiment duplicates 40 lines instead). `registered_enum` returns nullopt for a non-class owner (`is_class_type`); a
  namespace owner needs the same call. `is_enum_declaration` needs the one-line relaxation above.
- `metadata.hpp`: `meta_content<D>` takes the method, property and constructor sets as empty for a namespace and passes
  `PropertyAccessInStaticMetaCall`, as moc does (`oracle_main.cpp` shows moc's own flag).
- `object.hpp`: `static_meta_object<D>` writes `extradata` (item 4) for a class whose parent namespace declares a
  `staticMetaObject`.
- New: `rqt::namespace_meta<Ns = meta::current_namespace()>`, the ADL pair constrained to enums of a namespace, and
  `rqt::check_namespace<>`.

## Recommendation

Build it as **partial**: the meta-object, `QMetaEnum::fromType`, `QMetaType::metaObject` and the enumerator of a
property of a namespace enum all equal moc's, which is what A-04 and D-03 ask for. State in the matrix note the three lines,
the order of line 1 before line 2, the late-enum check, and the inline-namespace limit. The estimate is about a day of
library work plus a `parity_namespace_meta` test with a moc twin in the pattern of `parity_gadget`; the experiment already
shows what the twin compares. If a macro-free spelling with fewer lines matters more than the matrix verdict, the honest
alternative is to record both rows as a documented limit, because the order dependence cannot be removed: GCC evaluates
`members_of` at the point of the use, and only a function body gives the end-of-translation-unit view that a constant needs
before the end.

## How to reproduce

```sh
# the oracle: real moc on oracle_moc.h, printed by probe_common.hpp
docker run --rm -v "$PWD":/src:ro -w /src/docs/notes/namespace-spike reflect-moc/gcc16-qt610 sh -c '
  /opt/qt/libexec/moc oracle_moc.h -o /tmp/moc_oracle.cpp &&
  g++ -std=gnu++26 -fPIC -isystem /opt/qt/include/QtCore -isystem /opt/qt/include -I. -I/src/tests/qt \
      oracle_main.cpp /tmp/moc_oracle.cpp -o /tmp/oracle -L/opt/qt/lib -lQt6Core && LD_LIBRARY_PATH=/opt/qt/lib /tmp/oracle'
# the same through reflect-moc
docker run --rm -v "$PWD":/src:ro -w /src/docs/notes/namespace-spike reflect-moc/gcc16-qt610 sh -c '
  g++ -std=gnu++26 -freflection -fPIC -I/src/include -I. -I/src/tests/qt -isystem /opt/qt/include/QtCore -isystem /opt/qt/include \
      rqt_main.cpp -o /tmp/rqt -L/opt/qt/lib -lQt6Qml -lQt6Gui -lQt6Widgets -lQt6Core && LD_LIBRARY_PATH=/opt/qt/lib /tmp/rqt'
```

The two outputs are identical (checked with `diff`). The scratch variants named above (no extradata, the flags alias, a late
enum read through `fromType`) were one-line edits of `rqt_main.cpp` and are described rather than kept.
