# Private slots and private properties (matrix rows B-14 and C-09): spike

Status: spike, nothing in `include/` is changed. Experiments are in `docs/notes/private-slot-spike/`. Every claim below names the
file or command that produced it; what was not run is marked "not tried".

**Verdict: build it as `partial`, with a spelling, and write no library code for it.** A class that wants `Q_PRIVATE_SLOT(d_func(), void onTick(int))`
declares a private slot in the class and defines it next to the private class, as a one-line forwarder. The same holds for
`Q_PRIVATE_PROPERTY`: private forwarding accessors plus an ordinary `RQT_PROPERTY`. The result equals what real moc produces,
with one difference (parameter names, below). A spelling with no forwarder, one that takes the moc argument text, cannot work in a
header-only library, and reflection cannot name a member of the incomplete d-pointer class. Both are measured below.

## What the user would write

```cpp
// widget.h: WidgetPrivate stays incomplete here
class WidgetPrivate;
class Widget : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int level READ level)
  RQT_PROPERTY(int depth READ depth WRITE setDepth)     // Q_PRIVATE_PROPERTY(Widget::d_func(), int depth READ depth WRITE setDepth)
 public:
  explicit Widget(QObject* p = nullptr);
  ~Widget() override;
  int level() const { return 1; }
  [[= rqt::signal_function]] void changed() { rqt::emit{this}(); }
 private:
  [[= rqt::slot]] void onTick(int);                      // Q_PRIVATE_SLOT(d_func(), void onTick(int))
  [[= rqt::slot]] void reset();                          // Q_PRIVATE_SLOT(d_func(), void reset())
  int depth() const;
  void setDepth(int);
  WidgetPrivate* d_ptr;
  WidgetPrivate* d_func() const { return d_ptr; }
};

// widget.cpp, which includes widget_p.h
void Widget::onTick(int v) { d_func()->onTick(v); }
void Widget::reset() { d_func()->reset(); }
int Widget::depth() const { return d_func()->depth(); }
void Widget::setDepth(int d) { d_func()->setDepth(d); }
```

This is `rqt_main.cpp` (the four definitions are placed after a `twin_private.hpp` include, standing for `widget_p.h`).
`capability_private_members.cpp` already shows that private slots, private functions and private data behind a property work
with no extra line in the class, so nothing here needs library support.

## What real moc emits

Command: `moc twin.h` in the image `reflect-moc/gcc16-qt610`, on `twin.h` (a `Widget` with an incomplete `WidgetPrivate`,
two `Q_PRIVATE_SLOT`s, one `Q_PRIVATE_PROPERTY`, one `Q_PROPERTY`, one signal); output read from `/tmp/moc_twin.cpp`.

| piece | what moc wrote |
|---|---|
| string table | the slot names `onTick`, `reset` and the property name `depth` are ordinary entries |
| slots | `QtMocHelpers::SlotData<void(int)>(.., QMC::AccessPrivate, QMetaType::Void, {{ QMetaType::Int, 2 }})` and `SlotData<void()>(.., QMC::AccessPrivate, ..)`: no parameter name (the moc argument text has none) |
| property | `PropertyData<int>(.., QMetaType::Int, QMC::DefaultPropertyFlags \| QMC::Writable \| QMC::StdCppSet)`, the same flags as a plain `Q_PROPERTY` with READ and WRITE |
| method order | the signal first, then the private slots in declaration order (ids 0, 1, 2) |
| dispatch | `_t->d_func()->onTick((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])))`, `_t->d_func()->reset()`, and for the property `*reinterpret_cast<int*>(_v) = _t->Widget::d_func()->depth()` and `_t->Widget::d_func()->setDepth(*reinterpret_cast<int*>(_v))` |
| `IndexOfMethod` | only the signal is listed (`indexOfMethod<void (Widget::*)()>(_a, &Widget::changed, 0)`), as for any slot |

The thing to see in the dispatch lines: moc spells the call as `d_func()->onTick(...)` inside `qt_static_metacall`, in
`moc_widget.cpp`, which is compiled in a translation unit that has included the private header. That is the whole trick. The
class is incomplete in `widget.h` and complete in the one file that holds the meta-object.

## Questions asked, and the answers

