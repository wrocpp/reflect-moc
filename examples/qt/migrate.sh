#!/bin/sh
# Regenerate every <example>/ported tree, migration.diff and unmigrated.md
# from <example>/original with tools/rqt-migrate. Runs in the image, where
# moc is:
#
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 examples/qt/migrate.sh
#
# RQT_MIGRATE_FLAGS picks the target style (default: the Qt-like one).
# The annotation/mixin style is --style annotations.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
flags="${RQT_MIGRATE_FLAGS---style qtlike --signals members --macros rqt}"
[ $# -gt 0 ] || set -- mandelbrot birthdayparty sliders queuedcustomtype
for name in "$@"; do
  # shellcheck disable=SC2086
  "$repo/tools/rqt-migrate" "$here/$name/original" $flags \
    --output "$here/$name/ported" \
    --reflect-moc-include "$repo/include" \
    --title "$name" \
    --diff "$here/$name/migration.diff" \
    --report "$here/$name/unmigrated.md"
done
