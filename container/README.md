# IoStore containers

Three containers are built here. They claim different assets and mount together.

| Directory | Stem | Role |
|---|---|---|
| `ui-fit/` | `pakchunk98-ZCOMOpenKitUI_P` | The row refit. Recommended for anyone running the module. |
| `both/`, `padawan/`, `warrior/` and their `-swap` variants | `pakchunk99-ZCOMOpenKit_P` | The specialization pairing, and optionally the respec unlock. |
| `saber-armoury/` | `pakchunk97-ZCOMOpenKitSaberArmoury_P` | Lifts the Armory's lightsaber lock. Required by `armoury-sabers.ini`. |

Everything the native module does, it does without a container. These exist for
the three things a container can reach and a catalogue hook cannot: a property on
a view model, four layout floats across two widgets, and a tag query authored
into a widget's defaults.

## `ui-fit` — the specialization and talent rows

Four floats across four assets, and no tag anywhere:

```text
WBP_FocusTree_SpecializationCards   Specializations.HorizontalEntrySpacing  32 -> 16
WBP_FocusTree_SpecializationCard    SpecializationWidget.WidthOverride      90 -> 80
WBP_FocusTree_SpecializationTalentCards  Talents.HorizontalEntrySpacing     20 -> 12
WBP_FocusTree_SpecializationTalentCard   TalentWidget.Width/HeightOverride  74 -> 66
```

The row has 966 px (a 1042 frame, inset 44 left and 32 right). Stock is eight
specialization cards at 90 with 32 between, which is 944. The module makes it
ten, and 1188 does not fit. Ten at 80 with 16 between is 944 again — the width
the game already gives eight, with the same slack. The talent row is the same
arithmetic: twelve at 66 with 12 between is 924, against the 920 stock uses for
ten.

Width and height move together on the talent card because it is square and the
passive talents draw a circle in it, so shrinking one axis alone would draw
ellipses.

Widening the frame was rejected on measurement: the ability panel to its left is
a fixed 673, the two share one `Overlay`, and they already sit 23 apart at 1920.
Making the list scroll was rejected too — it has no width constraint above it, so
constraining it means inserting a `SizeBox` export rather than editing one.

Retune with `OPENKIT_CARD_WIDTH`, `OPENKIT_CARD_SPACING`, `OPENKIT_TALENT_WIDTH`
and `OPENKIT_TALENT_SPACING`; the builder refuses a combination that still
overflows. Confirmed in game.

**Known limit.** The commander carries 14 talents rather than 12, and 14 at 66/12
is 1080 against 966, so that one screen still stretches. Fixing it costs icon
size for every character to correct one screen, and the count has already moved
10 → 12 → 14 with no ceiling available from the data.

## `both`, `padawan`, `warrior` — the specialization pairing

One asset, `BP_SpecializationSelectionVM`, the focus-tree picker. Its class
default object holds `SpecializationPartMapping`, a `TMap` from primary
specialization to secondary, listing the eight the focus tree offers. The
container appends to it:

```text
both/      + CPD_TacticalSpec_Padawan -> CPD_TacticalSpec_PadawanExtended
           + CPD_TacticalSpec_Warrior -> null
padawan/   + the Padawan entry only
warrior/   + the Warrior entry only
```

The null secondary is correct: no `CPD_TacticalSpec_Warrior_Secondary` exists
anywhere in the game.

No character, faction or roster definition is involved, and no part definition is
edited.

### The `-swap` variants

These additionally retarget `SpecLockedQuery` and `TalentLockedQuery`, whose
single dictionary tag moves from `br.Customization.Part.Character.Info.Name` to
`br.Customization.Slot.Character.Class`.

Every hero kit part carries an `Info.Name` tag — that is how the game finds them
— so the stock query matches and a kit locks once taken. The replacement tag is
carried only by character-class parts, never by the specialization and talent
parts these queries are evaluated against, so nothing matches and the card
behaves normally.

The tag is retargeted rather than the token stream rewritten. The stream stays a
valid ANY-of-one-tag query (`[0,1,1,1,0]`), so nothing depends on having guessed
`FGameplayTagQuery`'s encoding correctly.

This lifts the lock for hero characters too, which is a behaviour change beyond
"what the game offers", and is why it is a separate variant rather than the
default.

## `saber-armoury` — the Armory's lightsaber lock

`WBP_Menu_Armory_WeaponLanding` carries three `FGameplayTagQuery` defaults, all
three identical:

```text
ALL( ALL( BitReactor.Item.UIType ), NONE( BitReactor.Item.UIType.Lightsaber ) )
```

All four lightsaber gear kits carry `BitReactor.Item.UIType.Lightsaber`, so
Change Weapon, Customize Weapon and Modify Weapon are all switched off the moment
a saber is equipped. Nobody can see this in the shipped game: Tel-Rea is the only
saber user and her hilt was never swappable.

`OPENKIT_SABER_ARMORY=all` (the default) lifts all three, which is what opens the
melee gear-kit picker. Lifting `CanChangeWeaponQuery` alone un-greys the list but
shows weapon specializations rather than hilts — the hilts are behind Customize
and Modify.

This ships on its own chunk and is deliberately not bundled with the others: if
that widget fails to construct, the Armory fails completely rather than
degrading.

The builder asserts the stock ten bytes and the exact tag pair before writing,
refusing rather than shipping an override that lifts nothing. Verified by
re-extracting from the mounted container: `CanChangeWeaponQuery` reads
`[0,1,2,1,0]` — `ALL( BitReactor.Item.UIType )` — and the other two are
byte-for-byte stock.

## What these will never contain

- `CPD_Faction_ZeroCompany`, or any other character, faction or roster
  definition. The company's identity is not this mod's business.
- Any asset not actually modified. Shipping a byte-identical override still makes
  the container claim the asset and collide with unrelated mods.

## Rebuilding

```bash
export PATH="$PWD/../lab/upstream/dotnet:$PATH" DOTNET_ROOT="$PWD/../lab/upstream/dotnet"
OPENKIT_TAG_MODE=mapping ../scripts/build-container.sh
```

Needs `retoc`, the .NET 10 SDK, a `.usmap` and an installed copy of the game.
Everything is regenerated from your own install. The built `.pak`, `.ucas` and
`.utoc` files are gitignored and never committed; they reach users through the
release archives. They carry only the definitions Open Kit modifies, and the
builder refuses a byte-identical override.

`mapping` is the only shippable tag mode. The others — `neutral`, `hero`,
`company`, `remove` — all write identity or edit requirements, and are kept only
because the changelog reasons about them.

The build asserts that the packed package path equals the path the asset was
extracted from, then mounts the container against the full shipped set and
confirms the packed asset is the one that comes back. That is a real override test
rather than a self-consistency check — the first containers built here packed to
a flat path, overrode nothing, and still read back perfectly when re-extracted on
their own.

If the game updates, rebuild against the new cooked assets and re-verify with
`retoc verify`.
