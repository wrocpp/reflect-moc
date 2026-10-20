# Porting the Qt layer to clang-p2996: what stands in the way

Status: a desk inventory from the sources, **not a build**. No clang-p2996 image exists on the machines this was
written on (the clang in the docker image is not the reflection fork, and the fork is built from source, which takes
hours), so nothing here was compiled with clang. The aim is to list what a real spike has to answer, with counts that
can be checked by `grep`.

## What the library uses that clang-p2996 is known to lack

The README lists three. Two still apply; the third no longer does (the library loops over `annotations_of` and never
calls `annotations_of_with_type`: `grep -rn annotations_of_with_type include` finds nothing).

| facility | where | lines | what it is for |
|---|---|---|---|
| `std::meta::current_function()` | `RQT_SELF` (qt/macros.hpp), `signal_id` in core/object.hpp and qt/object.hpp, core/introspection.hpp | 7 in 5 files | finds the class of the member function a signal is emitted from, so a signal call needs no explicit owner |
| `std::meta::exception` | the consteval table builders (qt/tables.hpp and the other headers and READMEs counted) | 21 in 7 files | a compile-time error with the reflection that caused it, as a diagnostic |

Everything else the code uses (`members_of`, `parent_of`, `offset_of`, `define_static_array`, `extract`,
`display_string_of`, `access_context`) is in the P2996 revision clang-p2996 implements, but each was only ever tried on
GCC 16.2, so "in the proposal" is not "works".

## What a spike should find out, in order of cost

1. **`std::meta::exception`**: replace it with a `static_assert` on a consteval predicate, or with a plain `throw`
   of a type that is not `std::meta::exception`, in the 21 places. Cheap, and it is a diagnostics change only. Check
   that the negative tests (the `limit_*` compile-fail tests match message text) still find their message.
2. **`current_function()`**: the hard one. Without it a signal member needs its owner given explicitly, which is what
   `rqt::static_signal` and `.from(this)` already do. So a clang build could support static signals first and leave
   the data-member `rqt::signal` for later. Count how many of the library tests use only static signals.
3. **Qt on that clang**: Qt 6.10's `QtMocHelpers` constexpr tables must compile under the fork's libc++ in C++26
   mode; the library relies on `std::define_static_array` and constexpr `std::string`, whose libc++ support is
   incomplete (the repository's guidance is to gate library facilities on feature-test macros).
4. **ABI and image**: an image with the fork and Qt (aqtinstall pulls a GCC-built Qt; its headers must also parse with
   clang) and a second CI job.

Decision to take before spending the hours: whether a clang build is worth having at all. The library's value is the
real `QMetaObject` built by constexpr from reflection, and GCC 16.2 already ships it; clang is a portability question,
not a missing feature.
