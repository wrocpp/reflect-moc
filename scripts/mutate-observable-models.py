#!/usr/bin/env python3
"""Mutation testing for examples/qt/observable_models, by hand-listed textual mutants.

Mull needs clang and the image has only GCC, and the changed code here is ordinary run-time code, so
this applies one small mutation at a time to a COPY of the repository, rebuilds the two observable-model
tests and runs them. A mutant is killed when a test fails, aborts or times out; it survives when both
pass; it is not viable when it does not compile. Surviving mutants are the output that matters: each is
either a missing test or an equivalent mutant, and the table says which one it was judged to be.

Run it in the image (the repository is mounted read-only; the work happens in /tmp):

  docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 python3 scripts/mutate-observable-models.py [name...]
"""
import os
import shutil
import subprocess
import sys

SRC = os.environ.get("RQT_SRC", "/src")
WORK = os.environ.get("RQT_MUTATE_WORK", "/tmp/rqt-mutate")
TARGETS = ["capability_observable_models_stress", "capability_observable_models_flaws_detected",
           "capability_observable_container_announcements"]
TEST_TIMEOUT_SECONDS = 180
DIR = "examples/qt/observable_models/"
CONTAINER, MODEL, TRAITS = DIR + "observable_container.hpp", DIR + "container_model.hpp", DIR + "container_traits.hpp"

# (name, file, old text, new text, judged equivalent?)
MUTANTS = [
    ("container: announce an insert after it", CONTAINER,
     "emit entryAboutToBeAdded(key);\n    Traits::insert(m_items, key, std::move(value));",
     "Traits::insert(m_items, key, std::move(value));\n    emit entryAboutToBeAdded(key);", False),
    ("container: never confirm an insert", CONTAINER, "    emit entryAdded(key);\n", "", False),
    ("container: remove without checking the key", CONTAINER,
     "if (!Traits::contains(m_items, key)) return false;\n    emit entryAboutToBeRemoved(key);",
     "emit entryAboutToBeRemoved(key);", False),
    ("container: never confirm a remove", CONTAINER, "    emit entryRemoved(key);\n", "", False),
    ("container: never confirm a replace", CONTAINER, "      emit entryChanged(key);\n", "", False),
    ("container: never announce a replace", CONTAINER, "      emit entryAboutToBeChanged(key);\n", "", False),
    ("container: move without checking the request", CONTAINER,
     "      if (!Traits::can_move(m_items, from, to)) return false;\n", "", False),
    ("container: never confirm a move", CONTAINER, "      emit entryMoved(from, to);\n", "", False),
    ("container: never announce a move", CONTAINER, "      emit entryAboutToBeMoved(from, to);\n", "", False),
    ("container: clear an empty container", CONTAINER,
     "    if (Traits::size(m_items) == 0) return false;\n", "", False),
    ("container: never confirm a clear", CONTAINER, "    emit cleared();\n", "", False),
    ("container: never announce a clear", CONTAINER, "    emit aboutToBeCleared();\n", "", False),
    ("traits: a list refuses an insert at its end", TRAITS,
     "static bool can_insert(const QList<V>& c, Key k) { return k >= 0 && k <= c.size(); }",
     "static bool can_insert(const QList<V>& c, Key k) { return k >= 0 && k < c.size(); }", False),
    ("traits: a list accepts the index one past its end", TRAITS,
     "static bool contains(const QList<V>& c, Key k) { return k >= 0 && k < c.size(); }",
     "static bool contains(const QList<V>& c, Key k) { return k >= 0 && k <= c.size(); }", False),
    ("traits: a list move to itself is allowed", TRAITS,
     "return contains(c, from) && contains(c, to) && from != to; }", "return contains(c, from) && contains(c, to); }", False),
    ("traits: a list cannot be moved", TRAITS,
     "  static constexpr bool movable = true;", "  static constexpr bool movable = false;", False),
    ("traits: a map counts the insert row from upperBound", TRAITS,
     "std::distance(c.cbegin(), c.lowerBound(k))", "std::distance(c.cbegin(), c.upperBound(k))", True),
    ("traits: a map finds the wrong key for a row", TRAITS,
     "std::next(c.cbegin(), row).key()", "std::next(c.cbegin(), row ? row - 1 : 0).key()", False),
    ("traits: a hash accepts a key it already holds", TRAITS,
     "static bool can_insert(const QHash<K, V>& c, const K& k) { return !c.contains(k); }",
     "static bool can_insert(const QHash<K, V>&, const K&) { return true; }", False),
    ("traits: a set can be assigned", TRAITS,
     "  static constexpr bool assignable = false;", "  static constexpr bool assignable = true;", False),
    ("model: a move down is counted one row short", MODEL,
     "target > source ? target + 1 : target", "target", False),
    ("model: an unordered insert is announced at row 0", MODEL,
     "      return m_order.size();", "      return 0;", False),
    ("model: an unordered key is never listed", MODEL, "m_order.append(key);", "(void)key;", False),
    ("model: an unordered key is never unlisted", MODEL, "      if constexpr (Traits::order == Order::unordered) m_order.removeAt(m_pendingRow);\n", "", False),
    ("model: a removal is announced for row 0", MODEL,
     "m_pendingRow = static_cast<int>(rowOf(key));", "m_pendingRow = static_cast<int>(rowOf(key)) * 0;", False),
    ("model: a change is reported for row 0", MODEL,
     "const QModelIndex changed = index(static_cast<int>(rowOf(key)));",
     "const QModelIndex changed = index(static_cast<int>(rowOf(key)) * 0);", False),
    ("model: a reset is never ended", MODEL, "      endResetModel();\n", "", False),
    ("model: a reset keeps the unordered key list", MODEL,
     "      if constexpr (Traits::order == Order::unordered) m_order.clear();\n", "", False),
    ("model: rowCount ignores the parent", MODEL,
     "    return parent.isValid() ? 0 : static_cast<int>(m_source->size());",
     "    (void)parent;\n    return static_cast<int>(m_source->size());", False),
    ("model: the key role answers nothing", MODEL,
     "    if (role == KeyRole) return QVariant::fromValue(key);\n", "", False),
    ("model: a sorted insert is announced at the end", MODEL,
     "      return Traits::insert_row(m_source->items(), key);", "      { (void)key; return m_source->size(); }", False),
]


