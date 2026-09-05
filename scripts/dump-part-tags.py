#!/usr/bin/env python3
"""Dump the gameplay tags authored on Zero Company customization parts.

This is the tool the lock analysis in docs/INVESTIGATION.md was produced with.
It extracts named cooked assets from the installed game with retoc and reports
the `br.*` tags in each asset's name table.

    ./scripts/dump-part-tags.py CPD_TacticalSpec_Warrior CPD_TacticalSpec_Soldier
    ./scripts/dump-part-tags.py --diff CPD_TacticalSpec_Warrior CPD_TacticalSpec_Soldier

UE5 zen assets use unversioned property serialization, so the name table holds
referenced names but not the struct that encloses them. That is enough to see
*which* tags a part carries and is not enough to see what the game does with
them -- see the open question in docs/INVESTIGATION.md.
"""

import argparse
import os
import pathlib
import shutil
import string
import subprocess
import sys
import tempfile

DEFAULT_PAKS = pathlib.Path.home() / (
    ".steam/steam/steamapps/common/Star Wars Zero Company/SWZeroCompany/Content/Paks"
)
DEFAULT_RETOC = pathlib.Path.home() / ".local/bin/retoc"
MIN_RUN = 4


def printable_runs(data: bytes, minimum: int = MIN_RUN):
    """The strings(1) algorithm, so the tool has no non-Python dependency."""
    allowed = set(bytes(string.printable[:-5], "ascii"))
    run = bytearray()
    for byte in data:
        if byte in allowed:
            run.append(byte)
            continue
        if len(run) >= minimum:
            yield run.decode("ascii")
        run.clear()
    if len(run) >= minimum:
        yield run.decode("ascii")


def extract(retoc: pathlib.Path, paks: pathlib.Path, names, into: pathlib.Path):
    """Pull each named asset out of the shipped containers by name filter."""
    for name in names:
        subprocess.run(
            [str(retoc), "to-legacy", "--filter", name, "--no-shaders",
             "--version", "UE5_6", str(paks), str(into)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
        )


def tags_of(asset: pathlib.Path):
    data = asset.read_bytes()
    return sorted({s for s in printable_runs(data) if s.startswith("br.")})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("assets", nargs="+", help="asset names, without path or extension")
    parser.add_argument("--paks", type=pathlib.Path, default=DEFAULT_PAKS)
    parser.add_argument("--retoc", type=pathlib.Path, default=DEFAULT_RETOC)
    parser.add_argument("--diff", action="store_true",
                        help="with exactly two assets, report only the tags that differ")
    parser.add_argument("--keep", type=pathlib.Path,
                        help="keep the extracted .uasset files in this directory")
    args = parser.parse_args()

    if not args.retoc.exists():
        print(f"retoc not found at {args.retoc}", file=sys.stderr)
        return 2
    if not args.paks.is_dir():
        print(f"game Paks directory not found at {args.paks}", file=sys.stderr)
        return 2
    if args.diff and len(args.assets) != 2:
        print("--diff needs exactly two assets", file=sys.stderr)
        return 2

    work = pathlib.Path(tempfile.mkdtemp(prefix="open-kit-tags-"))
    try:
        extract(args.retoc, args.paks, args.assets, work)
        found = {}
        for name in args.assets:
            matches = sorted(work.rglob(f"{name}.uasset"))
            if not matches:
                print(f"{name}: not found in the shipped containers", file=sys.stderr)
                continue
            found[name] = tags_of(matches[0])

        if args.diff:
            left, right = args.assets
            if len(found) != 2:
                return 1
            only_left = [t for t in found[left] if t not in found[right]]
            only_right = [t for t in found[right] if t not in found[left]]
            print(f"only in {left}:")
            print("\n".join(f"    {t}" for t in only_left) or "    (none)")
            print(f"only in {right}:")
            print("\n".join(f"    {t}" for t in only_right) or "    (none)")
        else:
            for name, tags in found.items():
                print(f"{name}")
                print("\n".join(f"    {t}" for t in tags) or "    (no br.* tags)")

        if args.keep:
            args.keep.mkdir(parents=True, exist_ok=True)
            for asset in work.rglob("*.uasset"):
                shutil.copy2(asset, args.keep / asset.name)
        return 0 if found else 1
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
