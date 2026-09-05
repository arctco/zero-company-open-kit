#!/usr/bin/env python3
"""Validate the FOMOD installer before it ships.

A broken FOMOD fails at the user's end, silently and after the download, so
this runs in the release build and fails it.

    ./scripts/check-fomod.py                      # structure only
    ./scripts/check-fomod.py --stage dist/fomod   # also check every source path

Checks:

1. Both XML files are well-formed.
2. Only elements in the Vortex / MO2 / ZCOM Mod Manager intersection are used.
   Anything outside it installs correctly in one manager and not another.
3. Every flag read by a flagDependency is written by some conditionFlags, and
   every flag written is read. A typo in either direction silently produces a
   step that never shows or files that never install.
4. Every group type is one the three managers agree on.
5. Source paths use backslashes (the format's convention) and, with --stage,
   actually exist in the staged archive.
"""

import argparse
import pathlib
import sys
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parent.parent
FOMOD = ROOT / "packaging" / "fomod"

# The intersection all three managers implement. See the comment at the top of
# ModuleConfig.xml for why the omissions are omissions and not oversights.
ALLOWED = {
    "config", "moduleName", "moduleImage",
    "installSteps", "installStep", "visible",
    "optionalFileGroups", "group", "plugins", "plugin",
    "description", "image", "conditionFlags", "flag",
    "typeDescriptor", "type", "dependencyType", "defaultType",
    "requiredInstallFiles", "conditionalFileInstalls",
    "patterns", "pattern", "dependencies", "flagDependency",
    "files", "file", "folder",
}
FORBIDDEN = {
    "moduleDependencies": "the ZCOM Mod Manager does not parse it",
    "fileDependency": "Vortex's file-state handling is unreliable",
    "gameDependency": "not parsed by the ZCOM Mod Manager",
}
GROUP_TYPES = {
    "SelectExactlyOne", "SelectAtMostOne", "SelectAtLeastOne",
    "SelectAny", "SelectAll",
}
PLUGIN_TYPES = {"Required", "Recommended", "Optional", "CouldBeUsable", "NotUsable"}


def fail(problems, message):
    problems.append(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", type=pathlib.Path,
                        help="staged archive root, to verify source paths exist")
    args = parser.parse_args()

    problems = []
    config_path = FOMOD / "ModuleConfig.xml"
    info_path = FOMOD / "info.xml"

    for path in (config_path, info_path):
        if not path.exists():
            fail(problems, f"missing {path.relative_to(ROOT)}")
    if problems:
        print("\n".join(problems), file=sys.stderr)
        return 1

    try:
        ET.parse(info_path)
        root = ET.parse(config_path).getroot()
    except ET.ParseError as exc:
        print(f"malformed XML: {exc}", file=sys.stderr)
        return 1

    set_flags, read_flags, sources = set(), set(), []

    for element in root.iter():
        tag = element.tag
        if tag in FORBIDDEN:
            fail(problems, f"<{tag}> is not portable: {FORBIDDEN[tag]}")
        elif tag not in ALLOWED:
            fail(problems, f"<{tag}> is outside the agreed element subset")

        if tag == "group":
            kind = element.get("type")
            if kind not in GROUP_TYPES:
                fail(problems, f"group '{element.get('name')}' has unsupported type {kind!r}")
        elif tag == "type" or tag == "defaultType":
            kind = element.get("name")
            if kind not in PLUGIN_TYPES:
                fail(problems, f"unsupported plugin type {kind!r}")
        elif tag == "flag":
            set_flags.add(element.get("name"))
        elif tag == "flagDependency":
            read_flags.add(element.get("flag"))
        elif tag in ("file", "folder"):
            source = element.get("source")
            if not source:
                fail(problems, f"<{tag}> with no source")
                continue
            if "/" in source:
                fail(problems, f"source {source!r} uses '/', FOMOD paths use '\\'")
            sources.append(source)
            if not element.get("destination"):
                fail(problems, f"<{tag} source={source!r}> has no destination")

    for flag in sorted(read_flags - set_flags):
        fail(problems, f"flag {flag!r} is tested but never set - that step or file never fires")
    for flag in sorted(set_flags - read_flags):
        fail(problems, f"flag {flag!r} is set but never tested - dead option")

    if args.stage:
        for source in sources:
            target = args.stage / pathlib.PurePosixPath(source.replace("\\", "/"))
            if not target.exists():
                fail(problems, f"source {source!r} does not exist in {args.stage}")

    if problems:
        print(f"FOMOD invalid ({len(problems)} problem(s)):", file=sys.stderr)
        print("\n".join(f"  - {p}" for p in problems), file=sys.stderr)
        return 1

    print(f"FOMOD valid: {len(set_flags)} flags, {len(sources)} install sources")
    if not args.stage:
        print("source paths not checked (pass --stage to verify against a staged archive)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
