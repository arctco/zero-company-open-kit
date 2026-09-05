#!/usr/bin/env python3
"""Emit the wardrobe rows for src/ZCOMOpenKit/src/wardrobe_table.inc.

The kits are hand-written in dllmain.cpp: there are six and each needed an
argument. The wardrobe is five hundred parts and is not worth typing -- it is a
mechanical consequence of what the game authors, so it is derived from the game,
and this script is how it is checked and regenerated.

    ./scripts/generate-wardrobe-table.py <json-tree> > src/ZCOMOpenKit/src/wardrobe_table.inc

<json-tree> is every CPD_H_Outfit*.uasset extracted with

    retoc to-legacy --filter CPD_H_Outfit --no-shaders --version UE5_6 <Paks> <tree>

and dumped to JSON with UAssetAPI (scripts/container-builder has the reference).

## What gates an outfit, and what this module does about each

Measured across all 1,253 outfit part definitions in the game:

    nothing                     already in everyone's wardrobe; not our business
    a hero's Info.Name          lifted -- the same lift the kits make
    Accepts.Outfit.<family>     lifted -- a capability, the shape of Mdo
    Accepts.AuthoredOnly        lifted -- proven liftable in game, see below
    Accepts.Helm.<species>      lifted, opt-in: it is a FIT gate, not a lock
    Accepts.Unlocks.Story_*     lifted, opt-out: campaign progress, not identity
    Accepts.Unlocks.Deluxe      NOT lifted -- a paid edition entitlement
    Part.Character.Rig.<other>  NOT lifted -- a different skeleton, not a gate

`Accepts.AuthoredOnly` is the widest of these and was the most doubtful. No part
in the entire game grants it -- a sweep of all 3,362 customization parts found
zero -- so it reads as "assigned by authoring, never chosen by a player". It was
scoped for Anakin's arms, boots and legs as a test, and the game offered all
three, so it is liftable. It names nobody, so unlike a hero's Info.Name it cannot
pull one character's content onto another.

`Accepts.Helm.<species>` is different from every other gate here: it is not
about permission but about whether a helmet fits the head under it. Only Human,
Clone, Mirialan and Umbaran carry `Accepts.Helm.Human`, so a Rodian, Togruta or
Zabrak is offered a different, much smaller set of helmets. Lifting it is what
makes every operator see the same list; the cost is that a helmet shaped for a
human skull sits on montrals, lekku and horns. That is a look, not a bug, and it
is the player's call -- hence a switch.

## What is excluded, and why -- every rule read out of the asset

    no Outfit slot        not a garment: Fathom's head is a face, and four _PACK
                          parts declare no slot tag at all
    Outfit.Pack.Mesh      backpacks; a torso that wants one carries it in a mesh
                          fragment of its own
    bReplacesBaseMesh     replaces the wearer's BODY rather than dressing it.
                          Kabb's torsos would put a Herglic body on an operator
    non-humanoid Rig      a different skeleton. Kabb's gear needs Rig.Kabb and
                          the game declines it for everyone else, correctly
    Deluxe / Preorder     content sold as a paid edition
"""

import json
import pathlib
import sys

SLOT = "br.Customization.Slot.Character."
NAME = "br.Customization.Part.Character.Info.Name."
RIG = "br.Customization.Part.Character.Rig."
ACCEPTS = "br.Customization.Accepts."

# Every outfit-family capability is scoped, and nothing is assumed granted.
#
# An earlier revision skipped any family that CPD_Char_Class_Hero_Humanoid
# grants, on the reasoning that Hawks and every recruit already carry those
# fourteen so the part needs no lift. That is true of Hawks and false of the
# characters he recruits alongside, because the capability comes from the
# character's CLASS and the authored heroes have far poorer ones:
#
#     Hero_Humanoid  (Hawks, every recruit)   14 families
#     Hero_Umbaran   (Luco)                   11
#     Hero_Mando     (Cly)                     1  -- only Mdo
#     Hero_Padawan   (Tel-Rea)                 0  -- none at all
#
# So the 195 parts skipped as "everyone already has these" were skipped on
# Hawks's behalf, and Cly and Tel-Rea could not wear any of them. Scoping the
# family for every part costs Hawks nothing -- the catalogue hook already
# refuses to append a definition the game has listed, and adding a tag the
# character already carries changes no answer -- and it is what lets the rest of
# the company wear the same clothes.

