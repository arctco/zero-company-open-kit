#!/usr/bin/env python3
"""Build the manual, drag-and-drop archive: the final layout, already resolved.

    ./scripts/build-manual.py                 # the recommended answers
    ./scripts/build-manual.py --all           # every switch on
    ./scripts/build-manual.py --core warrior --swap

A FOMOD stores its files under option folders and relies on the installer to
place them, so extracting one by hand produces the wrong layout. This archive is
the other half of that: one resolved configuration, laid out game-relative, that
a player drags over their game directory.

It is built by resolving packaging/fomod/ModuleConfig.xml rather than by
restating the layout here. The installer already knows every source, every
destination and every condition; duplicating that in a second script is how the
two archives drift apart until one of them installs to the wrong place. So this
reads the answers a user would give, evaluates the installer's own flag
conditions, and copies what the installer would have copied.

The payload comes from the FOMOD stage in dist/fomod, so the module, the option
fragments and the containers are byte-identical between the two archives. Run
scripts/build-fomod.py first.
"""

import argparse
import pathlib
import shutil
import sys
import xml.etree.ElementTree as ET
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONFIG = ROOT / "packaging" / "fomod" / "ModuleConfig.xml"
DIST = ROOT / "dist"
STAGE = DIST / "fomod"

# Reproducible archives, matching build-fomod.py: a fixed timestamp so two
# builds of the same tree are byte-identical and a release can be diffed
# against its predecessor.
STAMP = (2026, 1, 1, 0, 0, 0)

# The three feature flags from the first installer step. The FOMOD marks Armory
# "Optional" because a wizard should not tick things for you; a manual archive
# has no wizard and ships the whole mod, so all three are on unless asked
# otherwise. Everything below the features follows the installer's own
# Recommended answers.
FEATURES = ("core", "wardrobe", "armory")


def fail(message: str) -> None:
    sys.exit(f"error: {message}")


def tag(element) -> str:
    """Local name, so the parser does not care whether the XML is namespaced."""
    return element.tag.split("}")[-1]


def find_all(node, name: str):
    return [e for e in node.iter() if tag(e) == name]


def holds(node, flags: dict) -> bool:
    """Evaluate a <dependencies>/<visible> block against the current flags."""
    deps = [d for d in node if tag(d) == "flagDependency"]
    if not deps:
        return True
    results = [flags.get(d.get("flag")) == d.get("value") for d in deps]
    if (node.get("operator") or "And") == "Or":
        return any(results)
    return all(results)


def resolve_flags(root, args) -> dict:
    """Replay the installer: set feature flags, then take each visible step's
    answers. Recommended plugins are the default; --all takes everything a step
    allows, except where the step permits exactly one answer."""
    flags = {name: "on" for name in FEATURES if name in args.features}

    for step in find_all(root, "installStep"):
        visible = [v for v in step if tag(v) == "visible"]
        if visible and not all(holds(v, flags) for v in visible):
            continue
        for group in find_all(step, "group"):
            exclusive = (group.get("type") or "") in (
                "SelectExactlyOne", "SelectAtMostOne")
            for plugin in find_all(group, "plugin"):
                kinds = {t.get("name") for t in find_all(plugin, "type")}
                recommended = "Recommended" in kinds
                take = recommended or (args.all_options and not exclusive)
                if not take:
                    continue
                for flag in find_all(plugin, "flag"):
                    # The feature flags come from --features and are what step
                    # visibility is keyed on. Letting the Features step's own
                    # Recommended plugins write them back would turn a feature
                    # on again after it had been left out.
                    if flag.get("name") in FEATURES:
                        continue
                    flags[flag.get("name")] = (flag.text or "").strip()

    # Explicit overrides for the two exclusive choices a player is most likely
    # to want differently, applied after the replay so they win. Only for flags
    # the replay actually set: the steps that ask these questions are visible
    # only when Core is on, and forcing them anyway would install the Core
    # containers into a wardrobe-only archive.
    if "kits" in flags:
        flags["kits"] = args.core
    if "swap" in flags:
        flags["swap"] = "on" if args.swap else "off"
    return flags


def resolve_files(root, flags: dict) -> list:
    """Every (source, destination, is_folder) the installer would install."""
    out = []
    for block in ("requiredInstallFiles", "conditionalFileInstalls"):
        for section in find_all(root, block):
            if block == "requiredInstallFiles":
                for e in section.iter():
                    if tag(e) in ("file", "folder"):
                        out.append((e.get("source"), e.get("destination"),
                                    tag(e) == "folder"))
                continue
            for pattern in find_all(section, "pattern"):
                deps = [d for d in pattern if tag(d) == "dependencies"]
                if deps and not all(holds(d, flags) for d in deps):
                    continue
                for e in pattern.iter():
                    if tag(e) in ("file", "folder"):
                        out.append((e.get("source"), e.get("destination"),
                                    tag(e) == "folder"))
    return out


