# Provenance

Where Open Kit's material comes from. [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)
carries the licence text and the required notices; this document records the
sources and the method, so a finding can be reproduced and a derived file traced
to its origin.

## Licensed upstream source

### Zero Company Mandalorian Wardrobe

<https://github.com/Sternab/ZeroCompanyMandoWardrobe> — copyright (c) 2026
Sternab, MIT. Read and used at commit `d2d3d0b` (v0.4.2).

Derived from it:

| Mechanism | Where it lives in Open Kit |
|---|---|
| The catalogue append seam | `src/ZCOMOpenKit/src/dllmain.cpp` |
| The scoped-requirements technique — copy the tag container, add the one tag, ask the game, destroy the copy | `src/ZCOMOpenKit/src/dllmain.cpp` |
| Build-identity verification before hooking | `src/ZCOMOpenKit/src/dllmain.cpp` |
| `FPrimaryAssetId` construction for customization parts | `src/ZCOMOpenKit/src/dllmain.cpp` |
| Hook install and unwind structure | `src/ZCOMOpenKit/src/dllmain.cpp` |
| The 19 wardrobe part IDs and their per-part requirement flags | `src/ZCOMOpenKit/src/dllmain.cpp` |
| The helmet render fit, including the Man001 1.07 and Man002 1.06 scale constants | `src/ZCOMOpenKit/src/helmet_render_fit.{cpp,hpp}` |

`helmet_render_fit.{cpp,hpp}` is upstream's file, adapted rather than
reimplemented. Open Kit's changes there are the namespace, the log prefix, and a
data-driven family table in place of two hardcoded branches.

The runtime addresses these mechanisms need were re-resolved from this build's
own PDB, and the 19 part IDs were re-read from this build's cooked assets before
being used. Both agreed with upstream.

### Zero Company Expanded Colours

<https://github.com/Sternab/ZeroCompanyExpandedColours> — copyright (c) 2026
Sternab, MIT.

`src/ZCOMOpenKit/src/expanded_colours.{cpp,hpp}` is upstream's `dllmain.cpp`,
adapted rather than reimplemented. Derived from it: the
`FilterAssetDataByTags` seam, the method of identifying which picker is being
filled from what the game has already placed in the list, the picker-label
override through `InitializeViewModel`, and both curated palettes — 40 outfit
colours and 90 weapon paints, with their labels. Open Kit's changes are the
namespace, the log prefix, and a lifecycle the module drives rather than a mod
class of its own.

### RE-UE4SS

<https://github.com/UE4SS-RE/RE-UE4SS> — copyright (c) 2022 Narknon and
contributors, MIT. Used as the runtime the module loads into and as its type
headers. The game-specific UE4SS binary is required at install time and is not
included or redistributed.

## Zero Company itself

All Zero Company-specific data, addresses, assets, tags and runtime behaviour
used by Open Kit were verified against an installed copy of the game.

| Source | Read with | Used for |
|---|---|---|
| `SWZeroCompany.pdb`, shipped by Steam beside the executable | `llvm-pdbutil`, `llvm-symbolizer`, `scripts/resolve-symbols.sh` | Seam addresses and signatures — [SEAM-MAP.md](SEAM-MAP.md) |
| Cooked assets | `retoc`, UAssetAPI, `scripts/dump-part-tags.py` | Part IDs, gameplay tags, requirement data, the generated tables |
| The shipped executable | Signature bytes read directly | Verifying resolved addresses against the running build |
| Runtime state | UObject inspection, UE4SS logging builds | Enumerator behaviour, slot mapping, tag containers on live characters |
| In-game behaviour | In-game testing | Every behavioural finding in the changelog |

The generated tables in `src/ZCOMOpenKit/src/*.inc` are produced from cooked
assets by the scripts in `scripts/`, and each generator's header states the rule
behind every row it includes and every part it excludes.

Findings are recorded in [INVESTIGATION.md](INVESTIGATION.md) and
[CHANGELOG.md](../CHANGELOG.md), including the conclusions that were later
overturned.

## Compatibility research

Some findings were cross-checked against the shipped pak contents of other mods
for this game, diffed against stock with `retoc`. Those comparisons were used to
observe how other packages modify Zero Company's authored definitions. Any
conclusion Open Kit relies on was then checked against the stock game data or
tested independently. Two mods are named in [INVESTIGATION.md](INVESTIGATION.md)
and [CHANGELOG.md](../CHANGELOG.md) where the specific asset diff is needed to
reproduce a finding.

Open Kit's implementation derives from the licensed sources above, from Zero
Company's own interfaces, and from behaviour verified in game.

## Redistribution

Open Kit's release archives contain the compiled module, the option marker files,
and the IoStore override containers. **Those containers are game-derived**: each
holds modified copies of the specific cooked definitions Open Kit changes,
generated with `retoc` from an installed copy of the game. The builder refuses a
byte-identical override, so a container carries only assets Open Kit actually
modifies.

**The containers are not in this repository.** The built `.pak`, `.ucas` and
`.utoc` files are gitignored; `container/README.md` documents what each one does,
and `scripts/build-container.sh` regenerates them from your own installation.

No unmodified game asset, executable, PDB, save file or bulk cooked package is
included in the repository or in a release. The build scripts read the installed
game and write nothing to the game directory.
