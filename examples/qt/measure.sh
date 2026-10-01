#!/bin/sh
# Build time and binary size, moc build (original/) against reflect-moc build
# (ported/). Each variant is built from scratch RUNS times (configure + build,
# wall clock) and the median and spread are printed with the load average at
# the start and end of each run. Runs in the reflect-moc/gcc16-qt610 image:
#
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 examples/qt/measure.sh [example...]
#
# Knobs: RQT_RUNS (default 3), RQT_JOBS (default 2), RQT_EXAMPLES_BUILD.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
[ $# -gt 0 ] || set -- mandelbrot birthdayparty sliders queuedcustomtype
runs="${RQT_RUNS:-3}"
jobs="${RQT_JOBS:-2}"
out="${RQT_EXAMPLES_BUILD:-/tmp/rqt-measure}"
mkdir -p "$out"

app_of() { case "$1" in birthdayparty) echo valuesource ;; *) echo "$1" ;; esac; }
load() { cut -d' ' -f1 /proc/loadavg; }

echo "# cores: $(nproc), jobs: $jobs, runs: $runs, load at start: $(load)"
for name in "$@"; do
  for variant in original ported; do
    src="$here/$name/$variant"
    bin="$out/$name-$variant"
    times=""
    for run in $(seq "$runs"); do
      rm -rf "$bin"
      l0=$(load)
      start=$(date +%s.%N)
      cmake -S "$src" -B "$bin" -G Ninja -DCMAKE_BUILD_TYPE=Release > "$bin.log" 2>&1 &&
        cmake --build "$bin" -j "$jobs" >> "$bin.log" 2>&1 || { echo "$name $variant: build failed"; break; }
      end=$(date +%s.%N)
      t=$(echo "$end $start" | awk '{printf "%.1f", $1 - $2}')
      times="$times $t"
      echo "# $name $variant run $run: ${t}s (load $l0 -> $(load))"
    done
    [ -n "$times" ] || continue
    sorted=$(printf '%s\n' $times | sort -n)
    median=$(echo "$sorted" | sed -n "$(( (runs + 1) / 2 ))p")
    min=$(echo "$sorted" | head -1)
    max=$(echo "$sorted" | tail -1)
    app="$bin/$(app_of "$name")"
    size=$(wc -c < "$app")
    strip -o "$bin/app.stripped" "$app"
    stripped=$(wc -c < "$bin/app.stripped")
    mocobj=$(find "$bin" -name 'mocs_compilation.cpp.o' -path "*/$(app_of "$name").dir/*" -exec wc -c {} + | awk '{s += $1} END {print s + 0}')
    echo "RESULT $name $variant median=$median min=$min max=$max app_bytes=$size app_stripped_bytes=$stripped moc_object_bytes=$mocobj"
  done
done
echo "# load at end: $(load)"
