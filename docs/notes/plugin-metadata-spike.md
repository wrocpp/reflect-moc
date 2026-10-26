# Plugin metadata (matrix row E-01): spike

Status: spike, nothing in `include/` is changed. Experiments are in `docs/notes/plugin-metadata-spike/`. Every claim names the
file or command that produced it; what was not run is marked "not tried".

**Verdict: go, as `partial`.** A plugin built without moc loaded in a `QPluginLoader` host and answered like the one moc built:
same JSON metadata, same exported symbols, same ELF note payload, `qobject_cast<IDemo*>` works. The data path is
`#embed` into a constant, a `constexpr` JSON to CBOR encoder, and Qt's own `QT_MOC_EXPORT_PLUGIN_V2` for the entry points.
What does not carry over is the spelling: `FILE "meta.json"` becomes an `#embed` block next to the class, because `#embed` is a
directive and cannot be written inside a macro (GCC 16.2 stops with an internal compiler error if it is tried). Estimated at two
days to productise (below).

## What the user would write

```cpp
class DemoPlugin : public QObject, public IDemo {        // Q_OBJECT -> RQT_OBJECT; Q_INTERFACES stays, it expands to nothing
  RQT_OBJECT
  Q_INTERFACES(IDemo)
 public:
  int answer() const override { return 42; }
};

static constexpr unsigned char meta_json[] = {            // the FILE "meta.json" of Q_PLUGIN_METADATA
#embed "meta.json"
};
static constexpr auto rqt_plugin_blob = SPIKE_CBOR(meta_json, IDemo_iid, "DemoPlugin");
static constexpr auto& qt_pluginMetaDataV2_DemoPlugin = rqt_plugin_blob.bytes;
QT_MOC_EXPORT_PLUGIN_V2(DemoPlugin, DemoPlugin, qt_pluginMetaDataV2_DemoPlugin)
```

That is `rqt_plugin.cpp`. The last three lines would be one library macro, `RQT_PLUGIN_METADATA(DemoPlugin, IDemo_iid, meta_json)`,
which is not written; the spike spells them out. It has to be at namespace scope after the class, in the one `.cpp` of the plugin:
it defines `extern "C"` functions.

## Questions asked, and the answers

