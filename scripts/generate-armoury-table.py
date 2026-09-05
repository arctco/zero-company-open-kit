#!/usr/bin/env python3
"""Emit the Armory rows for src/ZCOMOpenKit/src/armoury_table.inc.

The wardrobe is five hundred parts and had to be generated. The Armory is eight,
and could have been typed -- but the argument for each of them is a reading of
the game's data, and a reading that is not executable is a reading nobody can
check. So the rules live here and the rows fall out of them.

    retoc to-legacy --filter CPD_ --no-shaders --version UE5_6 <Paks> <legacy>
    # dump each CPD_WeaponSpec_*, CPD_GK_*, CPD_GearKit_*, CPD_WP_* with
    # UAssetAPI (lab/tools/UAssetProbe, or scripts/container-builder) into <json-tree>
    ./scripts/generate-armoury-table.py <json-tree> > src/ZCOMOpenKit/src/armoury_table.inc

## What the Armory is, measured rather than assumed

Three slots, not one, and they are three different pickers:

    Slot.Character.Specializations.Weapon           the weapon CLASS
    Slot.Character.Specializations.Weapon.GearKit   the weapon MODEL you carry
    Slot.Weapon.Type                                the model a weapon is skinned as

A weapon class grants a category tag -- `Part.Character.Specializations.Weapon.
Blaster.Rifle` and so on -- and every gear kit in that category *requires* that
tag. That is not a lock and it is not lifted here: it is how the game keeps
rifles out of the pistol list, and a character earns it by equipping the class.

## Two roadmap items turned out to be already shipped

The Armory as planned was "Clone Commando armour, Captain Rex's armour, the
DC-17M, and Rex's pistol". The first two are outfits, and the wardrobe session
swept every outfit in the game:

    CPD_H_Outfit_Clo010_*        the Clone Commando family (Accepts.Outfit.CloCommando)
    CPD_H_Outfit_CaptainRexA_*   Rex's armour

All fourteen are already rows in wardrobe_table.inc, behind
`wardrobe-authored.ini`. Only `CaptainRexA_PACK` is absent, and that is the
backpack rule, not an oversight. So the Armory is weapons.

## What this module lifts, and what it refuses to

    Accepts.AuthoredOnly        lifted -- proven liftable in game via Anakin's outfit
    Accepts.Unlocks.DC17M       lifted -- required by one part, granted by NONE of
                                the game's 3,363 customization parts
    Info.Name.<Hero>            lifted, on a discarded copy, for one question

    Part.Character.Class.*      NOT lifted. `CPD_WeaponSpec_Enemy_*` want a
                                Battledroid or Imperial class. Scoping that in
                                would claim an operator is an NPC, which is the
                                argument that removed Anakin's class.
    Part.Character.Specializations.Weapon.*
                                NOT lifted -- the weapon category, above.
    Part.Weapon.Type.*          NOT lifted -- which weapon a model fits.

**No part in any of these three slots requires `Accepts.Character.Specializations`.**
That was recorded as the single biggest risk to this work -- the scripted
authoring family that Anakin, Rex and every enemy tactical spec belong to. It
does not reach the Armory: measured across all 208 parts in the weapon families,
the tag does not appear once.

## What a finished player weapon looks like, and what is excluded for not being one

Read off the stock, unlocked kits, every one of which has all four: a
`DisplayName`, both images, at least one ability, and a `Part.Weapon.Type.*` tag
saying which weapon it is. The exclusions below are each a part failing one of
those in a way that would show on screen.

    no DisplayName          a blank tile. Trilla's saber, the Umbaran rifle,
                            every CPD_GearKit_* NPC loadout.
    no DefaultPart          a weapon class that equips and draws no weapon.
                            CPD_WeaponSpec_Melee_1H and CPD_WeaponSpec_Pacifist
                            are exactly this; CHANGELOG.md already flagged the
                            first as "test, do not adopt".
    no weapon category      a gear kit that declares no category appears in
                            EVERY class's list. CPD_GK_Scattergun declares
                            nothing at all but the slot and the lock, and carries
                            no ability either.
    an _Enemy twin          the player already has the identical weapon under its
                            own name. Eight of these; offering both would put two
                            tiles for one gun in the list, which is the same
                            argument the colour work used for swatches nobody can
                            tell apart.
    disagrees with itself   CPD_WP_Type_Rifle_Umbaran declares `Slot.Character`
                            while requiring `Part.Weapon.Type.2HRifle.Rifle`, and
                            its DisplayName is Westar-M5's. Two independent
                            statements that it is unfinished -- the same
                            cross-check that caught thirteen outfit parts. The
                            slot is what excludes it; the borrowed name is the
                            corroboration.

## A rule that was written, tested against the data, and demoted

"Drop a part whose DisplayName belongs to another part" looked like the same
cross-check that caught those thirteen outfits. Run against the Armory it threw
out eleven parts, and reading them says why: **a gear kit and its weapon model
share a DisplayName by design** -- `CPD_GK_Pistol_Cly` and
`CPD_WP_Type_Pistol_Cly` are both "434", and twenty pairs do this. Restricted to
collisions *within one slot* it then catches nothing the other rules miss: the
`_Enemy` twins have their own rule and the Umbaran rifle is out on its slot.

What it does still find is worth saying out loud rather than acting on, so it is
reported as a caveat per row:

    CPD_GK_Pistol_DC-17_Rex     is called "DC-17", exactly like the stock kit
    CPD_WeaponSpec_Blaster_Pistol_Rex  is called "Blaster Pistol", exactly like
                                the stock pistol class

Both are real, distinct assets -- Rex's kit equips `GK_Pistol_DC-17_Rex`, which
is its own gear kit -- so the shared label is a presentation wart on a working
weapon, not evidence of an unfinished one. It is also why Rex is his own switch:
a player who would rather not have two identically-named cards can leave it off.

## The nameless-tile rule was measured on blasters and is wrong about sabers

"No `DisplayName` is a blank tile" was read off the stock *blaster* kits, every
one of which is named. Applied to the saber list it excludes the wrong thing:
**no lightsaber gear kit in the game has a DisplayName at all**, including
`CPD_GK_Lightsaber_TelRea`, which is the one a Padawan already carries as the
default part of her class. A nameless tile is not a defect there, it is the norm
for the category.

So the rule is now asked per weapon category: a missing name excludes a kit only
if some *other* kit in the same category has one. Where nothing in the category
is named, namelessness says nothing.

That is the **second** time in this file a rule right about one family of parts
turned out wrong about another -- the first being the DisplayName-collision rule
below. Both were caught the same way: by running the rule and reading what it
dropped, rather than by re-reading the rule.

What it lets in is the Second Sister's lightsaber
(`CPD_GK_Lightsaber_Enemy_Imperial_Trilla` -> `GK_Lightsaber_SecondSister_Trilla`
-> `BP_LightSaber_Enemy_Imperial_Trilla`), gated on `Melee.2H` and
`AuthoredOnly`, so it is reachable by anyone carrying the two-handed saber class.

**And Tel-Rea's own hilt with it, which is load-bearing rather than incidental.**
It is her class's `DefaultPart`, so it is what a Padawan is holding -- but it is
not otherwise in any list. Offering the Inquisitor hilt without it would let a
player switch away from the stock saber and have no way back to it.

## Known and accepted: three of the offered parts have no icon

`CPD_GK_Pistol_Hawks`, `CPD_GK_Pistol_DC-17_Rex`, `CPD_WP_Type_Pistol_Hawks` and
`CPD_WP_Type_Pistol_S-5` carry a DisplayName and no `SmallImage`/`LargeImage`.
Unversioned serialization omits a property at its default, so that is an empty
brush rather than a parse failure: the tile draws with a name and no picture.
That is a presentation cost on a working weapon, it is the player's to judge,
and each is behind a switch. It is reported by this script rather than hidden.

`CPD_GK_Pistol_DC-17_Rex` additionally grants no `Part.Weapon.Type.*` tag, so
the weapon-customization screen has nothing to key a model list on for it. Rex's
*class* is the part that carries his abilities; this kit is his model.
"""