def run(cmd, **kw):
    return subprocess.run(cmd, text=True, capture_output=True, **kw)


def main(argv):
    if os.path.exists(WORK):
        shutil.rmtree(WORK)
    shutil.copytree(SRC, WORK, ignore=shutil.ignore_patterns("build", ".git"), symlinks=True)
    os.chdir(WORK)
    configured = run(["cmake", "--preset", "qt"])
    if configured.returncode:
        sys.exit("configure failed:\n" + configured.stdout[-2000:] + configured.stderr[-2000:])
    baseline = run(["cmake", "--build", "--preset", "qt", "--target", *TARGETS])
    if baseline.returncode:
        sys.exit("baseline does not build:\n" + baseline.stderr[-3000:])

    def exe(name):
        for root, _, files in os.walk("build"):
            if name in files and os.access(os.path.join(root, name), os.X_OK):
                return os.path.join(root, name)
        raise SystemExit(f"no binary {name}")

    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", LC_ALL="C.UTF-8")

    def tests_pass():
        for name in TARGETS:
            try:
                if run([exe(name)], env=env, timeout=TEST_TIMEOUT_SECONDS).returncode:
                    return False
            except subprocess.TimeoutExpired:
                return False
        return True

    assert tests_pass(), "the unmutated tests must pass first"
    chosen = [m for m in MUTANTS if not argv or any(a in m[0] for a in argv)]
    rows, killed, survived, unviable = [], 0, [], 0
    for name, path, old, new, equivalent in chosen:
        original = open(path).read()
        if old not in original:
            sys.exit(f"mutant text not found in {path}: {name}")
        open(path, "w").write(original.replace(old, new, 1))
        try:
            built = run(["cmake", "--build", "--preset", "qt", "--target", *TARGETS])
            if built.returncode:
                verdict = "not viable"
                unviable += 1
            elif tests_pass():
                verdict = "SURVIVED (judged equivalent)" if equivalent else "SURVIVED"
                survived.append((name, equivalent))
            else:
                verdict = "killed"
                killed += 1
        finally:
            open(path, "w").write(original)
        print(f"{verdict:30} {name}", flush=True)
        rows.append((name, verdict))
    equivalents = sum(1 for _, e in survived if e)
    print(f"\nMutation: hand-listed textual mutants on observable_models, {len(chosen)} mutants: {killed} killed, "
          f"{len(survived)} survived ({equivalents} judged equivalent), {unviable} not viable")
    return 1 if any(not e for _, e in survived) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
