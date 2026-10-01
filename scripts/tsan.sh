#!/bin/sh
# Build the core tests and examples with ThreadSanitizer and run them, inside
# the reflect-moc/gcc16-qt610 image (Homebrew GCC on macOS has no libtsan).
#
#   scripts/tsan.sh            # from the repository root, on the host
#
# The build tree lives in the container (/tmp/tsan), so it never mixes with
# build/host. Any TSan report fails its test (TSAN_OPTIONS=halt_on_error=1).
set -eu

IMAGE="${REFLECT_MOC_IMAGE:-reflect-moc/gcc16-qt610}"

if [ "${1:-}" != "--inside" ]; then
  root="$(cd "$(dirname "$0")/.." && pwd)"
  # Docker Desktop's VM maps with 33 bits of ASLR entropy, which TSan's shadow
  # layout rejects ("incompatible memory layout"). The default seccomp profile
  # forbids the personality(ADDR_NO_RANDOMIZE) call that `setarch -R` needs.
  exec docker run --rm --security-opt seccomp=unconfined -v "$root":/src:ro -w /src "$IMAGE" \
    scripts/tsan.sh --inside
fi

export LC_ALL=C.UTF-8
cmake -S . -B /tmp/tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ \
  -DREFLECT_MOC_TSAN=ON -DRQT_BUILD_QT=OFF
cmake --build /tmp/tsan
setarch "$(uname -m)" -R ctest --test-dir /tmp/tsan --output-on-failure