| question | answer | evidence |
|---|---|---|
| Can reflection name a member of the incomplete d-pointer class? | No. `members_of(^^WidgetPrivate, access_context::unchecked())` while `WidgetPrivate` is only forward-declared throws `std::meta::exception`: "neither complete class type nor namespace". GCC also warns at the later definition: `defining 'WidgetPrivate', which previously failed to be complete in a SFINAE context [-Wsfinae-incomplete=]`. Once the class is defined the same call lists `onTick`. | `incomplete_members_of.cpp`, built with `-DASK_MEMBERS` (error) and `-DAFTER` (compiles) |
| Can a header write `d_func()->onTick(..)` and let the compiler resolve it once the private class is complete? | In one translation unit, yes: a function-template body with a dependent receiver (`w->d_func()->onTick(..)`, `w` of type `W*`) is instantiated at the end of the translation unit, so a private class defined later in the file is fine. Prints `got=7`. | `deferred_thunk.cpp` |
| Does that survive a second translation unit that has only the public header? | No. The pointer to the thunk lives in a `static constexpr` member, which every translation unit that uses the class instantiates, and there the private class is incomplete: `error: invalid use of incomplete type 'class WidgetPrivate'`. A header-only meta-object is built in every translation unit that takes `staticMetaObject`; moc's is built in one. | `deferred_thunk_client.cpp` |
| Does the forwarder spelling equal moc's meta-object? | Yes, apart from one field. The dump of `QMetaObject` (methods, access, parameter types, property flags, order) and the behaviour (property read 3, write 9, read 9; `invokeMethod` of `onTick(int)` and `reset()` both return true) are the same. The one difference: `parameterNames()` of `onTick(int)` is empty in moc's and `v` in reflect-moc's, which reflection evidently takes from the definition `void Widget::onTick(int v)` (the declaration has no name). | `run_twin.sh`; `diff` of `/tmp/oracle.out` and `/tmp/rqt.out` shows only that line |
| Is the class size or the ODR affected? | Not measured. The forwarders are ordinary out-of-line member functions, so there is nothing new to measure; not tried. | - |

## What does not carry over

1. **The moc argument text cannot be accepted.** `Q_PRIVATE_SLOT(d_func(), void onTick(int))` hands the macro a declaration; a
   preprocessor macro cannot split it into a name and a parameter list, and `RQT_PROPERTY` only works because
   `parse_property(#__VA_ARGS__)` is a `constexpr` parser run on the stringised text. A `RQT_PRIVATE_SLOT` could do the same for
   the signature text. But the dispatch still needs `d_func()->onTick(..)` to be compiled where the
   private class is complete, and the third row of the table shows that a header-only meta-object cannot offer that. A macro-based spelling
   is therefore not just inconvenient, it is not buildable without changing where the meta-object lives.
2. **One forwarder per private slot or accessor.** The user writes a body that moc generated. For a class with many
   private slots that is boilerplate; it is also checked by the compiler (a typo is an error), unlike a string.
3. **Parameter names show up** in `QMetaMethod::parameterNames()` where moc's have none. Who reads them (QML tooling, for one) was not checked.
4. **A forwarder is a real member function**, so `private:` code in the class can call it, and `QMetaObject::invokeMethod` on a
   private slot works exactly as for moc's (`invoke onTick(5): 1` in both runs).

## What the library would change

Nothing for the forwarder spelling. Two small, optional items, neither built:

- A line in the matrix note for B-14 and C-09 stating the spelling, the same way A-04 states the three namespace lines.
- Optionally a test `parity_private_slot` in the pattern of `parity_gadget`, which is the `run_twin.sh` comparison with the
  `names=` field masked. Estimate half a day; the experiment already has the twin and the probe.

The alternative that was weighed and rejected: a `RQT_PRIVATE_SLOT` macro whose thunk is instantiated only in the translation
unit that includes the private header (an `RQT_DEFINE_PRIVATE(Widget)` line in `widget.cpp`, which would hold the meta-object
itself, as moc does). It would need the meta-object to move out of the header for such classes: a new, separate way of building
`staticMetaObject`. Estimated at several days and not tried; it gives up the property that makes the library header-only.

## Recommendation

Mark B-14 and C-09 `partial`: the generated meta-object and the behaviour equal moc's; the spelling is a forwarder per private slot
or accessor, and the parameter names differ. Do not build a macro form. If a customer has hundreds of `Q_PRIVATE_SLOT`s, the honest
answer is a mechanical rewrite (the forwarder is derivable from the macro text) and not a library feature; that rewrite tool
was not tried.

## How to reproduce

```sh
# lock, then (the repo mounted read-only; the image has moc and Qt)
docker run --rm -v "$PWD/../../..":/src:ro -w /src/docs/notes/private-slot-spike reflect-moc/gcc16-qt610 sh run_twin.sh
# prints the one differing line (the names= field of onTick); with no difference it would print IDENTICAL
docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 sh -c '
  g++ -std=gnu++26 -freflection -DASK_MEMBERS -c incomplete_members_of.cpp -o /tmp/x.o      # the error
  g++ -std=gnu++26 -freflection -DAFTER -c incomplete_members_of.cpp -o /tmp/x.o            # compiles
  g++ -std=gnu++26 deferred_thunk.cpp -o /tmp/t && /tmp/t                                     # got=7
  g++ -std=gnu++26 -c deferred_thunk_client.cpp -o /tmp/c.o                                   # the error'
```

GCC 16.2.0 and Qt 6.10.3 in `reflect-moc/gcc16-qt610`, the compiler version read with `g++ --version` in the image.