import json
import pathlib
import sys

SLOT = "br.Customization.Slot."
NAME = "br.Customization.Part.Character.Info.Name."
ACCEPTS = "br.Customization.Accepts."
CATEGORY = "br.Customization.Part.Character.Specializations.Weapon"
WEAPON_TYPE = "br.Customization.Part.Weapon.Type."
CLASS = "br.Customization.Part.Character.Class."

CLASS_SLOT = SLOT + "Character.Specializations.Weapon"
KIT_SLOT = SLOT + "Character.Specializations.Weapon.GearKit"
MODEL_SLOT = SLOT + "Weapon.Type"

CONSTANTS = {
    "ClyKullervo": "kClyName",
    "CaptainRex": "kCaptainRexName",
    "Trick": "kTrickName",
    "Anakin": "kAnakinName",
    "Tel-ReaVokoss": "kTelReaName",
}

# One file per weapon a player would recognise, which is what a FOMOD checkbox
# has to mean. Assigned by name, the way the wardrobe assigns its curated
# families.
SWITCHES = (
    ("kOptionArmouryDC17M", ("DC-17M", "DC17M")),
    ("kOptionArmouryRexPistol", ("Pistol_DC-17_Rex", "Blaster_Pistol_Rex")),
    ("kOptionArmourySabers", ("Lightsaber",)),
)
DEFAULT_SWITCH = "kOptionArmouryHeroWeapons"