CONSTANTS = {
    "Tel-ReaVokoss": "kTelReaName", "ClyKullervo": "kClyName", "Anakin": "kAnakinName",
    "Hawks": "kHawksName", "KabbUppercut": "kKabbName", "JaeMordant": "kJaeName",
    "Baker": "kBakerName", "Dozen": "kDozenName", "Sawtooth": "kSawtoothName",
    "LucoBronc": "kLucoName", "TheCommander": "kCommanderName", "Trick": "kTrickName",
    "Visser": "kVisserName",
}

# Families the module offers under their own curated switches, so the generator
# assigns them to that switch rather than to the general wardrobe.
CURATED = {
    "kOptionWardrobe": ("CPD_H_Outfit_Man001A_", "CPD_H_Outfit_Man002A_"),
    "kOptionWardrobeCly": ("CPD_H_Outfit_Cly_", "CPD_H_Outfit_ClyB_"),
    "kOptionWardrobeJedi": ("CPD_H_Outfit_TelReaA_", "CPD_H_Outfit_TelReaB_"),
    "kOptionWardrobeAnakin": ("CPD_H_Outfit_Anakin_",),
}


def strings(node):
    if isinstance(node, dict):
        for value in node.values():
            yield from strings(value)
    elif isinstance(node, list):
        for value in node:
            yield from strings(value)
    elif isinstance(node, str):
        yield node


# A skeletal mesh is named ..._TORS, ..._HELM and so on, and that suffix is the
# one independent statement of what a part actually is. It is used to check the
# slot the part declares -- see classify().
MESH_SLOT = {
    "TORS": "Torso", "LEGS": "Legs", "ARMS": "Arms",
    "BOOT": "Boots", "HELM": "Helmet", "PACK": "Pack",
}


def read(path):
    name = path.stem
    asset = json.loads(path.read_text())
    allowed, replaces, meshes = None, False, []
    exports = asset.get("Exports")
    if not isinstance(exports, list):
        return None
    for export in exports:
        if not isinstance(export, dict):
            continue
        for prop in export.get("Data") or []:
            if not isinstance(prop, dict):
                continue
            if prop.get("Name") == "bReplacesBaseMesh":
                replaces = True
            if prop.get("Name") == "SkeletalMeshOptions":
                meshes += [t.rsplit("/", 1)[-1] for t in strings(prop.get("Value"))
                           if t.startswith("/Game")]
            if prop.get("Name") == "AllowedSlots" and export.get("ObjectName") == name:
                allowed = list(dict.fromkeys(strings(prop.get("Value"))))
    if allowed is None:
        return None
    mesh_kinds = {MESH_SLOT[token] for mesh in meshes
                  for token in mesh.split("_") if token in MESH_SLOT}
    name_kinds = [MESH_SLOT[token] for token in name.split("_") if token in MESH_SLOT]
    return {"name": name, "allowed": allowed, "replaces": replaces,
            "mesh_kind": mesh_kinds.pop() if len(mesh_kinds) == 1 else None,
            "name_kind": name_kinds[0] if name_kinds else None}


