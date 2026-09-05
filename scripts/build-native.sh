#!/usr/bin/env bash
# Cross-compile a UE4SS C++ mod to a Windows DLL, entirely on Linux.
#
#   ./scripts/build-native.sh                    # build Open Kit's own module
#   ./scripts/build-native.sh <source-dir>       # build any UE4SS C++ mod
#
# The second form is how the MIT baseline is built unchanged, which is the
# known-good reference this project's module is measured against:
#
#   ./scripts/build-native.sh ../lab/upstream/ZeroCompanyMandoWardrobe
#
# Run scripts/setup-native-toolchain.sh first.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"
SOURCE_DIR="$(cd "${1:-$ROOT/src/ZCOMOpenKit}" && pwd)"
BUILD_DIR="${OPENKIT_BUILD_DIR:-$ROOT/dist/native/$(basename "$SOURCE_DIR")}"
SDK="${OPENKIT_WINSDK:-$HOME/.local/share/openkit-winsdk}"
RUST="${OPENKIT_RUST:-$HOME/.local/share/openkit-rust}"
UE4SS="${RE_UE4SS_SOURCE_DIR:-$ROOT/../lab/upstream/RE-UE4SS}"

fail() { echo "$*" >&2; exit 1; }

[ -f "$SOURCE_DIR/CMakeLists.txt" ] || fail "no CMakeLists.txt in $SOURCE_DIR"
[ -d "$SDK/crt/include" ] || fail "Windows SDK missing; run scripts/setup-native-toolchain.sh"
[ -f "$UE4SS/CMakeLists.txt" ] || fail "RE-UE4SS not found at $UE4SS (set RE_UE4SS_SOURCE_DIR)"

# UE4SS links patternsleuth, which is Rust, so the build needs a Rust standard
# library for the Windows target -- a system rustc without rustup will not have
# one and fails deep inside the dependency graph with E0463.
[ -d "$UE4SS/deps/first/Unreal/include" ] \
    || fail "UE4SS's Unreal submodule is empty. It is EpicGames-membership gated
and its URL is SSH; with a GitHub account linked to Epic, fetch it with:
  git -C \"$UE4SS\" submodule sync --recursive
  git -C \"$UE4SS\" -c url.\"https://github.com/\".insteadOf=\"git@github.com:\" \\
      submodule update --init --recursive"
[ -d "$RUST/rustup/toolchains" ] || fail "Rust toolchain missing; run scripts/setup-native-toolchain.sh"
export CARGO_HOME="$RUST/cargo" RUSTUP_HOME="$RUST/rustup"
export PATH="$CARGO_HOME/bin:$PATH"
rustup target list --installed | grep -qx x86_64-pc-windows-msvc \
    || fail "Rust target x86_64-pc-windows-msvc missing; run scripts/setup-native-toolchain.sh"

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$HERE/windows-msvc.cmake" \
    -DCMAKE_BUILD_TYPE=Game__Shipping__Win64 \
    -DRE_UE4SS_SOURCE_DIR="$UE4SS"

# Build only the mod's own target, which by convention is the source directory's
# name. Building everything instead drags in two UE4SS build-time tools that
# cannot work when cross-compiling: proxy_generator, a Windows PE the build then
# tries to execute (on Linux it hangs forever with a defunct child rather than
# failing), and an .asm proxy stub needing MASM's ml64. The mod target needs
# neither.
TARGET="${OPENKIT_TARGET:-$(basename "$SOURCE_DIR")}"
# grep without -q on purpose: under `set -o pipefail`, a -q that exits on the
# first match kills ninja with SIGPIPE and the whole pipeline reports failure.
ninja -C "$BUILD_DIR" -t targets all 2>/dev/null | grep "^$TARGET\.dll:" >/dev/null \
    || fail "no CMake target named '$TARGET' in $SOURCE_DIR
This script builds the target matching the source directory's name; override it
with OPENKIT_TARGET=<name>. Do not build all targets."
cmake --build "$BUILD_DIR" --target "$TARGET" -j "$(nproc)"

DLL="$(find "$BUILD_DIR" -maxdepth 2 -name "$TARGET.dll" | head -1)"
[ -n "$DLL" ] || fail "build reported success but produced no $TARGET.dll"

echo
printf '%s\n  %s\n' "$DLL" "$(file -b "$DLL")"
# A UE4SS C++ mod is loaded through these two exports. A DLL that links but
# exports neither loads and does nothing, silently.
for symbol in start_mod uninstall_mod; do
    llvm-readobj --coff-exports "$DLL" | grep -q "Name: $symbol\$" \
        || fail "  MISSING EXPORT: $symbol"
done
echo "  exports start_mod and uninstall_mod"
