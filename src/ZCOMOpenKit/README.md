# Native module

The one place Open Kit changes anything. It hooks three functions and drives
everything else from generated tables.

| Seam | Job |
|---|---|
| `UCustomizationStatics::GetCustomizationPartDefinitions` | Reads the slot tag being asked about and appends the rows registered for that slot. |
| `UCustomizationStatics::DoesPartIdMeetRequirements` | Answers the scoped question when the player *picks* one of this module's parts. Every other part goes straight through. |
| `UCustomizationPartsTagSubsystem::FilterAssetDataByTags` | The colour palettes, which do not pass through the catalogue at all. |

Fifteen RVAs are pinned, all resolved from the game's own PDB and byte-verified
before hooking.

```bash
../../scripts/build-native.sh src/ZCOMOpenKit
../../scripts/stage-native-mod.sh ZCOMOpenKit     # -> dist/ZCOMOpenKit.zip
```

## What is in it

Everything. Core, the wardrobe and the Armory are one module.

```text
Specializations.Tactical.Primary     Padawan, Warrior
Specializations.Tactical.Secondary   PadawanExtended ("Wayseeker")
Specializations.Talent               TheLostPadawan, TheMandalorian
Specializations.Weapon               Melee_2H_TelRea, Melee_1H_Anakin, DC-17M, Rex's pistol
Specializations.Weapon.GearKit       the hilts and Rex's twin DC-17s
Outfit.*.Mesh                        653 wardrobe rows
(no slot tag)                        135 outfit colours, 90 weapon paints
```

They ship together on purpose. Two DLLs hooking the same customization functions
compete for the same seam, and whether they coexist depends on both
implementations; one module with every feature behind an option fragment cannot
conflict with itself, and it halves the hooking surface.

## Why one hook reaches every list

Measured, not assumed. Of the four functions the game's PDB says can enumerate
customization parts, three are never called at all. This one assembles every
list in the game, and the **first tag of the container it is passed is the slot
being filled**. So a slot-keyed table reaches the specialization cards, the
wardrobe, the Armory and the appearance sliders through a single seam.

Colours are the exception and the reason the third hook exists: they never pass
through the catalogue, they carry no slot tag, and they come from the tag
subsystem instead.

## Asking rather than bypassing

The hero parts turned out never to be enumerated, rather than enumerated and
refused, so appending is enough to make a tile appear. It is not enough to let
the player pick it — the game re-asks the requirement, unscoped, on selection.
Specializations are not re-checked that way; outfits and gear are.

So the module asks the game the question itself, on a copy:

```text
copy the character's FGameplayTagContainer
add up to kMaxScopedTags (4) tags to the copy
put the copy to the game's own DoesPartIdMeetRequirements
destroy the copy, return the answer
```

Nothing is written to the character and no requirement is defeated. Exactly one
gate is lifted per row, named in the row itself; every other requirement a part
declares is still answered by the game against the real character. A row whose
scoped tag or slot tag this build cannot resolve disables **its own row only** —
refusing the whole mod would let one renamed tag take away features that still
work.

The technique is Sternab's, from the MIT wardrobe source. See
`../../THIRD_PARTY_NOTICES.md`.

## The tables are generated

`src/wardrobe_table.inc`, `src/armoury_table.inc` and `src/colour_table.inc` are
written by scripts, not by hand. Each generator's header carries the argument for
every row included and every part excluded, read out of the game's own cooked
assets rather than chosen:

```bash
../../scripts/generate-wardrobe-table.py
../../scripts/generate-armoury-table.py
../../scripts/generate-colour-table.py
```

The catalogue's size is deduced with `std::to_array` rather than written down; it
was wrong twice while it was a literal.

## Two rules that came out of a crash

- **Never call into the game from outside a hook.** An early probe polled for
  part resolution from `on_update` and dereferenced a null `UAssetManager` about
  a second after startup. The same call from inside a customization hook is safe,
  because the game is demonstrably already using the asset manager there.
- **Never log from inside a hook.** The catalogue seam is hot and
  `FName::ToString` is not something to call in it. Hooks record a bit;
  `on_update` reports it.

The native call is SEH-guarded either way.

## Configuration: a directory of fragments, not one config.ini

The module reads every `.ini` in its `options/` directory and merges them. There
are 19 switches. Contents are ignored — presence is the whole signal.

This exists for a specific reason. A FOMOD cannot merge files, only place them.
With a single `config.ini`, every independent toggle doubles the number of preset
files the installer has to carry: three checkboxes is eight presets, four is
sixteen. With one fragment per option, each checkbox maps to exactly one file and
the count stays linear, so new options are cheap to add forever.

Requirements this puts on the implementation:

- **Absent file means off.** Never fail because a fragment is missing; that is
  the normal way an option is disabled.
- **Order-independent merge.** Read the directory sorted, but no fragment may
  depend on another having been read first.
- **A missing `options/` directory is valid** and means the defaults, so someone
  who unzipped only the DLL still gets a working mod.
- **Unknown fragments are logged and ignored,** so an older DLL survives a newer
  install rather than refusing to load.

Users can edit, add or delete fragments afterwards without reinstalling. The
installer picks a starting point; it does not own the configuration.

Two switches are off by default: `padawan-saber.ini`, because the saber row in
the Weapon slot reaches character creation and softlocks the tutorial; and
`wardrobe-story.ini`, because the campaign hands those outfits out as it is
played.

## Reading the log

One `READY` line says what installed and what is active. Counts are **fields**
rather than literals, deliberately — a build containing 135 colours while
reporting a hardcoded 40 is exactly the failure that shipped once.

```text
[ZCOM_OPEN_KIT] READY hooks_active=true build=24874058
                seam=GetCustomizationPartDefinitions-post
                seam2=DoesPartIdMeetRequirements-scoped
                seam3=FilterAssetDataByTags helmet_fit=1
                rows=663/670 armoury_rows=10/10 switches=...
                identity_tags_written=0 character_edits=0
                requirements=asked-per-character
                lifted=named-capability-or-hero-name-on-a-discarded-copy
```

Per-row lines follow from `on_update`: `offered`, `declined`, `row_state` and
`pick_asked`. Rows log when their answer **changes**, not once ever — once-only
logging hid every evaluation after the first and cost three test cycles.