def classify(part):
    """Return (option, scoped constants) or (None, reason it is not offered)."""
    allowed = part["allowed"]
    slots = [t for t in allowed if t.startswith(SLOT)]
    heroes = [t[len(NAME):] for t in allowed if t.startswith(NAME)]
    rigs = [t[len(RIG):] for t in allowed if t.startswith(RIG)]
    accepts = [t[len(ACCEPTS):] for t in allowed if t.startswith(ACCEPTS)]

    # A well-formed part names exactly one slot, in a container of two to five
    # tags. Sixteen parts -- the CPD_H_Outfit_Clo001_BOOT_Tint* family -- come
    # back from UAssetAPI with two hundred tags naming three different slots,
    # which is a container that did not parse rather than a part that is valid
    # in three places. Guessing which slot such a part belongs in would put a
    # boot in the arms list, so they are dropped instead.
    if len(set(slots)) > 1 or len(allowed) > 16:
        return None, "AllowedSlots did not parse (multiple slots)"
    if not slots or not slots[0].startswith(SLOT + "Outfit."):
        return None, "not a garment (no outfit slot)"

    # The slot a part declares is checked against two independent statements of
    # what the part is: its own name, and the mesh it draws. Two families in the
    # game disagree with themselves -- every CPD_H_Outfit_Clo009_* and
    # CPD_H_Outfit_PlagueTrooper_* part declares Outfit.Arms.Mesh whatever it is,
    # while being named and meshed as a torso, helmet, boot or leg. Appending
    # those to the arms list would put a torso on somebody's forearms.
    #
    # BOTH sources have to agree with each other and disagree with the slot
    # before a part is dropped, because one alone gives false positives: a torso
    # may legitimately draw a helmet mesh, which is what the "H" means in
    # CPD_H_Outfit_Luco_TORS_MSA_HTALB, and the mesh check alone threw it out.
    declared = slots[0][len(SLOT + "Outfit."):].removesuffix(".Mesh")
    if (part["mesh_kind"] is not None
            and part["mesh_kind"] == part["name_kind"] != declared):
        return None, "name and mesh agree the slot it declares is wrong"
    if slots[0].endswith("Pack.Mesh"):
        return None, "backpack"
    if part["replaces"]:
        return None, "replaces the wearer's body"
    if any(rig != "Humanoid" for rig in rigs):
        return None, "needs a non-humanoid skeleton"

    unlocks = [a for a in accepts if a.startswith("Unlocks.")]
    if any("Deluxe" in a or "Preorder" in a for a in unlocks):
        return None, "paid edition entitlement"

    # Scoped tags are emitted as literals rather than named constants. There are
    # too many of them to name -- every story unlock is its own tag -- and a
    # literal says exactly what is being lifted at the row that lifts it.
    scoped = []
    for hero in heroes:
        if hero not in CONSTANTS:
            return None, f"no constant for hero {hero}"
        scoped.append(CONSTANTS[hero])
    for a in accepts:
        if a == "AuthoredOnly" or a.startswith("Helm.") or a.startswith("Unlocks."):
            scoped.append(f'STR("{ACCEPTS}{a}")')
        elif a.startswith("Outfit.") and not a.startswith("Outfit.Color"):
            scoped.append(f'STR("{ACCEPTS}{a}")')

    if not scoped:
        return None, "already available to everyone"

    for option, prefixes in CURATED.items():
        if part["name"].startswith(prefixes):
            return option, scoped

    # Otherwise the switch is named for the widest thing the row has to lift, so
    # a player turning one off knows exactly what they are turning off.
    # A part often needs more than one lift, so it is filed under the switch
    # whose meaning a player would most want control of. Story first because it
    # is an entitlement -- a story-gated part must be behind the story switch or
    # turning that switch off would not turn it off. Then helmets, because that
    # is the one switch with a visible cost. Then the rest, widest first.
    if unlocks:
        return "kOptionWardrobeStory", scoped
    if any("Accepts.Helm." in s for s in scoped):
        return "kOptionWardrobeHelmets", scoped
    if any("AuthoredOnly" in s for s in scoped):
        return "kOptionWardrobeAuthored", scoped
    if heroes:
        return "kOptionWardrobeHeroes", scoped
    return "kOptionWardrobeFamilies", scoped


def main(tree):
    rows, skipped = [], []
    for path in sorted(pathlib.Path(tree).glob("CPD_H_Outfit*.json")):
        part = read(path)
        if part is None:
            continue
        option, result = classify(part)
        if option is None:
            skipped.append((part["name"], result))
            continue
        slot = [t for t in part["allowed"] if t.startswith(SLOT)][0]
        rows.append((option, part["name"], slot, result))

    print("// Generated by scripts/generate-wardrobe-table.py -- do not edit by hand.")
    print("// Every row, and every exclusion, is read out of the game's own cooked")
    print("// assets. Run the script again to regenerate; the header explains each rule.")
    print(f"// {len(rows)} rows.")
    widest = max(len(scoped) for _, _, _, scoped in rows)
    print(f"// Widest lift: {widest} tags on one part.")
    for option, name, slot, scoped in sorted(rows):
        print(f'    {{STR("{slot}"),')
        print(f'     STR("{name}"), {option}, {{{", ".join(scoped)}}}}},')

    counts = {}
    for option, _, _, _ in rows:
        counts[option] = counts.get(option, 0) + 1
    print(f"\n// {len(rows)} rows, widest lift {widest}", file=sys.stderr)
    for option, count in sorted(counts.items()):
        print(f"//   {option:32} {count}", file=sys.stderr)
    reasons = {}
    for _, why in skipped:
        reasons[why] = reasons.get(why, 0) + 1
    for why, count in sorted(reasons.items(), key=lambda kv: -kv[1]):
        print(f"// excluded {count:5}  {why}", file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv[1])
