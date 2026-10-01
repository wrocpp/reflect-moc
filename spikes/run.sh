#!/bin/sh
# Build and run spikes 01-05 on GCC 16.2, then compile each probe macro and
# print the first error lines. Usage: spikes/run.sh [spike-number ...]
set -u
CXX="${CXX:-/opt/homebrew/bin/g++-16}"
FLAGS="-std=c++26 -freflection -Wall -Wextra"
OUT="${TMPDIR:-/tmp}/reflect-moc-spikes"
mkdir -p "$OUT"
cd "$(dirname "$0")"

probes() {
  case "$1" in
    01) echo SPIKE_TEMPLATE_BLOCK_NAMESPACE_TARGET SPIKE_FN_BLOCK SPIKE_RETURN_TYPE SPIKE_CRTP_BASE SPIKE_IN_CLASS SPIKE_LAZY_ALIAS SPIKE_CONSTEVAL_FN ;;
    02) echo SPIKE_STRING_LITERAL ;;
    04) echo SPIKE_NO_MEMBER ;;
    05) echo SPIKE_EMIT_OUTSIDE ;;
  esac
}

status=0
for f in ${@:-01 02 03 04 05}; do
  src=$(ls "$f"-*.cpp)
  echo "=== $src"
  if $CXX $FLAGS "$src" -o "$OUT/$f" 2> "$OUT/$f.err"; then
    grep -c warning "$OUT/$f.err" | sed 's/^/warnings: /'
    "$OUT/$f"; rc=$?
    echo "exit: $rc"; [ $rc -eq 0 ] || status=1
  else
    echo "BUILD FAILED"; grep -E 'error|note: .*(exception|reason)' "$OUT/$f.err" | head -25; status=1
  fi
  for p in $(probes "$f"); do
    echo "--- probe -D$p"
    if $CXX $FLAGS -D"$p" "$src" -o "$OUT/$f-$p" 2> "$OUT/$f-$p.err"; then
      echo "probe BUILT"; "$OUT/$f-$p"; echo "probe exit: $?"
    else
      grep -E 'error' "$OUT/$f-$p.err" | head -6
    fi
  done
done
exit $status
