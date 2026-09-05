#!/usr/bin/env python3
"""Build installable test archives for the Core containers.

Core is a pak and contains no code, so it cannot report on itself. The probe is
a small UE4SS Lua mod that reads the result back out of the running game; it is
a test aid and not part of the mod.

Two archives, because there are two ways people install:

  Manual        game-relative layout, drag over the game directory
  ZCOM-Manager  Core and Probe as separately tracked components

Both carry the "both kits" container. Use scripts/build-container.sh first.
"""

import json
import pathlib
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONTAINER = ROOT / "container"
PACKAGING = ROOT / "packaging"
PROBE = ROOT / "src" / "ZCOMOpenKitProbe"
DIST = ROOT / "dist"

PAK_DIR = "SWZeroCompany/Content/Paks/~mods"
PROBE_DIR = "SWZeroCompany/Binaries/Win64/ue4ss/Mods/ZCOMOpenKitProbe"
FILES = ["pakchunk99-ZCOMOpenKit_P.pak", "pakchunk99-ZCOMOpenKit_P.ucas", "pakchunk99-ZCOMOpenKit_P.utoc"]
STAMP = (2026, 1, 1, 0, 0, 0)

README = """Open Kit - Core test build {version}
=====================================

UNTESTED. This is the first build of the Core half that has ever been put in
front of the game. Back up your save before you use it.

WHAT IT DOES
  Adds the Padawan and Warrior specializations to the focus tree, so any
  operator can take them. It changes one asset - the focus-tree picker - and
  touches no character, no faction and no part definition. No UE4SS required.

INSTALL
  1. Close the game.
  2. Copy the SWZeroCompany folder from this archive over your game directory.
     You should end up with:
       ...\\SWZeroCompany\\Content\\Paks\\~mods\\pakchunk99-ZCOMOpenKit_P.pak
  3. Launch, load a save, open a character's Personnel / focus tree screen.

WHAT TO LOOK FOR
  Padawan and Warrior should appear alongside Assault, Gunslinger, Heavy,
  Medic, Scoundrel, Scout, Sniper and Soldier.

THE PROBE (optional, needs UE4SS)
  The pak has no code and cannot log anything. The probe reads the result back
  out of the running game and writes one line to UE4SS.log. It installs nothing
  and changes nothing.

  Install it too, then after opening the focus tree, search
  SWZeroCompany\\Binaries\\Win64\\ue4ss\\UE4SS.log for ZCOM_OPEN_KIT:

    [ZCOM_OPEN_KIT] status version=... cdo=found core=applied
      specializations=10/8 (foreach) swap=false
      spec_lock_tag=br.Customization.Part.Character.Info.Name ...

  core=applied means the container mounted and the picker really has ten
  entries. core=not-applied means it did not - send that line back.

UNINSTALL
  Close the game, delete the three pakchunk99-ZCOMOpenKit_P files from ~mods,
  and the ZCOMOpenKitProbe folder if you installed it. Nothing is written to
  your save by the container itself, but if an operator is holding a Padawan or
  Warrior specialization when you remove it, put them back on a stock
  specialization and save first.
"""


def add(archive, arcname, data: bytes):
    info = zipfile.ZipInfo(arcname, STAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o644 << 16
    archive.writestr(info, data)


def add_file(archive, source: pathlib.Path, arcname: str):
    add(archive, arcname, source.read_bytes())


def version() -> str:
    return (ROOT / "VERSION").read_text(encoding="utf-8").strip()


def manifest(kind: str, release: str) -> bytes:
    data = json.loads((PACKAGING / f"zcom-mod.{kind}.json").read_text(encoding="utf-8"))
    data["version"] = release
    return (json.dumps(data, indent=2) + "\n").encode("utf-8")


def build_manual(release: str, variant: str) -> pathlib.Path:
    path = DIST / f"ZCOM-Open-Kit-v{release}-Core-{variant}-Manual.zip"
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        add(z, "README.txt", README.format(version=release).encode("utf-8"))
        for name in FILES:
            add_file(z, CONTAINER / variant / name, f"{PAK_DIR}/{name}")
        add_file(z, PROBE / "Scripts" / "main.lua", f"{PROBE_DIR}/Scripts/main.lua")
        add(z, f"{PROBE_DIR}/enabled.txt", b"")
    return path


def build_manager(release: str, variant: str) -> pathlib.Path:
    path = DIST / f"ZCOM-Open-Kit-v{release}-Core-{variant}-ZCOM-Manager.zip"
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for name in FILES:
            add_file(z, CONTAINER / variant / name, f"Core/{name}")
        add(z, "Core/zcom-mod.json", manifest("core", release))
        add_file(z, PROBE / "Scripts" / "main.lua", "Probe/ZCOMOpenKitProbe/Scripts/main.lua")
        add(z, "Probe/zcom-mod.json", manifest("probe", release))
        # No enabled.txt in the manager package: the manager owns the mods.txt
        # toggle and enabled.txt would fight a user disabling the probe from its UI.
    return path


def main() -> int:
    variant = sys.argv[1] if len(sys.argv) > 1 else "both"
    if not (CONTAINER / variant).is_dir():
        print(f"container/{variant} not built - run scripts/build-container.sh", file=sys.stderr)
        return 1
    DIST.mkdir(exist_ok=True)
    release = version()
    for build in (build_manual, build_manager):
        path = build(release, variant)
        print(f"{path.relative_to(ROOT)}  ({path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