def strings(node):
    if isinstance(node, dict):
        for value in node.values():
            yield from strings(value)
    elif isinstance(node, list):
        for value in node:
            yield from strings(value)
    elif isinstance(node, str):
        yield node


def read(path):
    """Everything the rules below need, and nothing inferred."""
    name = path.stem
    asset = json.loads(path.read_text())
    exports = asset.get("Exports")
    if not isinstance(exports, list):
        return None
    part = {"name": name, "allowed": None, "grants": [], "display": None,
            "images": 0, "abilities": 0, "default": None}
    for export in exports:
        if not isinstance(export, dict):
            continue
        owner = export.get("ObjectName") or ""
        for prop in export.get("Data") or []:
            if not isinstance(prop, dict):
                continue
            key = prop.get("Name")
            if key == "AllowedSlots" and owner == name:
                part["allowed"] = list(dict.fromkeys(strings(prop.get("Value"))))
            elif key == "GameplayTags" and owner.startswith("CustomizationFragmentGameplayTags"):
                part["grants"] += list(strings(prop.get("Value")))
            elif key == "DisplayName":
                part["display"] = prop.get("Value")
            elif key in ("SmallImage", "LargeImage"):
                part["images"] += 1
            elif key == "Abilities":
                part["abilities"] += len(prop.get("Value") or [])
            elif key == "DefaultPart":
                value = prop.get("Value") or {}
                part["default"] = (value.get("AssetPath") or {}).get("AssetName")
    # An absent AllowedSlots is an empty one -- unversioned serialization omits
    # a property at its default. It is kept rather than skipped, because
    # CPD_WeaponSpec_Blaster_Pistol is exactly that and the label check below
    # has to be able to see it. classify() rejects it for naming no slot.
    if part["allowed"] is None:
        part["allowed"] = []
    part["grants"] = sorted(set(part["grants"]))
    return part


def classify(part, offered_kit_categories, named_categories, all_names):
    """Return (option, slot, scoped constants) or (None, None, reason)."""
    allowed = part["allowed"]
    slots = [t for t in allowed if t.startswith(SLOT)]
    if len(set(slots)) != 1:
        return None, None, "AllowedSlots does not name exactly one slot"
    slot = slots[0]
    if slot not in (CLASS_SLOT, KIT_SLOT, MODEL_SLOT):
        return None, None, "not an Armory slot"

    heroes = [t[len(NAME):] for t in allowed if t.startswith(NAME)]
    accepts = [t[len(ACCEPTS):] for t in allowed if t.startswith(ACCEPTS)]
    classes = [t for t in allowed if t.startswith(CLASS)]

    # The lift, and it is the whole of it.
    scoped = []
    for hero in heroes:
        if hero not in CONSTANTS:
            return None, None, f"no constant for hero {hero}"
        scoped.append(CONSTANTS[hero])
    for accept in accepts:
        if accept == "AuthoredOnly" or accept.startswith("Unlocks."):
            scoped.append(f'STR("{ACCEPTS}{accept}")')
    if not scoped:
        return None, None, "already available -- nothing this module lifts"
    if classes:
        return None, None, "requires an NPC or droid character class"

    # A finished player part, measured against the stock unlocked ones.
    categories = {t for t in allowed if t.startswith(CATEGORY)}
    if not part["display"] and (slot != KIT_SLOT or categories & named_categories):
        return None, None, "no DisplayName -- a blank tile"

    if slot == CLASS_SLOT:
        if not part["default"]:
            return None, None, "a weapon class with no DefaultPart draws no weapon"
        categories = [t for t in part["grants"] if t.startswith(CATEGORY)]
        if not categories:
            return None, None, "a weapon class that grants no category"
        if not any(c in offered_kit_categories for c in categories):
            return None, None, "no named gear kit exists in the category it grants"
    elif slot == KIT_SLOT:
        if not any(t.startswith(CATEGORY) for t in allowed):
            return None, None, "a gear kit in no category appears in every list"
        if part["name"].endswith("_Enemy") and part["name"][: -len("_Enemy")] in all_names:
            return None, None, "an _Enemy twin of a weapon already offered"
    else:  # MODEL_SLOT
        if not any(t.startswith(WEAPON_TYPE) for t in allowed):
            return None, None, "a weapon model that fits no weapon"

    for option, prefixes in SWITCHES:
        if any(token in part["name"] for token in prefixes):
            return option, slot, scoped
    return DEFAULT_SWITCH, slot, scoped


