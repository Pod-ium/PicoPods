#!/bin/sh
# PicoPods gates (see docs/ROADMAP.md):
#   G-host  PC build of every library, pod and example + the ctest suite
#   G-fw    firmware build (.uf2) for each board given (default pico2_w)
#
#   tools/gate.sh [host|fw|all] [board...]
#
# PICO_SDK_PATH is needed for G-fw and enables the Bluetooth host tests.
# Build trees go to build-gate/ (GATE_BUILD_DIR overrides it); logs next to them.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
what=${1:-all}
[ $# -gt 0 ] && shift
boards=${*:-pico2_w}
jobs=$(nproc 2>/dev/null || echo 4)
out=${GATE_BUILD_DIR:-$root/build-gate}
mkdir -p "$out"

# step <log> <command...>: run quietly, show the log tail if it fails.
step() {
    log=$1; shift
    if ! "$@" >"$log" 2>&1; then
        tail -n 40 "$log"
        echo "== FAILED: $* (log: $log)"
        exit 1
    fi
}

if [ "$what" = host ] || [ "$what" = all ]; then
    echo "== G-host"
    step "$out/host-configure.log" cmake -S "$root" -B "$out/host" -DPD_TERMINAL=host \
        ${PICO_SDK_PATH:+-DPICO_SDK_PATH=$PICO_SDK_PATH}
    step "$out/host-build.log" cmake --build "$out/host" -j "$jobs"
    step "$out/host-ctest.log" ctest --test-dir "$out/host" -j "$jobs" --output-on-failure
    grep "tests passed" "$out/host-ctest.log"
fi
if [ "$what" = fw ] || [ "$what" = all ]; then
    for b in $boards; do
        echo "== G-fw ($b)"
        step "$out/fw-$b-configure.log" cmake -S "$root" -B "$out/fw-$b" -DPD_BOARD="$b"
        step "$out/fw-$b-build.log" cmake --build "$out/fw-$b" -j "$jobs"
        (cd "$out/fw-$b" && find . -name '*.uf2' | sed 's|^\./||' | sort)
    done
fi
echo "== gates passed"
