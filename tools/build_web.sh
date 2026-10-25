#!/bin/sh
# PicoPods Web: builds the pods for the browser and assembles the site.
#
#   PICO_SDK_PATH=/path/to/pico-sdk tools/build_web.sh [out-dir]
#
# Needs the Emscripten SDK on PATH (source emsdk_env.sh; tested with 3.1.74)
# and PICO_SDK_PATH for its mbedTLS sources. Output (default dist/web):
# index.html, app.js, keyboard.js, pods.json and one <pod>.pod.js per pod.
# Serve that folder over HTTP (python3 -m http.server -d dist/web) and open it.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$root/dist/web}
build=${WEB_BUILD_DIR:-$root/build-web}
pods="podboy podwriter podfiles pd_demo"   # the template and the USB probe are no use in a browser

command -v emcmake >/dev/null || { echo "emcmake not found: source emsdk_env.sh first"; exit 1; }
: "${PICO_SDK_PATH:?set PICO_SDK_PATH (the browser build uses its mbedTLS)}"

emcmake cmake -S "$root" -B "$build" -DPD_TERMINAL=host -DCMAKE_BUILD_TYPE=Release \
    -DPICO_SDK_PATH="$PICO_SDK_PATH" >/dev/null
cmake --build "$build" -j "$(nproc 2>/dev/null || echo 4)" --target $pods

rm -rf "$out"
mkdir -p "$out"
cp "$root"/web/index.html "$root"/web/app.js "$root"/web/keyboard.js "$out"/
for p in $pods; do
    cp "$(find "$build" -name "$p.pod.js" -not -path '*/CMakeFiles/*' | head -n 1)" "$out"/
done
# pods.json: name, title, version (from picopods_add_pod) and file size.
python3 - "$build" "$out" $pods <<'PY'
import json, os, sys
build, out, names = sys.argv[1], sys.argv[2], sys.argv[3:]
pods = []
for n in names:
    meta = {"name": n, "title": n, "version": "?"}
    for dirpath, _, files in os.walk(build):
        if n + ".pod.json" in files:
            meta.update(json.load(open(os.path.join(dirpath, n + ".pod.json"))))
            break
    meta["size"] = os.path.getsize(os.path.join(out, n + ".pod.js"))
    pods.append(meta)
json.dump({"pods": pods}, open(os.path.join(out, "pods.json"), "w"), indent=1)
PY
echo "PicoPods Web: $out"
ls -l "$out"
