#!/usr/bin/env bash
# Lay a built native DLL out the way UE4SS loads it, and zip it.
#
#   ./scripts/stage-native-mod.sh ZCOMOpenKitSeamProbe
#
# UE4SS looks for ue4ss/Mods/<Name>/dlls/main.dll next to the game executable,
# with an enabled.txt marker beside it. The DLL's own filename is not the mod
# name -- it must be renamed to main.dll, which is the single most common way a
# hand-installed C++ mod ends up silently not loading.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"
NAME="${1:?usage: stage-native-mod.sh <TargetName>}"
BUILD_DIR="${OPENKIT_BUILD_DIR:-$ROOT/dist/native/$NAME}"
STAGE="$ROOT/dist/$NAME"

DLL="$(find "$BUILD_DIR" -maxdepth 2 -name "$NAME.dll" | head -1)"
[ -n "$DLL" ] || { echo "no $NAME.dll under $BUILD_DIR -- build it first" >&2; exit 1; }

MOD="$STAGE/ue4ss/Mods/$NAME"
rm -rf "$STAGE"
mkdir -p "$MOD/dlls"
cp "$DLL" "$MOD/dlls/main.dll"
: > "$MOD/enabled.txt"

if [ -f "$ROOT/src/$NAME/INSTALL.txt" ]; then
    cp "$ROOT/src/$NAME/INSTALL.txt" "$STAGE/README.txt"
fi

# python's zipfile rather than the zip binary, which is not installed here and
# is not worth requiring for one archive.
python3 - "$STAGE" "$ROOT/dist/$NAME.zip" <<'PY'
import os, sys, zipfile
stage, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk(stage):
        for name in sorted(files):
            full = os.path.join(root, name)
            z.write(full, os.path.relpath(full, stage))
PY

echo "$STAGE"
find "$STAGE" -type f | sed "s|^$STAGE|  .|"
echo "$ROOT/dist/$NAME.zip"
