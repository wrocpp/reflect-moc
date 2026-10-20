#!/bin/sh
# Build one variant of this example, run its harness and compare the output with the recorded baseline.
# Runs inside the reflect-moc/gcc16-qt610 image:
#
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 \
#     examples/qt/observable_container/check.sh [--record] original|ported
#
# --record writes the harness output as expected_output.txt (use it on the original variant only).
# The ported build must be warning-free and must not run moc. Its sources are written by hand: moc cannot
# read the class template, so rqt-migrate has no input to port (see README.md).
set -u
export LC_ALL=C.UTF-8
record=0
if [ "${1:-}" = "--record" ]; then record=1; shift; fi
variant="${1:?usage: check.sh [--record] original|ported}"
here="$(cd "$(dirname "$0")" && pwd)"
out="${RQT_EXAMPLES_BUILD:-/tmp/rqt-examples}/observable_container-$variant"
log="$out.build.log"
warning_flags="-Wall -Wextra"
moc_pattern='libexec/moc|AutoMoc|Automatic MOC|mocs_compilation|moc_[A-Za-z0-9_]+\.cpp'
status=0
rm -rf "$out"
mkdir -p "$(dirname "$out")"
echo "=== observable_container ($variant)"
if ! { cmake -S "$here/$variant" -B "$out" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="$warning_flags" &&
       cmake --build "$out" --verbose; } > "$log" 2>&1; then
  echo "BUILD FAILED (log: $log)"
  grep -E 'error|Error' "$log" | head -40
  exit 1
fi
warnings=$(grep -c 'warning:' "$log")
echo "warnings in build log: $warnings"
[ "$variant" = ported ] && [ "$warnings" -ne 0 ] && { echo "FAIL: the ported build is not warning-free"; status=1; }
mocs=$(grep -cE "$moc_pattern" "$log")
echo "moc lines in build log: $mocs"
[ "$variant" = ported ] && [ "$mocs" -ne 0 ] && { echo "FAIL: the ported build ran moc"; status=1; }
actual="$out.output.txt"
# stderr stays out of the baseline: QAbstractItemModelTester itself prints "Trying to construct an instance of an
# invalid type, type id: 4097" on Qt 6.10.3, with or without reflect-moc.
QT_QPA_PLATFORM=offscreen "$out/observable_container_harness" > "$actual" 2> "$out.stderr.txt"
rc=$?
echo "harness exit: $rc"
[ $rc -eq 0 ] || status=1
expected="$here/expected_output.txt"
if [ $record -eq 1 ]; then
  cp "$actual" "$expected"
  echo "recorded $(wc -l < "$expected") lines"
elif diff -u "$expected" "$actual"; then
  echo "output: identical to the baseline ($(wc -l < "$expected") lines)"
else
  echo "FAIL: output differs from the baseline"
  status=1
fi
exit $status