| question | answer | evidence |
|---|---|---|
| Does GCC 16.2 have `#embed`? | Yes. `-std=c++26`, `-std=gnu++26` build with `-Wall -Wextra -pedantic` and no diagnostic. `-std=c++20` builds with `warning: '#embed' before C++26 is a GCC extension [-Wc++26-extensions]`. `__has_embed("meta.json") == __STDC_EMBED_FOUND__` is true; `suffix(, 0)` and `limit(4)` parameters work. | `embed_probe.cpp`, built with each `-std=` in the image |
| Does the embedded data reach `constexpr`? | Yes. An array initialised by `#embed` can be indexed in `static_assert` (`json[0] == '{'`, size) and looped over in a `consteval` function (counted 22 quote characters, which `tr -cd '"' < meta.json \| wc -c` agrees with). | `embed_probe.cpp` |
| Does the path resolve like `FILE` in moc? | Relative to the file that holds the `#embed`: including `/src/embed_probe.cpp` from a `t.cpp` in another directory compiled. `-MD` lists `meta.json` as a dependency (counted once in the `.d` file), so a build system rebuilds when the JSON changes. moc's `-I` search for `FILE` and `#embed`'s `-I` search were not compared (not tried). | `embed_probe.cpp` included from `/tmp/sub/t.cpp`; `run_extra.sh` ("dependency tracking") |
| Can `#embed` be used inside a macro argument, so that `FILE` keeps its spelling? | No. `WRAP(` newline `#embed "meta.json"` newline `)` is an internal compiler error on GCC 16.2: `internal compiler error: in paste_all_tokens, at libcpp/macro.cc:1108`. | `embed_probe.cpp -DEMBED_MACRO` |
| Can minimal JSON become CBOR in constexpr code? | Yes, and the bytes equal moc's. A hand-written parser and encoder (`json_to_cbor.hpp`, 190 lines, `std::vector` and `std::string` used inside `consteval` only, the result copied into a plain array) produces the exact bytes of `qt_pluginMetaDataV2_*` for `meta.json` (113 bytes), for a 293-byte file with 17 integers at the CBOR width boundaries, doubles, empty containers, nested arrays and objects, `\u` escapes and a surrogate pair (`edge.json`), and for a file with repeated keys (`dup.json`). | `run_cbor.sh meta.json edge.json dup.json`: `IDENTICAL` three times, compared with `memcmp` against the array in the real moc output |
| What does moc's CBOR do that a simple encoder would get wrong? | Four things, each found by a failed comparison. (1) Keys of every JSON object come out **sorted** (by UTF-8 bytes in the case tried: `A`, `B`, `aa`, `b`, `é`, a key with U+FFE0 and one with U+1F600 matched), not in file order. (2) A repeated key keeps its **last** value (`dup.json`). (3) Every JSON number is read as a double: an integral value inside the 64-bit range is written as a CBOR integer (`3.0` and `1e3` come out as `3` and `1000`, `-0.0` as `0`), an integer literal past 2^53 is rounded (`9007199254740993` is written as 2^53), `9223372036854775807` and `-9223372036854775808` stay doubles (`0xfb`). (4) Doubles are always 8 bytes (`0xfb`), never shortened. | the first-difference bytes printed by `cbor_vs_moc.cpp` at each step |
| What is in the top level? | An indefinite map (`0xbf` ... `0xff`) with integer keys 2 (`IID`), 3 (`className`) and 4 (`MetaData`). The `IID` text is the macro argument; `className` is the class name as written. Inside, objects are definite-length maps. | the moc output of `plugin.h`; `plugin_cbor` in `json_to_cbor.hpp` |
| What does a plugin need besides the data? | The two entry points, and Qt supplies them: `QT_MOC_EXPORT_PLUGIN_V2(CLASS, MANGLED, MD)` in `<QtCore/qplugin.h>` defines `extern "C" qt_plugin_query_metadata_v2()` and `qt_plugin_instance()` (or, with `QT_STATICPLUGIN`, `qt_static_plugin_<name>()`), where `MD` is a reference to an array of `unsigned char`. moc's output only declares the array and invokes the macro. Nothing else is generated. | `moc plugin.h` and `grep QT_MOC_EXPORT_PLUGIN_V2 -A4 /opt/qt/include/QtCore/qplugin.h` |
| Does the reflect-moc plugin load and behave like moc's? | Yes. Built both as shared libraries from the same `meta.json`; `QPluginLoader` in a small host printed byte-identical `metaData()` JSON (`IID`, `MetaData` with the sorted keys, `className`, `version`, `archlevel`), `instance()` loaded, `qobject_cast<IDemo*>` succeeded, `answer()` returned 42, and `qt_metacast("org.example.IDemo/1.0")` answered. | `run_plugin.sh`: `SAME-HOST-VIEW` |
| Same exported symbols and section? | `nm -D` shows `qt_plugin_instance` and `qt_plugin_query_metadata_v2` as `T` in both. The reflect-moc one also exports a weak `W` symbol, the conversion operator of `QPluginMetaDataV2<..>`; moc's build did not. The ELF note `qt-project!` payload printed by `readelf -n` is identical (`NOTES-IDENTICAL`, after masking the offsets). | `run_plugin.sh` |
| A static plugin? | Both build with `-DQT_STATICPLUGIN` and define `qt_static_plugin_DemoPlugin()`. Loading one through `Q_IMPORT_PLUGIN` was not run (not tried). | `run_plugin.sh` ("static plugin build", compile only) |

## What does not work, or costs something

1. **The spelling changes.** `#embed` is a directive; the JSON file moves out of `Q_PLUGIN_METADATA(.. FILE ..)` into a visible
   block. The IID is repeated as a macro argument instead of read from the macro. Both are one-time edits per plugin.
2. **Compile time and the constexpr limit.** Measured on the image, one translation unit with the whole `qt.hpp`: 2.7 s for the
   113-byte file, 3.1 s for the 293-byte `edge.json`, 2.9 s for the same class with no metadata (noise is larger than the
   difference at this size). With a generated JSON of 4001 bytes the encoder took about 8.6 s wall in all; at 5451 bytes it stops
   with `'constexpr' evaluation operation count exceeds limit of 33554432` (default `-fconstexpr-ops-limit`). At 79 KB, with the limit raised
   to 2 billion, `cc1plus` was killed (the container's memory). The spike encoder is naive (it copies `Value` trees, and `SPIKE_CBOR`
   runs the whole encoder twice, once for the size and once for the bytes). Real plugin JSON is usually a few lines; larger
   files need a streaming encoder, which was not written. `run_big.sh` has the sizes up to 5451 bytes; the 79 KB runs were typed by hand with the same generator and are not kept.