def main(tree):
    paths = sorted(p for p in pathlib.Path(tree).glob("CPD_*.json")
                   if not p.name.endswith(".summary.json"))
    parts = [p for p in (read(path) for path in paths) if p is not None]
    all_names = {p["name"] for p in parts}

    # Kept, but as a caveat rather than a rule: see the note in the header.
    # Counted within one slot, because a gear kit and its weapon model share a
    # DisplayName by design and a global count throws out twenty real pairs.
    # A part that declares no slot at all is offered in every picker, so it
    # counts against all of them -- which is how the stock CPD_WeaponSpec_Blaster_Pistol,
    # whose AllowedSlots is empty, ends up sharing a card name with Rex's class.
    armoury_slots = (CLASS_SLOT, KIT_SLOT, MODEL_SLOT)
    shared_label = {}
    for part in parts:
        if not part["display"]:
            continue
        slots = {t for t in part["allowed"] if t.startswith(SLOT)}
        against = armoury_slots if not slots else (slots if len(slots) == 1 else ())
        for slot in against:
            key = (slot, part["display"])
            shared_label[key] = shared_label.get(key, 0) + 1

    # Which categories have a gear kit a player would see a name for. Used by
    # the weapon-class rule, so a class is never offered into an empty list.
    offered_kit_categories = set()
    for part in parts:
        if part["display"] and KIT_SLOT in part["allowed"]:
            offered_kit_categories |= {t for t in part["allowed"] if t.startswith(CATEGORY)}

    # Which weapon categories name their gear kits at all. Used to decide
    # whether a missing DisplayName means anything -- see the header. Every
    # blaster category names its kits; no melee category does.
    named_categories = set()
    for part in parts:
        if part["display"] and KIT_SLOT in part["allowed"]:
            named_categories |= {t for t in part["allowed"] if t.startswith(CATEGORY)}

    rows, skipped = [], []
    for part in parts:
        option, slot, result = classify(part, offered_kit_categories, named_categories,
                                        all_names)
        if option is None:
            skipped.append((part["name"], result))
            continue
        rows.append((option, part["name"], slot, result, part["images"],
                     shared_label.get((slot, part["display"]), 0) > 1, part["display"]))

    print("// Generated by scripts/generate-armoury-table.py -- do not edit by hand.")
    print("// Every row, and every exclusion, is read out of the game's own cooked")
    print("// assets. Run the script again to regenerate; the header explains each rule.")
    print(f"// {len(rows)} rows.")
    widest = max((len(scoped) for _, _, _, scoped, _, _, _ in rows), default=0)
    print(f"// Widest lift: {widest} tags on one part.")
    for option, name, slot, scoped, images, shared, label in sorted(rows):
        if images == 0:
            print("    // No icon of its own: the tile draws its name and no picture.")
        if shared:
            print(f'    // Shares the DisplayName "{label}" with another part in this')
            print("    // slot, so the two tiles read alike. A distinct asset even so.")
        print(f'    {{STR("{slot}"),')
        print(f'     STR("{name}"), {option}, {{{", ".join(scoped)}}}}},')

    counts = {}
    for option, _, _, _, _, _, _ in rows:
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
