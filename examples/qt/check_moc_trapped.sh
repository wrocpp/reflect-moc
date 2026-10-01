#!/bin/sh
# Proof that building reflect-moc code never runs moc, by making moc impossible to run.
#
# Every `moc` in the image is replaced by a trap that records the call (with its
# caller) and fails. The script then builds the ported examples, the demos and the
# library tests. A build that needs moc breaks and leaves a record.
#
# Positive controls: the trap must record its own self-test call, and the
# differential test (the one target that runs real moc on purpose, as its oracle)
# must be the only target that fails to build.
#
# THIS SCRIPT RENAMES THE REAL moc. It refuses to run outside a container. Run it in
# a throwaway one, with the repository mounted read-only:
#
#   docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 \
#     sh examples/qt/check_moc_trapped.sh
#
# Environment:
#   RQT_TRAP_TMP     work directory inside the container (default /tmp/rqt-trap)
#   RQT_TRAP_QUICK=1 skip step 4, the library tests (the long step)
#   QTDIR            Qt installation (default /opt/qt)
#
# Exit status: 0 PASS, 1 FAIL, 2 refused to run.
set -u
export LC_ALL=C.UTF-8

[ -e /.dockerenv ] || [ -e /run/.containerenv ] || {
  echo "refusing to run outside a container: this script replaces moc (see the header)"
  exit 2
}

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
QT="${QTDIR:-/opt/qt}"
work="${RQT_TRAP_TMP:-/tmp/rqt-trap}"
trap_log="$work/moc-invoked.log"
differential=capability_differential_against_moc
status=0
mkdir -p "$work"

fail() { echo "FAIL: $*"; status=1; }

# Print and clear what the trap recorded since the last call; lines are left in $recorded.
recorded=""
take_record() {
  recorded=""
  if [ -s "$trap_log" ]; then recorded="$(cat "$trap_log")"; : > "$trap_log"; fi
}
expect_no_moc() {
  take_record
  if [ -n "$recorded" ]; then fail "$1 called moc:"; echo "$recorded" | sed 's/^/    /'; else echo "ok: $1: no moc call"; fi
}

echo "=== 1. replace every moc in the image with a trap"
: > "$trap_log"
count=0
find / -xdev \( -name moc -o -name moc.exe \) \( -type f -o -type l \) 2>/dev/null | grep -v '^/proc' > "$work/moc-paths.txt"
while read -r f; do
  [ -n "$f" ] || continue
  mv "$f" "$f.real" || { fail "cannot move $f (is the container read-only?)"; exit 1; }
  cat > "$f" <<EOF
#!/bin/sh
echo "MOC INVOKED: args=[\$*] parent=[\$(ps -o args= -p \$PPID 2>/dev/null | cut -c1-200)] cwd=[\$PWD]" >> "$trap_log"
exit 99
EOF
  chmod +x "$f"
  count=$((count + 1))
  echo "trap installed over $f"
done < "$work/moc-paths.txt"
[ "$count" -gt 0 ] || { fail "found no moc to trap"; exit 1; }
"$QT/libexec/moc" --version > /dev/null 2>&1
take_record
case "$recorded" in
  *"MOC INVOKED: args=[--version]"*) echo "ok: trap self-test recorded its call" ;;
  *) fail "the trap did not record its own self-test call" ;;
esac

echo "=== 2. the ported examples: configure and build each, compare output with the original"
for name in mandelbrot birthdayparty sliders queuedcustomtype; do
  rm -rf "$work/ported-$name"
  if cmake -S "$here/$name/ported" -B "$work/ported-$name" -G Ninja -DCMAKE_BUILD_TYPE=Release > "$work/ported-$name.log" 2>&1 &&
     cmake --build "$work/ported-$name" >> "$work/ported-$name.log" 2>&1; then
    expect_no_moc "ported/$name (configure and build)"
  else
    fail "ported/$name did not build (log: $work/ported-$name.log)"; tail -5 "$work/ported-$name.log"
    take_record
  fi
