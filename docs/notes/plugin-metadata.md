# Plugin metadata (matrix row E-01)

`RQT_PLUGIN_METADATA` (`include/reflect_moc/qt/plugin.hpp`) is `Q_PLUGIN_METADATA(IID "x" FILE "x.json")` without moc. The row is
`partial`: the metadata and the entry points are what moc's are, the spelling and the size of the file are not. How to write it is in
`include/reflect_moc/qt/README.md`, "Plugin metadata"; the tests are in `tests/qt/plugin`. This note says what was measured.
It follows the spike on branch `spike/private-and-plugin` (draft PR #42), whose encoder was the starting point and whose limit it fixes.

GCC 16.2.0, Qt 6.10.3, aarch64 Linux in Docker (`reflect-moc/gcc16-qt610`). Nothing here was run on another compiler.

## What equals moc's

| What | How it is checked | Result |
|---|---|---|
| The CBOR bytes | `parity_plugin_cbor_<file>`: one executable per JSON file includes moc's output for the same class and compares moc's `qt_pluginMetaDataV2_*` array with the macro's, `memcmp` | identical for `meta.json` (113 bytes), `edge.json` (293), `dup.json` (53), `wide.json` (3373), `numbers.json` (328), `escapes.json` (157), `keys.json` (232: keys sharing more than eight bytes, a NUL in a key, a repeated empty key), a generated 20 KB description (13321), the two generated files just under the cap (24105 and 22773) and, with no `FILE`, 37 bytes |
| What a host sees | `parity_plugin_loader`: a moc-built and a reflect-moc-built plugin (same class name, two namespaces), both read by `QPluginLoader` before loading | same metadata object, same size and bytes behind `qt_plugin_query_metadata_v2` (the ELF note payload), `qobject_cast<IDemo*>` and `answer()` equal |
| No moc at all | `capability_plugin_metadata`: the reflect-moc plugin alone, as a shared library, in the global namespace and in one | the metadata read from the file (not loaded), the same after `instance()`, `qobject_cast`, `qt_metacast`, `unload()` |
| A static plugin | `capability_plugin_metadata_static`: the same source with `QT_STATICPLUGIN` and `Q_IMPORT_PLUGIN` | `QPluginLoader::staticPlugins()` lists it with the same metadata, and its instance answers |

What moc's CBOR does that a plain encoder gets wrong, each found by a failed comparison in the spike and pinned here: keys of every object
sorted by their bytes; a repeated key keeps its last value; every number is a double, written as an integer when it holds one
that fits 64 bits (`3.0` and `1e3` are `3` and `1000`), as eight bytes otherwise, never shortened; an integer literal past 2^53 is
rounded to a double; `-2^63` stays a double. Without `FILE` there is no `MetaData` key. `className` is the unqualified name, also
for a class in a namespace; the qualified name is only the first argument of Qt's entry-point macro.

The test breaks were made by hand and are not kept. The moc twin pointed at another JSON file made `parity_plugin_loader` fail three
checks (metadata object, size, bytes) and every `parity_plugin_cbor_*` that was built; flipping the key comparison made them all
fail with the first differing byte printed. Thirty hand-made mutants of the encoder (tie order of a repeated key, no deduplication,
`-2^63` as an integer, integral doubles not narrowed, a head one byte short, negative integers, the surrogate arithmetic and offset,
hex digits, the UTF-8 lead and limits, the `\b` and `\/` escapes, the sign of an exponent, the scale of a fraction, the exactness
bounds of a decimal, a descending sort, the count after deduplication, the key prefix four ways, the count of an array, the nesting
limit, a literal) were each caught by at least one test, two of them by a test input that no longer compiled. One survives and is
equivalent: a prefix one byte shorter only changes how soon a comparison ends. Four first runs survived and led to tests: the
surrogate escapes (`escapes.json` held the characters, not the escapes), the nesting limit, a lone surrogate and the bounds of a
decimal (`limit_plugin_json_nested_too_deep`, `_lone_low_surrogate`, `_lone_high_surrogate`, `limit_plugin_number_power_of_ten_*`).

## The size limit

The spike's encoder built a tree of values and copied it, and stopped at 5451 bytes with `constexpr evaluation operation count
exceeds limit of 33554432` (the default of `-fconstexpr-ops-limit`). This one reads the text once and writes the CBOR as it reads, into
plain arrays: a head is one byte reserved and filled in when the length or the count is known, and only a head that needs more bytes
moves what follows. An object is put in order at its closing brace, and left where it is if its keys are already ascending and none is
repeated; keys are compared on their first eight bytes before the rest, which took the sort-heavy shape from 33 to 49 KB. The compiler
counts every operation of a constant evaluation, and `std::vector` or `std::span` in the loop cost several times what a pointer costs
(an earlier version of this encoder with them built 12 KB and failed at 25 KB).

The largest file that builds under the default limit, by bisection, for six shapes of JSON (`docs/notes/plugin-metadata/limit_sweep.sh`,
`gen_json.py`; the second column is the first size that failed, all with the operation-count error). The operation count is about
350 per byte for an indented description and about 550 per byte for the same with no white space:

| shape | builds | fails |
|---|---|---|
| a description with features, one space per level | 92701 bytes | 96757 |
| the same with no white space (the most operations per byte) | 58697 | 61169 |
| an array of fractions (three or four bytes of text, nine of CBOR each) | 66994 | 70818 |
| an array of strings | 151218 | 160178 |
| one object of 2944 keys, not in order (the sort) | 48940 | 51116 |
| 100 nested one-member objects around an array of strings | 151812 | 160772 |

**`RQT_PLUGIN_JSON_MAX_BYTES` is 32768**, 56 percent of the worst shape (the description with no white space, 58697). The usual file is a
description of a few lines to a few KB; a 20 KB description (`rqt_plugin_realistic.json`, generated by the tests) passes with room. A
larger file stops at a `static_assert` that names the macro and the flag (`limit_plugin_json_too_large`). The cap is the safe value,
not the edge: to go past it, define the macro and pass `-fconstexpr-ops-limit=` to match; a file that is past the compiler's limit
anyway (a shape not measured here) gets GCC's own message, which names the flag. The cost per byte depends on the shape, which is why
the cap sits under the worst measured one. `capability_plugin_json_at_the_cap` keeps the claim true: it builds the two costliest
shapes (the list with no white space and the unsorted flat object), each just under the cap, so a change to the encoder that eats the
margin turns that test red. The loop limit (`-fconstexpr-loop-limit`, 262144 iterations of one loop) was not reached by any of these;
nesting is capped at 128 levels by the encoder (`json: nested too deep`), under the depth limit of 512.

Compile time, on a loaded machine, one translation unit with Qt's headers: the 123-byte `meta.json` 4.3 and 4.8 s, a 30 KB description
3.7 and 7.7 s. The noise is as large as the difference, so no figure is claimed. Memory was not measured.

## What does not work, or was not tried

- **The spelling.** `#embed` is a directive and cannot be written inside a macro argument: GCC 16.2 stops with
  `internal compiler error: in paste_all_tokens, at libcpp/macro.cc:1108` (spike, `-DEMBED_MACRO`). The file is embedded in a block
  next to the class, and the IID is repeated as an argument.
- **Where the file is looked for.** `#embed "x.json"` searches the directory of the source file that holds it, then the directories of
  `--embed-dir=`. The `-I` directories are not searched, for the quoted or the angle form (`#embed <x.json>` with only `-I` gave
  `no include path in which to search`). Checked with a probe: a file that exists only in the `--embed-dir` directory is found by
  both forms. The tests that embed a file made in the build tree pass `--embed-dir=` for it. moc's `FILE` searches its `-I` directories.
- **Numbers.** Exact for a fraction or an exponent with at most 15 digits and a power of ten up to 22 (Clinger's fast path), and for an integer of
  at most 19 digits. Any other stops the build with `json: this number needs a correctly rounded decimal conversion` and the byte
  (`limit_plugin_number_needs_rounding`, `limit_plugin_integer_past_2_64`). A correctly rounded conversion in `constexpr` was not written.
- **Errors.** A malformed file, a file that is not an object, a lone surrogate or a number past the limits is `error: uncaught
  exception 'rqt::detail::plugin::json_error{((const char*)"json: bad number"), 6}'`: the reason and the byte offset. GCC follows it with
  `could not convert 'md' from 'const int' to 'QPluginMetaData'`, which is noise from the failed reference. It is the throw that reports,
  so a build with `-fno-exceptions` was not tried.
- **Not checked:** clang and MSVC (the library needs GCC 16.2 for reflection and `#embed`); the macOS and Windows metadata sections (the
  ELF note is what was compared); raw control characters and invalid UTF-8 in the file (moc's reader may refuse what this passes
  through); more than one plugin class in one library; a class template as a plugin.
- `plugin.hpp` is not included by `qt.hpp`, so a build that never names it (the MSVC job on the spec bindings) does not see it.

## Reproduce

```sh
docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 sh -c \
  'cmake --preset qt && cmake --build --preset qt && ctest --preset qt -R "plugin"'
docker run --rm -v "$PWD":/src:ro -w /tmp reflect-moc/gcc16-qt610 sh /src/docs/notes/plugin-metadata/limit_sweep.sh    # about 15 minutes
```
