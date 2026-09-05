#!/usr/bin/env python3
"""Read a UE4SS crash minidump and name the faulting function.

    ./scripts/read-minidump.py ".../ue4ss/crash_2026_09_04_03_30_12.dmp"

UE4SS writes a .dmp beside itself when it catches an access violation, and the
game keeps running -- so a crash can be reported as "it crashed but it loaded"
with no UE crash report to look at. Nothing on a Linux box reads these, hence
this.

The dump records module load addresses, so the faulting address reduces to an
RVA in SWZeroCompany.exe, and the PDB the game ships names it. See
docs/SEAM-MAP.md for that mapping.
"""
import struct
import subprocess
import sys
from pathlib import Path

GAME = Path.home() / ".steam/steam/steamapps/common/Star Wars Zero Company"
EXE = GAME / "SWZeroCompany/Binaries/Win64/SWZeroCompany.exe"
PREFERRED_BASE = 0x140000000

STREAM_MODULE_LIST = 4
STREAM_EXCEPTION = 6

# Access violation, and the shape of its two parameters.
EXCEPTION_ACCESS_VIOLATION = 0xC0000005
ACCESS_KIND = {0: "read", 1: "write", 8: "execute"}


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def u64(data, offset):
    return struct.unpack_from("<Q", data, offset)[0]


def minidump_string(data, rva):
    length = u32(data, rva)
    return data[rva + 4 : rva + 4 + length].decode("utf-16-le", errors="replace")


def read_streams(data):
    if data[:4] != b"MDMP":
        raise SystemExit("not a minidump")
    count, directory = u32(data, 0x08), u32(data, 0x0C)
    streams = {}
    for index in range(count):
        entry = directory + index * 12
        streams[u32(data, entry)] = (u32(data, entry + 4), u32(data, entry + 8))
    return streams


def read_modules(data, streams):
    if STREAM_MODULE_LIST not in streams:
        return []
    _, rva = streams[STREAM_MODULE_LIST]
    modules = []
    for index in range(u32(data, rva)):
        # MINIDUMP_MODULE is 108 bytes; the name RVA sits at +20, after
        # BaseOfImage, SizeOfImage, CheckSum and TimeDateStamp.
        entry = rva + 4 + index * 108
        modules.append(
            (u64(data, entry), u32(data, entry + 8), minidump_string(data, u32(data, entry + 20)))
        )
    return sorted(modules)


def owning_module(modules, address):
    for base, size, name in modules:
        if base <= address < base + size:
            return name.split("\\")[-1], address - base
    return None, None


def symbolize(rva):
    if not EXE.exists():
        return None
    try:
        out = subprocess.run(
            ["llvm-symbolizer", f"--obj={EXE}", "--functions=linkage", "--demangle",
             hex(PREFERRED_BASE + rva)],
            capture_output=True, text=True, timeout=120,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    first = out.stdout.strip().splitlines()
    return first[0] if first and first[0] not in ("??", "") else None


def report(path):
    data = Path(path).read_bytes()
    streams = read_streams(data)
    modules = read_modules(data, streams)

    print(f"== {Path(path).name}")
    if STREAM_EXCEPTION not in streams:
        print("  no exception stream")
        return

    _, rva = streams[STREAM_EXCEPTION]
    code = u32(data, rva + 8)
    address = u64(data, rva + 24)
    parameters = [u64(data, rva + 40 + 8 * i) for i in range(min(u32(data, rva + 32), 15))]

    print(f"  exception 0x{code:08X} at 0x{address:X}")
    if code == EXCEPTION_ACCESS_VIOLATION and len(parameters) >= 2:
        kind = ACCESS_KIND.get(parameters[0], f"kind {parameters[0]}")
        print(f"  access violation: {kind} of 0x{parameters[1]:X}")

    name, offset = owning_module(modules, address)
    if name is None:
        print("  faulting address is in no mapped module")
    else:
        print(f"  in {name} + 0x{offset:X}")
        if name.lower() == "swzerocompany.exe":
            symbol = symbolize(offset)
            print(f"  {symbol}" if symbol else "  (no symbol; is the PDB present?)")
        else:
            print("  no PDB for this module, so no symbol -- match it by build identity instead")

    print("  loaded modules:")
    for base, size, module in modules:
        short = module.split("\\")[-1]
        print(f"    0x{base:012X} +0x{size:08X} {short}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for argument in sys.argv[1:]:
        report(argument)
        print()
