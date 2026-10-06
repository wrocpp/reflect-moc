#!/bin/sh
# Build one variant of the Qt example ports, run each harness and compare its
# output with the recorded baseline. Runs inside the reflect-moc/gcc16-qt610
# image:
#
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 \
#     examples/qt/check.sh [--record] original|ported [example...]
#
# --record writes the harness output as <example>/expected_output.txt (use it
# on the original variant only). A ported build fails if its log shows any
# moc invocation.
set -u
export LC_ALL=C.UTF-8
record=0
if [ "${1:-}" = "--record" ]; then record=1; shift; fi
variant="${1:?usage: check.sh [--record] original|ported [example...]}"
shift
here="$(cd "$(dirname "$0")" && pwd)"
[ $# -gt 0 ] || set -- mandelbrot birthdayparty sliders queuedcustomtype
out="${RQT_EXAMPLES_BUILD:-/tmp/rqt-examples}"
warning_flags="-Wall -Wextra"
harness_timeout=120
smoke_seconds=2
timed_out=124
status=0
mkdir -p "$out"

harness_of() {
  case "$1" in
    mandelbrot) echo mandelbrot_harness ;;
    birthdayparty) echo valuesource_harness ;;
    sliders) echo sliders_harness ;;
    queuedcustomtype) echo queuedcustomtype_harness ;;
  esac
}
# The mandelbrot kernel (a1*a1 - b1*b1 + ax) is contracted into fused
# multiply-adds on aarch64 by default and not on x86-64, which moves the
# escape-time of chaotic pixels and so the image checksums. Contraction off
# makes the output the same on both.
cxx_flags_of() {
  case "$1" in
    mandelbrot) echo "$warning_flags -ffp-contract=off" ;;
    *) echo "$warning_flags" ;;
  esac
}
app_of() {
  case "$1" in
    birthdayparty) echo valuesource ;;
    *) echo "$1" ;;
  esac
}

for name in "$@"; do
  src="$here/$name/$variant"
  bin="$out/$name-$variant"
  log="$bin.build.log"
  echo "=== $name ($variant)"
  rm -rf "$bin"
  start=$(date +%s)
  if ! { cmake -S "$src" -B "$bin" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="$(cxx_flags_of "$name")" &&
         cmake --build "$bin" --verbose; } > "$log" 2>&1; then
    echo "BUILD FAILED (log: $log)"
    grep -E 'error|Error' "$log" | head -40
    status=1
    continue
  fi
  echo "build: $(( $(date +%s) - start ))s"
  warnings=$(grep -c 'warning:' "$log")
  echo "warnings in build log: $warnings"
  [ "$variant" = ported ] && [ "$warnings" -ne 0 ] && { echo "FAIL: the ported build is not warning-free"; status=1; }
  if grep -qE 'libexec/moc|AutoMoc|Automatic MOC|mocs_compilation|moc_[A-Za-z0-9_]+\.cpp' "$log"; then
    mocs=$(grep -cE 'libexec/moc|AutoMoc|Automatic MOC|mocs_compilation|moc_[A-Za-z0-9_]+\.cpp' "$log")
    echo "moc lines in build log: $mocs"
    [ "$variant" = ported ] && { echo "FAIL: the ported build ran moc"; status=1; }
  else
    echo "moc lines in build log: 0"
  fi

  harness="$bin/$(harness_of "$name")"
  echo "harness size: $(wc -c < "$harness") bytes"
  actual="$bin.output.txt"
  timeout "$harness_timeout" "$harness" > "$actual" 2>&1
  rc=$?
  echo "harness exit: $rc"
  [ $rc -eq 0 ] || status=1
  expected="$here/$name/expected_output.txt"
  if [ $record -eq 1 ]; then
    cp "$actual" "$expected"
    echo "recorded $(wc -l < "$expected") lines"
  elif diff -u "$expected" "$actual"; then
    echo "output: identical to the baseline ($(wc -l < "$expected") lines)"
  else
    echo "FAIL: output differs from the baseline"
    status=1
  fi

  timeout "$smoke_seconds" "$bin/$(app_of "$name")" > "$bin.app.txt" 2>&1
  rc=$?
  if [ $rc -eq $timed_out ]; then
    echo "app: still running after ${smoke_seconds}s, stopped"
  else
    echo "FAIL: app exited with $rc"; head -5 "$bin.app.txt"; status=1
  fi
done
exit $status
