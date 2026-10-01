#!/bin/sh
# Proof (a): the ported builds never run moc. Rebuilds each ported example from
# scratch with the full command lines and fails if the log shows moc, an
# AUTOMOC step, mocs_compilation.cpp or a moc_*.cpp file, or if a generated
# moc file exists in the build tree. Runs in the reflect-moc/gcc16-qt610 image:
#
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 examples/qt/check_no_moc.sh [example...]
set -u
here="$(cd "$(dirname "$0")" && pwd)"
[ $# -gt 0 ] || set -- mandelbrot birthdayparty sliders queuedcustomtype
out="${RQT_EXAMPLES_BUILD:-/tmp/rqt-ported-check}"
# libexec/moc or bin/moc as a command, an AUTOMOC step, or the files moc writes.
pattern='(libexec|bin)/moc( |$)|(^|[ ;&])moc( |$)|AutoMoc|Automatic MOC|mocs_compilation|moc_[A-Za-z0-9_]+\.cpp'
# RQT_VARIANT=original is the positive control: the detector must fire on the moc build.
variant="${RQT_VARIANT:-ported}"
status=0
mkdir -p "$out"
for name in "$@"; do
  bin="$out/$name"
  log="$bin.build.log"
  rm -rf "$bin"
  if ! { cmake -S "$here/$name/$variant" -B "$bin" -G Ninja -DCMAKE_BUILD_TYPE=Release &&
         cmake --build "$bin" --verbose; } > "$log" 2>&1; then
    echo "FAIL $name: the build failed (log: $log)"
    status=1
    continue
  fi
  hits=$(grep -cE "$pattern" "$log")
  files=$(find "$bin" \( -name 'moc_*.cpp' -o -name 'mocs_compilation*.cpp' -o -name 'moc_predefs.h' \) | wc -l)
  if [ "$hits" -ne 0 ] || [ "$files" -ne 0 ]; then
    echo "FAIL $name: $hits moc lines in the log, $files generated moc files"
    grep -E "$pattern" "$log" | head -5
    status=1
  else
    echo "ok $name: no moc invocation ($(wc -l < "$log") log lines checked)"
  fi
done
exit $status
