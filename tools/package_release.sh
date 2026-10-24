#!/bin/sh
# Builds the three pods for the PicoCalc + Pico 2 W and names the firmware
# files for a release:  <pod>_<version>.uf2
#
#   tools/package_release.sh <version> [outdir]
#
# A leading "v" is dropped from the version (tag v1.2.0 -> podboy_1.2.0.uf2).
# Needs PICO_SDK_PATH. Files go to outdir (default dist/).
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
[ $# -ge 1 ] || { echo "usage: $0 <version> [outdir]" >&2; exit 2; }
version=${1#v}
out=${2:-$root/dist}
board=${PD_BOARD:-pico2_w}
build=${RELEASE_BUILD_DIR:-$root/build-release}
pods="podboy podwriter podfiles"

case $version in
    ''|*/*|*' '*) echo "bad version: '$1'" >&2; exit 2 ;;
esac

cmake -S "$root" -B "$build" -DPD_BOARD="$board" >/dev/null
# shellcheck disable=SC2086
cmake --build "$build" -j "$(nproc 2>/dev/null || echo 4)" --target $pods

mkdir -p "$out"
for p in $pods; do
    cp "$build/pods/$p/$p.uf2" "$out/${p}_${version}.uf2"
done
(cd "$out" && sha256sum ./*_"$version".uf2 | sed 's| \./| |' > "SHA256SUMS_$version.txt" && ls -l)
