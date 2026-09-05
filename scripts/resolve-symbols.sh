#!/usr/bin/env bash
# Resolve Zero Company's own symbols from the PDB the game ships.
#
#   ./scripts/resolve-symbols.sh 'UCustomizationStatics'
#   ./scripts/resolve-symbols.sh 'CrossTraining'
#
# Prints "RVA<TAB>demangled signature" for every public symbol whose mangled
# name matches the regex. RVAs are load-address independent: add the module base
# (0x140000000 in a default load) to get a runtime address.
#
# The game installs SWZeroCompany.pdb next to the executable, so no symbol
# server, no reverse engineering and no third-party mod is involved in this.
#
# The first run dumps the public symbol table to a cache. That takes a couple of
# minutes and about 260 MB; every later run reads the cache.
set -euo pipefail

GAME="${ZCOM_GAME_DIR:-$HOME/.steam/steam/steamapps/common/Star Wars Zero Company}"
BIN="$GAME/SWZeroCompany/Binaries/Win64"
EXE="$BIN/SWZeroCompany.exe"
PDB="$BIN/SWZeroCompany.pdb"
CACHE="${ZCOM_SYMBOL_CACHE:-${TMPDIR:-/tmp}/zcom-publics.tsv}"

# Section 0001 is .text and starts at RVA 0x1000, so a public symbol's recorded
# offset plus 0x1000 is its RVA. Validated against the eight targets the MIT
# wardrobe pins by hand -- every one lands on its documented address.
readonly TEXT_BASE=4096

[ -f "$EXE" ] || { echo "game executable not found: $EXE" >&2; exit 1; }
[ -f "$PDB" ] || { echo "game PDB not found: $PDB" >&2; exit 1; }
command -v llvm-pdbutil >/dev/null || { echo "llvm-pdbutil not found (install LLVM)" >&2; exit 1; }
command -v llvm-symbolizer >/dev/null || { echo "llvm-symbolizer not found (install LLVM)" >&2; exit 1; }
# The cache parser uses gawk's three-argument match(); mawk and busybox awk
# silently produce an empty cache instead of failing, so refuse them outright.
awk --version 2>/dev/null | grep -q 'GNU Awk' || { echo "gawk required" >&2; exit 1; }
[ $# -ge 1 ] || { echo "usage: $0 <regex>" >&2; exit 1; }

if [ ! -s "$CACHE" ]; then
    echo "==> caching public symbols to $CACHE (first run only)" >&2
    llvm-pdbutil dump --publics "$PDB" \
    | awk '
        match($0, /S_PUB32 \[size = [0-9]+\] `(.*)`$/, m) { name = m[1]; next }
        name != "" && match($0, /addr = ([0-9]+):([0-9]+)/, a) {
            print a[1] "\t" a[2] "\t" name; name = ""
        }' > "$CACHE"
fi

# Resolve each candidate address through the PDB rather than demangling the
# public name, so a wrong offset shows up as a mismatched name instead of a
# plausible-looking lie.
grep -P "$1" "$CACHE" | grep -Fv 'Z_Construct' \
| awk -F'\t' -v b="$TEXT_BASE" '$1 == "0001" { printf "0x%X\n", $2 + b }' \
| sort -u \
| while read -r rva; do
    addr=$(printf '0x%X' $(( rva + 0x140000000 )))
    name=$(llvm-symbolizer --obj="$EXE" --functions=linkage --demangle "$addr" | head -1)
    case "$name" in
        '??'|'') continue ;;
    esac
    printf '%s\t%s\n' "$rva" "$name"
done
