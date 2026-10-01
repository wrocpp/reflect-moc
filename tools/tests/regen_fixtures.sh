#!/bin/sh
# Regenerate the moc JSON each fixture's tests replay (fixtures/<case>/moc),
# so the unit tests run anywhere, without Qt. Runs in the image, where moc is:
#
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 tools/tests/regen_fixtures.sh
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
tmp="$(mktemp -d)"
for case_dir in "$here"/fixtures/*/; do
  rm -rf "$case_dir/moc"
  "$here/../rqt-migrate" "$case_dir/src" --output "$tmp/out" --reflect-moc-include "$tmp/include" \
    --save-json "$case_dir/moc" --diff /dev/null
done
rm -rf "$tmp"