3. **A bad file is a compile error with a reason.** `{"a": ,}` gives `error: uncaught exception '(const char*)"json: bad number"'`,
   followed by a secondary error in `to_array`. A missing file is `fatal error: nosuch.json: No such file or directory`.
   (`run_extra.sh`.) The first message is readable; the second is noise that a library `static_assert` could hide.
4. **Decimal numbers are not fully general.** The encoder is exact for up to 15 significant digits and a power of ten up to 22
   (Clinger's fast path) and stops with `json: this number needs a correctly rounded decimal conversion` otherwise. Integers need
   nothing more. A full correctly rounded conversion in `constexpr` was not written; whether `std::from_chars` for floating point is
   usable in a constant expression on GCC 16.2 was not tried.
5. **`MD` must be a reference to a C array.** `QPluginMetaData::copy` takes `II (&in)[ISize]`, so a `std::array` result does not fit by its signature (not built to see the error). The spike returns a struct holding a plain array and passes `blob.bytes`. In the object file moc's
   array is in read-only data (`r`) and the reference variable in the reflect-moc build is `d` (writable data), because
   it is a reference variable; the array itself is `r`. Harmless; not looked at further.
6. **Not tried:** a plugin class inside a namespace (the `MANGLEDNAME` argument, and whether `className` carries the qualification);
   `Q_PLUGIN_METADATA(IID "x")` with no `FILE`; `Q_PLUGIN_METADATA` on a class that has no `Q_INTERFACES`; the Windows and macOS metadata sections; clang
   (`#embed` support was checked on GCC 16.2 only); MSVC; `QFactoryLoader` lookup by
   `Keys`; more than one plugin class per library.

## What the library would have to add

- `include/reflect_moc/qt/plugin.hpp`: the encoder (about 190 lines now, to be reworked for the operation limit) and
  `RQT_PLUGIN_METADATA(Class, iid, json_array)`, which emits the blob, the array reference and the `QT_MOC_EXPORT_PLUGIN_V2` line.
  For a class in a namespace, a mangled name argument.
- Tests: a parity test that compiles `moc` output for a fixed set of JSON files and compares the bytes with `memcmp`
  (this spike's `cbor_vs_moc.cpp`, for which the table above is the specification), and a host test that loads a built plugin
  with `QPluginLoader` (this spike's `host.cpp`). The second needs a shared library in the test build; the repo's tests are
  executables, so the build system gets a small addition.
- Matrix note for E-01: the `#embed` block, GCC only for now (clang and MSVC not checked), the JSON subset, the number limit.

## Go / no-go

**Go.** The riskiest part, byte equality with moc's CBOR, is done and compared on 3 files. Effort estimate for the library
work: about two days, i.e. half a day to move the encoder to a single streaming pass (the operation limit above), half a day for the
namespaced-class and static-plugin cases, a day for the parity and host tests and the matrix text. If the customer's plugins
have JSON of a few hundred bytes, the spike encoder is already enough for the metadata, and only the macro and tests are missing.
The verdict stays `partial`, because the spelling differs and the file is GCC-only until `#embed` is checked elsewhere.

## How to reproduce

```sh
cd docs/notes/plugin-metadata-spike    # run each with the repo mounted at /src, the image reflect-moc/gcc16-qt610, under the shared lock
docker run --rm -v "$PWD/../../..":/src:ro -w /src/docs/notes/plugin-metadata-spike reflect-moc/gcc16-qt610 sh run_cbor.sh meta.json edge.json dup.json
docker run --rm -v "$PWD/../../..":/src:ro -w /src/docs/notes/plugin-metadata-spike reflect-moc/gcc16-qt610 sh run_plugin.sh
docker run --rm -v "$PWD/../../..":/src:ro -w /src/docs/notes/plugin-metadata-spike reflect-moc/gcc16-qt610 bash -c 'cp -r /src /tmp/w && cd /tmp/w/docs/notes/plugin-metadata-spike && bash run_extra.sh && bash run_big.sh'
# embed_probe.cpp: g++ -std=gnu++26 -c embed_probe.cpp ; add -DEMBED_MACRO for the internal compiler error
```

GCC 16.2.0, Qt 6.10.3, aarch64 Linux in Docker. `plugin.h` and `moc plugin.h` were the first read of moc's output; `plugin_edge.h` is the
twin the byte comparison uses.