done
RQT_EXAMPLES_BUILD="$work/check" sh "$here/check.sh" ported > "$work/check.log" 2>&1 || { fail "check.sh ported failed"; grep -E "FAIL|BUILD FAILED" "$work/check.log" | head; }
grep -q "output: identical to the baseline" "$work/check.log" && echo "ok: ported harness output identical to the original's baseline ($(grep -c 'output: identical' "$work/check.log") examples)"
expect_no_moc "check.sh ported"

echo "=== 3. the demos compile with plain g++, no CMake"
flags="-std=c++26 -freflection -fPIC -I$repo/include -I$QT/include -I$QT/include/QtCore -I$QT/include/QtGui -I$QT/include/QtQml -I$QT/include/QtQuick -I$QT/include/QtQmlIntegration"
libs="-L$QT/lib -Wl,-rpath,$QT/lib"
g++ $flags "$repo/examples/demo/demo.cpp" $libs -lQt6Core -o "$work/rqt_demo" 2> "$work/demo.err" || { fail "console demo did not build"; head -5 "$work/demo.err"; }
if [ -x "$work/rqt_demo" ]; then
  "$work/rqt_demo" --tour 2>&1 | grep -q "invoked add(int) -> 10" && echo "ok: console demo ran its tour" || fail "console demo tour output is wrong"
fi
g++ $flags "$repo/examples/demo/demo_qml.cpp" $libs -lQt6Quick -lQt6Qml -lQt6Gui -lQt6Core -o "$work/rqt_demo_qml" 2> "$work/demo_qml.err" || { fail "Qt Quick demo did not build"; head -5 "$work/demo_qml.err"; }
expect_no_moc "the demos"

if [ "${RQT_TRAP_QUICK:-0}" = 1 ]; then
  echo "=== 4. skipped (RQT_TRAP_QUICK=1): the library tests"
else
  echo "=== 4. the library tests: only the differential oracle may need moc"
  rm -rf "$work/lib"
  cmake -S "$repo" -B "$work/lib" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DRQT_BUILD_QT=ON -DCMAKE_PREFIX_PATH="$QT" > "$work/lib-configure.log" 2>&1 || fail "the library tests did not configure"
  take_record
  # CMake asks moc for its version when it sees a target with AUTOMOC ON (the differential test).
  if [ -n "$recorded" ]; then
    echo "$recorded" | grep -v "args=\[--version\]" | grep -q . && { fail "configure called moc for something other than --version:"; echo "$recorded" | sed 's/^/    /'; }
    echo "note: configure asked moc for its version only (CMake does this for the differential target's AUTOMOC)"
  fi
  cmake --build "$work/lib" -j4 -- -k 0 > "$work/lib-build.log" 2>&1
  failed="$(grep -E '^FAILED:' "$work/lib-build.log" | sed 's/^FAILED: //' | awk '{print $1}' | sed 's#.*/##' | sort -u)"
  echo "targets that failed to build: ${failed:-none}"
  others="$(echo "$failed" | grep -v "^$differential" | grep -v '^$' || true)"
  [ -z "$others" ] || { fail "targets other than the differential test failed:"; echo "$others" | sed 's/^/    /'; }
  echo "$failed" | grep -q "^$differential" && echo "ok: positive control: the differential test failed to build without moc, as it must" || fail "the differential test built without moc: the trap is not working"
  take_record
  stray="$(echo "$recorded" | grep -v "$differential" | grep -v '^$' || true)"
  [ -z "$stray" ] || { fail "moc was called by something other than the differential test:"; echo "$stray" | sed 's/^/    /'; }
  ctest --test-dir "$work/lib" -j2 -E differential > "$work/ctest.log" 2>&1 || fail "ctest failed on the targets that built"
  tail -4 "$work/ctest.log" | sed 's/^/    /'
fi

echo "=== 5. generated moc files in the build trees"
n="$(find "$work" \( -name 'moc_*.cpp' -o -name 'mocs_compilation*.cpp' -o -name 'moc_predefs.h' \) | grep -v "$differential" | wc -l)"
[ "$n" -eq 0 ] && echo "ok: none" || fail "$n generated moc files outside the differential target"

echo
[ "$status" -eq 0 ] && echo "PASS: building reflect-moc code needs no moc" || echo "FAIL: see the lines above"
exit "$status"
