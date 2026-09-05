#!/usr/bin/env python3
"""Build the real, installable FOMOD archive.

    ./scripts/build-fomod.py

Unlike build-fomod-preview.py, which packages the installer against inert
placeholders so the wizard can be walked safely, this one stages the actual
payload: the built module, the option fragments and every container variant.

Run scripts/build-native.sh first -- the module is not built here, because
cross-compiling is slow and the usual reason to rebuild the archive is a
packaging change rather than a code change.

Every source path named in ModuleConfig.xml must exist in the stage, and
check-fomod.py --stage is run at the end to prove it. A FOMOD that references a
missing file fails at the user's end, after the download, which is the failure
this script exists to make impossible.
"""

import pathlib
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
FOMOD = ROOT / "packaging" / "fomod"
CONTAINER = ROOT / "container"
DIST = ROOT / "dist"
STAGE = DIST / "fomod"
BUILT_DLL = DIST / "native" / "ZCOMOpenKit" / "ZCOMOpenKit.dll"

# Reproducible archives: a fixed timestamp so two builds of the same tree are
# byte-identical and a release can be diffed against its predecessor.
STAMP = (2026, 1, 1, 0, 0, 0)

# Every switch the module reads, exactly as src/ZCOMOpenKit/src/dllmain.cpp
# names them. A name that drifts out of step here is logged as option_ignored at
# runtime and silently does nothing, so this list is checked against the built
# DLL below rather than trusted.
OPTIONS = [
    "padawan", "warrior", "padawan-secondary", "padawan-saber",
    "wardrobe", "wardrobe-cly", "wardrobe-jedi", "wardrobe-heroes",
    "wardrobe-anakin", "wardrobe-authored", "wardrobe-helmets",
    "wardrobe-families", "wardrobe-story",
    "colours", "colours-extra",
    "armoury-dc17m", "armoury-rex-pistol", "armoury-hero-weapons",
    "armoury-sabers",
]

# Container variant -> the folder the installer names as a source.
CONTAINERS = {
    "both": "core/both",
    "both-swap": "core/both-swap",
    "padawan": "core/padawan",
    "padawan-swap": "core/padawan-swap",
    "warrior": "core/warrior",
    "warrior-swap": "core/warrior-swap",
    "ui-fit": "ui-fit",
    "saber-armoury": "saber-armoury",
}

FRAGMENT = (
    "# Open Kit option: {name}\n"
    "# Presence is the signal; the contents of this file are ignored.\n"
    "# Delete it to turn this option off, put it back to turn it on.\n"
    "# Takes effect on the next launch.\n"
)


def fail(message: str) -> None:
    sys.exit(f"error: {message}")


def main() -> int:
    version = (ROOT / "VERSION").read_text().strip()

    if not BUILT_DLL.is_file():
        fail(f"{BUILT_DLL.relative_to(ROOT)} not found -- run:\n"
             f"  ./scripts/build-native.sh src/ZCOMOpenKit")

    # The archive is only honest if the DLL in it actually knows the options the
    # installer places. Checked against the binary, not against the source.
    blob = BUILT_DLL.read_bytes()
    missing = [n for n in OPTIONS
               if f"{n}.ini".encode("utf-16-le") not in blob]
    if missing:
        fail("the built DLL does not contain these option names, so installing "
             "them would do nothing:\n  " + "\n  ".join(missing))

    if STAGE.exists():
        shutil.rmtree(STAGE)
    STAGE.mkdir(parents=True)

    # 1. The installer itself.
    shutil.copytree(FOMOD, STAGE / "fomod")

    # 2. The module.
    mod = STAGE / "native" / "common"
    (mod / "dlls").mkdir(parents=True)
    shutil.copy2(BUILT_DLL, mod / "dlls" / "main.dll")
    (mod / "enabled.txt").write_text("")

    # 3. One option fragment per switch.
    options = STAGE / "options"
    options.mkdir()
    for name in OPTIONS:
        (options / f"{name}.ini").write_text(FRAGMENT.format(name=name))

    # 4. Per-component manifests. The ZCOM Mod Manager names a component from
    #    the closest manifest above its files; with none, it synthesises one
    #    from fomod/info.xml and every component ends up sharing the installer's
    #    title. It reads these and never copies them into the game.
    manifests = STAGE / "manifests"
    manifests.mkdir()
    for src, dest in (("module", "module.json"), ("containers", "containers.json")):
        source = ROOT / "packaging" / f"zcom-mod.{src}.json"
        if not source.is_file():
            fail(f"packaging/zcom-mod.{src}.json is missing")
        text = source.read_text()
        if f'"{version}"' not in text:
            fail(f"packaging/zcom-mod.{src}.json version does not match "
                 f"VERSION ({version})")
        shutil.copy2(source, manifests / dest)

    # 5. The containers.
    for variant, dest in CONTAINERS.items():
        source = CONTAINER / variant
        if not source.is_dir():
            fail(f"container/{variant} is missing -- run scripts/build-container.sh")
        target = STAGE / dest
        target.mkdir(parents=True, exist_ok=True)
        paks = sorted(p for p in source.iterdir() if p.suffix in {".pak", ".ucas", ".utoc"})
        if not paks:
            fail(f"container/{variant} holds no .pak/.ucas/.utoc")
        for pak in paks:
            shutil.copy2(pak, target / pak.name)

    # 6. Prove every source path the installer names exists in the stage.
    check = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "check-fomod.py"), "--stage", str(STAGE)],
        capture_output=True, text=True)
    sys.stdout.write(check.stdout)
    sys.stderr.write(check.stderr)
    if check.returncode != 0:
        fail("check-fomod.py rejected the staged archive")

    out = DIST / f"ZCOM-Open-Kit-v{version}-FOMOD.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for path in sorted(STAGE.rglob("*")):
            if path.is_file():
                info = zipfile.ZipInfo(str(path.relative_to(STAGE)), date_time=STAMP)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                z.writestr(info, path.read_bytes())

    files = len(zipfile.ZipFile(out).namelist())
    print(f"\n{out.relative_to(ROOT)}  ({out.stat().st_size:,} bytes, {files} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
