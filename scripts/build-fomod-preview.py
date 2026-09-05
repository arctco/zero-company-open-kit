#!/usr/bin/env python3
"""Build a FOMOD preview archive: the real installer, inert placeholder payload.

Neither half of Open Kit is built yet, so there is nothing to install. This
packages the actual ModuleConfig.xml against stand-in files so the wizard can be
walked end to end in ZCOM Mod Manager, Vortex or MO2 -- checking that the steps
appear in order, that the flag logic hides and shows what it should, and that
every choice routes to the destination it claims.

It installs no working mod, and deliberately contains:

  * no .pak/.ucas/.utoc  - a malformed IoStore container in ~mods can stop the
                           game mounting its own paks
  * no main.dll          - nothing to load
  * no enabled.txt       - so UE4SS does not try to start an empty mod

so that deploying it and forgetting about it cannot break anything.
"""

import pathlib
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
FOMOD = ROOT / "packaging" / "fomod"
DIST = ROOT / "dist"
STAGE = DIST / "fomod-preview"

BANNER = (
    "PLACEHOLDER - Open Kit is not built yet.\n"
    "\n"
    "This file is a stand-in so the FOMOD installer can be tested. It does\n"
    "nothing. The real payload for this option will be:\n"
    "\n"
    "    {}\n"
    "\n"
    "Safe to delete.\n"
)

# What each option folder will really carry, once there is something to carry.
FOLDER_PAYLOAD = {
    "core/both": "pakchunk99-ZCOMOpenKit_P.{pak,ucas,utoc} - Padawan + Warrior pairing",
    "core/both-swap": "pakchunk99-ZCOMOpenKit_P.{pak,ucas,utoc} - both kits, swappable",
    "core/padawan": "pakchunk99-ZCOMOpenKit_P.{pak,ucas,utoc} - Padawan only",
    "core/padawan-swap": "pakchunk99-ZCOMOpenKit_P.{pak,ucas,utoc} - Padawan only, swappable",
    "core/warrior": "pakchunk99-ZCOMOpenKit_P.{pak,ucas,utoc} - Warrior only",
    "core/warrior-swap": "pakchunk99-ZCOMOpenKit_P.{pak,ucas,utoc} - Warrior only, swappable",
    "ui-fit": "pakchunk98-ZCOMOpenKitUI_P.{pak,ucas,utoc} - the specialization and talent row refit",
    "saber-armoury": "pakchunk97-ZCOMOpenKitSaberArmoury_P.{pak,ucas,utoc} - lifts the Armory lightsaber lock",
    "native/common": "dlls/main.dll and enabled.txt - the module itself",
}

# One fragment per switch the module actually reads, named exactly as
# src/ZCOMOpenKit/src/dllmain.cpp names them. A name that drifts out of step is
# logged as option_ignored at runtime and silently does nothing, so the preview
# ships the true names to make the routing test honest.
FRAGMENTS = {
    "options/padawan.ini":
        "# Open Kit option: padawan\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/warrior.ini":
        "# Open Kit option: warrior\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/padawan-secondary.ini":
        "# Open Kit option: padawan-secondary\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/padawan-saber.ini":
        "# Open Kit option: padawan-saber\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe.ini":
        "# Open Kit option: wardrobe\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe-cly.ini":
        "# Open Kit option: wardrobe-cly\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe-jedi.ini":
        "# Open Kit option: wardrobe-jedi\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe-heroes.ini":
        "# Open Kit option: wardrobe-heroes\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe-anakin.ini":
        "# Open Kit option: wardrobe-anakin\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe-authored.ini":
        "# Open Kit option: wardrobe-authored\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe-helmets.ini":
        "# Open Kit option: wardrobe-helmets\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe-families.ini":
        "# Open Kit option: wardrobe-families\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/wardrobe-story.ini":
        "# Open Kit option: wardrobe-story\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/colours.ini":
        "# Open Kit option: colours\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/colours-extra.ini":
        "# Open Kit option: colours-extra\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/armoury-dc17m.ini":
        "# Open Kit option: armoury-dc17m\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/armoury-rex-pistol.ini":
        "# Open Kit option: armoury-rex-pistol\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/armoury-hero-weapons.ini":
        "# Open Kit option: armoury-hero-weapons\n"
        "# The module reads only the file name; contents are ignored.\n",
    "options/armoury-sabers.ini":
        "# Open Kit option: armoury-sabers\n"
        "# The module reads only the file name; contents are ignored.\n",
}

README = """Open Kit - FOMOD preview
========================

THIS DOES NOT INSTALL A WORKING MOD.

Open Kit is not built yet. This archive exists so the installer can be tested:
open it in ZCOM Mod Manager, Vortex or Mod Organizer 2 and walk the wizard.

What to check:

  1. Six steps appear, in order.
  2. The two "Core" steps only appear if you tick Core in step 1.
  3. The two "Wardrobe" steps only appear if you tick Wardrobe in step 1.
  3b. The "Armory" step only appears if you tick Armory in step 1.
  3c. Ticking Wardrobe OR Armory (or both) installs native/common exactly once.
  4. "Both kits" / "Leave hero kits pinned" / "All three sets" / "Fit helmets"
     / "Helmet voice" are pre-ticked as Recommended.
  5. The files it says it will install land where you expect:
       Core     -> SWZeroCompany\\Content\\Paks\\~mods
       Wardrobe -> SWZeroCompany\\Binaries\\Win64\\ue4ss\\Mods\\ZCOMOpenKit
       Armory   -> the same module folder, plus a pak for the Rex pistol category

Everything it installs is a .txt or a small .ini. There is no .pak, no .dll and
no enabled.txt, so deploying it cannot affect the game. Uninstall from your
manager when you are done, or delete the files by hand.
"""


def version() -> str:
    return (ROOT / "VERSION").read_text(encoding="utf-8").strip()


def stage() -> None:
    if STAGE.exists():
        shutil.rmtree(STAGE)
    STAGE.mkdir(parents=True)
    shutil.copytree(FOMOD, STAGE / "fomod")
    (STAGE / "README.txt").write_text(README, encoding="utf-8")

    for folder, payload in FOLDER_PAYLOAD.items():
        target = STAGE / folder
        target.mkdir(parents=True, exist_ok=True)
        (target / "PLACEHOLDER.txt").write_text(BANNER.format(payload), encoding="utf-8")

    for path, body in FRAGMENTS.items():
        target = STAGE / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(body, encoding="utf-8")


def validate() -> None:
    """Refuse to ship a preview whose installer does not check out."""
    result = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "check-fomod.py"), "--stage", str(STAGE)],
        capture_output=True, text=True,
    )
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode != 0:
        raise SystemExit("FOMOD validation failed; preview not built")


def archive(release: str) -> pathlib.Path:
    path = DIST / f"ZCOM-Open-Kit-v{release}-FOMOD-PREVIEW.zip"
    files = sorted(p for p in STAGE.rglob("*") if p.is_file())
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
        for source in files:
            # Fixed timestamps, so two builds of identical content hash alike.
            info = zipfile.ZipInfo(
                str(source.relative_to(STAGE)).replace("\\", "/"), (2026, 1, 1, 0, 0, 0)
            )
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            zf.writestr(info, source.read_bytes())
    return path


def main() -> int:
    DIST.mkdir(exist_ok=True)
    stage()
    validate()
    path = archive(version())
    print(f"\n{path.relative_to(ROOT)}  ({path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
