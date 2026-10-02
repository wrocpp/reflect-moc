#!/bin/sh
# ThreadSanitizer run of the threaded Qt-layer tests, inside the reflect-moc/gcc16-qt610 image.
#
#   scripts/tsan-qt.sh         # from the repository root, on the host
#
# Prints "N tests, R reports" and exits non-zero if a test fails or TSan reports a race.
#
# Qt itself is not built with TSan. Qt hands a queued signal's arguments to the receiving thread through a
# futex-based mutex that TSan cannot see, so without help it reports every queued argument as a race between
# `operator new` in libQt6Core and the slot that reads it, in tests that have nothing to do with static signals
# (capability_queued_across_qthread fails the same way). TSAN_OPTIONS=ignore_noninstrumented_modules=1 drops the
# accesses made by the uninstrumented library; accesses in this library's own code and in the tests stay checked.
# A control program with a real race must still be reported, or the run fails.
#
# Only tests whose cross-thread hand-off uses atomics are run. capability_custom_metatype_queued reads plain
# members after a QSemaphore::acquire, a synchronization TSan cannot see in the uninstrumented Qt, so it reports
# 8 races that are not races; it needs a TSan build of Qt.
set -eu

IMAGE="${REFLECT_MOC_IMAGE:-reflect-moc/gcc16-qt610}"
TESTS="capability_static_signal_queued capability_queued_across_qthread capability_signal_to_signal_connect"

if [ "${1:-}" != "--inside" ]; then
  root="$(cd "$(dirname "$0")/.." && pwd)"
  # See scripts/tsan.sh: Docker Desktop's ASLR entropy needs seccomp=unconfined for `setarch -R`.
  exec docker run --rm --security-opt seccomp=unconfined -v "$root":/src:ro -w /src "$IMAGE" \
    scripts/tsan-qt.sh --inside
fi

export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen
export TSAN_OPTIONS="halt_on_error=0:ignore_noninstrumented_modules=1"
build=/tmp/tsan-qt
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ \
  -DREFLECT_MOC_TSAN=ON -DRQT_BUILD_QT=ON -DREFLECT_MOC_BUILD_EXAMPLES=OFF -DCMAKE_PREFIX_PATH=/opt/qt > "$build.cfg" 2>&1
# shellcheck disable=SC2086
cmake --build "$build" --target $TESTS

# the control: a race the run must catch
cat > "$build/control.cpp" <<'CPP'
#include <thread>
int main() {
  int counter = 0;
  std::thread a([&] { for (int i = 0; i < 100000; ++i) ++counter; });
  std::thread b([&] { for (int i = 0; i < 100000; ++i) ++counter; });
  a.join();
  b.join();
  return counter == 0;
}
CPP
g++ -std=c++26 -fsanitize=thread -g -O1 "$build/control.cpp" -o "$build/control" -pthread
control="$(setarch "$(uname -m)" -R "$build/control" 2>&1 | grep -c 'WARNING: ThreadSanitizer' || true)"
echo "control (a real race): $control report(s)"
[ "$control" -gt 0 ] || { echo "FAIL: the control race was not reported"; exit 1; }

tests=0 reports=0 failed=0
for t in $TESTS; do
  tests=$((tests + 1))
  rc=0
  setarch "$(uname -m)" -R "$build/tests/qt/$t" > "$build/$t.log" 2>&1 || rc=$?
  r="$(grep -c 'WARNING: ThreadSanitizer' "$build/$t.log" || true)"
  reports=$((reports + r))
  [ "$rc" -eq 0 ] || failed=$((failed + 1))
  echo "$t: exit $rc, $r report(s)"
done
echo "$tests tests, $reports reports, $failed failed"
[ "$reports" -eq 0 ] && [ "$failed" -eq 0 ]