def win(path: str) -> pathlib.PurePosixPath:
    return pathlib.PurePosixPath(path.replace("\\", "/"))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build the manual drag-and-drop archive.")
    parser.add_argument("--all", dest="all_options", action="store_true",
                        help="turn on every optional switch, not just the "
                             "installer's recommended ones")
    parser.add_argument("--core", choices=("both", "padawan", "warrior"),
                        default="both", help="which kits to offer (default: both)")
    parser.add_argument("--swap", action="store_true",
                        help="let hero kits be swapped off again")
    parser.add_argument("--features", default=",".join(FEATURES),
                        help="comma-separated subset of core,wardrobe,armory")
    args = parser.parse_args()
    args.features = {f.strip() for f in args.features.split(",") if f.strip()}
    unknown = args.features - set(FEATURES)
    if unknown:
        fail(f"unknown feature(s): {', '.join(sorted(unknown))}")
    if not args.features:
        fail("at least one feature is required")

    version = (ROOT / "VERSION").read_text().strip()

    if not STAGE.is_dir():
        fail("dist/fomod is missing -- run:\n  ./scripts/build-fomod.py")

    root = ET.parse(CONFIG).getroot()
    flags = resolve_flags(root, args)
    installs = resolve_files(root, flags)
    if not installs:
        fail("the chosen answers install nothing")

    build = DIST / "manual"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir(parents=True)

    options: list[str] = []
    containers = 0
    for source, destination, is_folder in installs:
        src = STAGE / win(source)
        if not src.exists():
            fail(f"the installer names {source}, which is not in dist/fomod -- "
                 f"rebuild it with ./scripts/build-fomod.py")
        dest = win(destination)
        # Nothing may land outside the game directory the player drags this over.
        if dest.parts[0] != "SWZeroCompany":
            fail(f"{source} installs to {destination}, outside SWZeroCompany")
        target = build / dest
        if is_folder:
            target.mkdir(parents=True, exist_ok=True)
            for child in sorted(src.rglob("*")):
                if child.is_file():
                    out = target / child.relative_to(src)
                    out.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(child, out)
                    if out.suffix in {".pak", ".ucas", ".utoc"}:
                        containers += 1
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, target)
            if target.suffix == ".ini" and target.parent.name == "options":
                options.append(target.stem)

    module = build / "SWZeroCompany/Binaries/Win64/ue4ss/Mods/ZCOMOpenKit"
    dll = module / "dlls" / "main.dll"
    if not dll.is_file():
        fail("the resolved layout has no dlls/main.dll -- the module is the mod")
    if not (module / "enabled.txt").is_file():
        fail("the resolved layout has no enabled.txt -- UE4SS would not load it")
    if not options:
        fail("the resolved layout has no option fragments, so nothing is on")

    # The archive is only honest if the DLL in it knows the switches beside it.
    # Same check build-fomod.py makes, against the binary rather than the source.
    blob = dll.read_bytes()
    unknown_options = [n for n in options
                       if f"{n}.ini".encode("utf-16-le") not in blob]
    if unknown_options:
        fail("the built DLL does not know these options, so shipping them would "
             "do nothing:\n  " + "\n  ".join(sorted(unknown_options)))

    every = sorted(p.stem for p in (STAGE / "options").glob("*.ini"))
    off = [n for n in every if n not in options]
    (build / "README.txt").write_text(README.format(
        version=version,
        on="\n".join(f"  {n}" for n in sorted(options)),
        off=("\n".join(f"  {n}" for n in off) if off else "  (none -- every switch is on)"),
    ))

    out = DIST / f"ZCOM-Open-Kit-v{version}-Manual.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for path in sorted(build.rglob("*")):
            if path.is_file():
                info = zipfile.ZipInfo(str(path.relative_to(build)), date_time=STAMP)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                z.writestr(info, path.read_bytes())

    files = len(zipfile.ZipFile(out).namelist())
    kits = flags.get("kits", "-- (Core is off)")
    print(f"kits={kits} swap={flags.get('swap', '--')} "
          f"features={','.join(sorted(args.features))}")
    print(f"{len(options)} options on, {len(off)} off, {containers} container files")
    print(f"\n{out.relative_to(ROOT)}  ({out.stat().st_size:,} bytes, {files} files)")
    return 0


README = """Open Kit {version} -- manual installation

Requires UE4SS for Zero Company, installed first.

1. Close the game.
2. In Steam: right-click Star Wars: Zero Company > Manage > Browse local files.
3. Drag the SWZeroCompany folder from this archive into that directory and merge.
4. Launch.

You should end up with:

  SWZeroCompany\\Binaries\\Win64\\ue4ss\\Mods\\ZCOMOpenKit\\dlls\\main.dll

DO NOT pick the Jedi specialization during character creation. A character
created that way cannot finish the tutorial and there is no way back. Recruit
first, then respec -- that path works.

Turning features on and off
---------------------------

Each file in ZCOMOpenKit\\options\\ turns one thing on. Delete a file to turn
that thing off; put it back to turn it on again. The contents are ignored, so
an empty file is enough to enable one. Changes take effect on the next launch.

On in this archive:

{on}

Available but not enabled here -- create an empty file with that name in
ZCOMOpenKit\\options\\ to turn one on:

{off}

Uninstalling
------------

Put anyone wearing modded armour or carrying modded gear back into stock
equipment and SAVE FIRST. Then close the game and delete:

  SWZeroCompany\\Binaries\\Win64\\ue4ss\\Mods\\ZCOMOpenKit
  the Open Kit pak/ucas/utoc files in SWZeroCompany\\Content\\Paks\\~mods

Do not delete the whole ue4ss folder; other mods live there.

MIT licensed. Source, documentation and issues:
https://github.com/arctco/zero-company-open-kit
"""


if __name__ == "__main__":
    raise SystemExit(main())
