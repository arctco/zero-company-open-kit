#!/usr/bin/env bash
# Set up a Windows cross-compiler for the native module, without root.
#
# The native half is a Windows DLL and this is a Linux machine, but nothing here
# needs a Windows box or a package manager. Verified working: a PE32+ x86-64 DLL
# with correct exports, built and linked entirely on Linux.
#
#   clang, clang-cl, lld-link   already present (LLVM)
#   cargo                       already present, used to install xwin
#   xwin                        fetches the MSVC CRT and Windows SDK to $HOME
#
# Roughly 630 MB of SDK lands in the output directory. Nothing is installed
# system-wide and no sudo is used.
set -euo pipefail

SDK="${OPENKIT_WINSDK:-$HOME/.local/share/openkit-winsdk}"

command -v cargo >/dev/null || { echo "cargo not found" >&2; exit 1; }
command -v clang-cl >/dev/null || { echo "clang-cl not found (install LLVM)" >&2; exit 1; }
command -v lld-link >/dev/null || { echo "lld-link not found (install LLVM)" >&2; exit 1; }

if ! command -v "$HOME/.local/bin/xwin" >/dev/null 2>&1; then
    echo "==> installing xwin"
    cargo install xwin --root "$HOME/.local"
fi

if [ ! -d "$SDK/crt/include" ]; then
    echo "==> downloading the MSVC CRT and Windows SDK to $SDK"
    # --cache-dir defaults to ./.xwin-cache, which drops a gigabyte of download
    # cache into whatever directory this was run from. Keep it beside the SDK.
    "$HOME/.local/bin/xwin" --accept-license --arch x86_64 \
        --cache-dir "$SDK/.cache" splat --output "$SDK"
fi

# UE4SS links patternsleuth, which is Rust, so the build needs a Rust standard
# library for the Windows target. A distro rustc ships only the host target and
# fails deep in the dependency graph with E0463, so install a self-contained
# toolchain that does not shadow the system one.
RUST="${OPENKIT_RUST:-$HOME/.local/share/openkit-rust}"
export CARGO_HOME="$RUST/cargo" RUSTUP_HOME="$RUST/rustup"

if [ ! -x "$CARGO_HOME/bin/rustup" ]; then
    echo "==> installing an isolated Rust toolchain to $RUST"
    mkdir -p "$CARGO_HOME" "$RUSTUP_HOME"
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
        | sh -s -- -y --no-modify-path --profile minimal --default-toolchain stable
fi

if ! "$CARGO_HOME/bin/rustup" target list --installed | grep -qx x86_64-pc-windows-msvc; then
    echo "==> adding the Rust Windows target"
    "$CARGO_HOME/bin/rustup" target add x86_64-pc-windows-msvc
fi

cat <<INFO

Toolchain ready. Build a UE4SS C++ mod with:

  ./scripts/build-native.sh <source-dir>

which drives CMake through scripts/windows-msvc.cmake. To compile a plain DLL by
hand instead:

  clang-cl --target=x86_64-pc-windows-msvc /LD <sources> -fuse-ld=lld \\
    /imsvc "$SDK/crt/include" \\
    /imsvc "$SDK/sdk/include/ucrt" \\
    /imsvc "$SDK/sdk/include/um" \\
    /imsvc "$SDK/sdk/include/shared" \\
    /Fe:main.dll \\
    /link /libpath:"$SDK/crt/lib/x86_64" \\
          /libpath:"$SDK/sdk/lib/ucrt/x86_64" \\
          /libpath:"$SDK/sdk/lib/um/x86_64"

Two things that are easy to get wrong:

  * -fuse-ld=lld is required. Without it clang-cl looks for link.exe and fails
    with "posix_spawn failed: No such file or directory".
  * /winsysroot does not work with an xwin tree. It expects a Visual Studio
    layout; xwin produces crt/ and sdk/, so pass the paths explicitly as above.

INFO
