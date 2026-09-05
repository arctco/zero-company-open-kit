# Third-party notices

Licence text and required notices. For where each source was read and which
mechanisms derive from it, see [docs/PROVENANCE.md](docs/PROVENANCE.md).

## Sternab's Zero Company Mandalorian Wardrobe

The wardrobe half derives from Sternab's MIT-licensed Zero Company Mandalorian
Wardrobe: the exact part-ID catalogue, the customization catalogue seam, and the
helmet render-fit approach. Copyright (c) 2026 Sternab. Use, modification and
redistribution are granted by the MIT licence included in that project.

**The helmet render fit is Sternab's file, adapted rather than reimplemented.**
`src/ZCOMOpenKit/src/helmet_render_fit.{cpp,hpp}` is their
`helmet_render_fit.{cpp,hpp}`, carrying their mechanism, their mirror of
`FSkinnedMeshSceneProxyDynamicData`, their three render hooks, and the two scale
constants (Man001 1.07, Man002 1.06) that they measured by looking at the game
and that are not derivable from its data. Open Kit's changes are the namespace,
the log prefix, and a data-driven family table in place of two hardcoded
branches. The seven runtime addresses it pins were re-resolved from this build's
own PDB and every one landed on the function upstream names.

**The wardrobe part list.** `src/ZCOMOpenKit/src/dllmain.cpp` carries nineteen part
IDs and the per-part note of which of them require Cly Kullervo's name on top of
the `Accepts.Outfit.Mdo` capability. Both are Sternab's, taken from
`src/dllmain.cpp` in that project. Every one was re-read out of this build's
cooked assets with UAssetAPI before being written here and the data agreed with
upstream on all nineteen, but the list and its per-part flags are theirs and are
the reason this took an afternoon rather than a week. Sixteen of the nineteen are
offered; the three `_PACK` parts are excluded, which is also upstream's call.

**The specializations use it too.** The scoped-requirements
technique in `src/ZCOMOpenKit/src/dllmain.cpp` -- copy the character's
`FGameplayTagContainer`, add to the copy the one tag being deliberately
overridden, put the copy to the game's own
`UCustomizationStatics::DoesPartIdMeetRequirements`, then destroy the copy -- is
Sternab's, as is the shape of the four runtime addresses it needs (the
requirement check plus the container's copy constructor, destructor and
`AddTag`). Those addresses were re-resolved from this build's own PDB and their
signature bytes read out of the shipped executable rather than copied, but the
idea and its structure are theirs.

- <https://github.com/Sternab/ZeroCompanyMandoWardrobe> — read and used at
  commit `d2d3d0b` (v0.4.2).

## Sternab's Zero Company Expanded Colours

`src/ZCOMOpenKit/src/expanded_colours.{cpp,hpp}` is Sternab's `dllmain.cpp` from
the MIT-licensed Zero Company Expanded Colours, adapted rather than
reimplemented: <https://github.com/Sternab/ZeroCompanyExpandedColours>,
copyright (c) 2026 Sternab.

Theirs: the seam (`UCustomizationPartsTagSubsystem::FilterAssetDataByTags`),
the trick of identifying which picker is being filled from what the game already
put in the list, the picker-label override through
`UBitReactorCustomizationPartViewModel::InitializeViewModel`, and both curated
palettes -- 40 outfit colours and 90 weapon paints, with the labels that tell
`Amber 02` from `Amber 13`.

Open Kit's changes are the namespace, the log prefix, and a lifecycle the module
can drive instead of a mod class of its own. Both runtime addresses were
re-resolved from this build's PDB and each landed on the function upstream names.
All 130 asset names were checked against the game's own data before shipping;
every one exists.

The curation was checked rather than taken on trust, and it holds: of the 335
locked outfit colours in the game, 82 are within 0.02 in linear RGB of a colour
the player can already pick and 126 more within 0.05. Upstream's 40 are the ones
worth adding.

## RE-UE4SS

Copyright (c) 2022 Narknon and contributors. MIT License.

- <https://github.com/UE4SS-RE/RE-UE4SS>

The game-specific UE4SS binary is required but is not included or redistributed.

## Game content

The IoStore containers are game-derived: they hold modified copies of the cooked
definitions Open Kit changes, generated from an installed copy of the game. They
are not committed to this repository — they are built by
`scripts/build-container.sh` and included in the release archives. No unmodified
game asset, executable, PDB, save or bulk cooked package is included in the
repository or in a release. See [docs/PROVENANCE.md](docs/PROVENANCE.md).

Star Wars and related marks and assets belong to their respective owners. This
project is not affiliated with Bit Reactor, Electronic Arts, Lucasfilm, Disney,
Nexus Mods or UE4SS.
