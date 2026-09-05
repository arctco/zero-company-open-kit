# Open Kit

**Open Kit exposes Zero Company's game-authored hero specializations, equipment
and customization to your ordinary operators.**

It is MIT licensed, documented down to the seam addresses, and built as a
reusable native customization framework: one shared catalogue hook over
generated content tables, so adding content is a row rather than another hook.
Nothing is written to any character, faction or roster definition.

To install Open Kit, start with the **[install guide](docs/manual-install.md)**.
This README covers the project itself: how it works, how it is built, and how to
work on it.

---

## Status

**Pre-release.** All three features are built, tested and confirmed in game, and
both release archives — the FOMOD installer and the manual archive — are built
and validated. An ordinary operator can be given the Padawan or Warrior kit, wear
any of 653 catalogue garments, and carry Rex's twin DC-17s or Trick's DC-17m —
with no identity tag written to anyone.

Remaining before release: a release pass over the Nexus copy, and an end-to-end
install and uninstall through a manager. Verified against game build `24874058`;
the module refuses to hook a build it does not recognise.

## Features

Three features in one native module, each independently switchable, 19 option
fragments in total.

| Feature | What it adds | Rows |
|---|---|---|
| **Core** | The Padawan and Warrior specializations, their talents, both lightsaber grips, and optional primary/secondary pairing | — |
| **Wardrobe** | The Mandalorian beskar sets and every other character-locked outfit, plus the locked colour palette | 653 garments, 95 colours |
| **Armory** | The DC-17M, Rex's twin DC-17s, the hero gear kits and the lightsaber hilts | 10 |

With the palette on, the outfit picker offers 135 colours and 90 weapon paints;
95 of those outfit colours are rows Open Kit adds beyond the upstream palette it
builds on.

Every feature is a marker file in `options/`, read at startup. The FOMOD picks a
starting point; it does not own your configuration, and changing a switch never
means reinstalling.

## Known issues

**Do not pick the Jedi specialization during character creation.** The
specialization step in the character creator offers Padawan, and a character
created with it cannot complete the tutorial. The tutorial is hard-coded to four
characters and cannot recruit, so there is no way to recover except to start
again.

Use the kits on an operator you already have — recruit first, then respec. Every
other path works: an existing operator can take a kit, keep it across save and
load, use its abilities in a mission, and respec out of it again.

If you want the creator step gone entirely, delete `options/padawan.ini` and
`options/warrior.ini`, create your character, then put them back. The module
reads `options/` at startup, so this costs one restart each way.

This is the one defect that can cost a playthrough, which is why it leads this
section. It is not a crash and nothing is corrupted. The cause is still open: a
creation pawn carries `Info.Faction.ZeroCompany` and 86 tags against 82-85 for
an ordinary operator, so there is nothing distinguishable to gate on, and the
mechanism behind the softlock itself has not been found.

Smaller, cosmetic, and deliberately not fixed:

- **Some Armory tiles draw the wrong picture.** Rex's DC-17 and every melee hilt
  show another weapon's art, and the model preview says IMAGE MISSING. Those
  parts declare no icon and the game substitutes rather than drawing blank. The
  weapon that equips is correct.
- **The Inquisitor lightsaber can be seen and not selected**, except by authored
  characters such as Tel-Rea and Trick. It is gated on `Accepts.AuthoredOnly`,
  and no tag this mod can add satisfies that — measured, see
  [docs/INVESTIGATION.md](docs/INVESTIGATION.md) §6c.
- **The commander's talent row is one card too wide.** Ordinary operators are
  unaffected.

See [CHANGELOG.md](CHANGELOG.md) for what is settled and
[docs/INVESTIGATION.md](docs/INVESTIGATION.md) for the evidence behind it.

## How it works

Three seams, and the middle one is the whole idea.

**1. One hook, over a slot-keyed table.** The catalogue hook sits after
`GetCustomizationPartDefinitions`. It reads the first tag of the container the
game passes — which is the slot being filled — and appends the rows registered
for that slot. Robes, sabers, wardrobe items, colours and Armory gear are all
rows in one table rather than separate hooks. Because the slot arrives as data,
a table reaches every list in the game through a single seam. That is what makes
this a framework rather than a set of patches: **new content is a row.**

**2. The module asks the game before it offers anything, on a copy.** Every
candidate is put to `UCustomizationStatics::DoesPartIdMeetRequirements` against
the real character, and a "no" means the part is not offered.

The trick is what to ask. These parts are gated on a hero's `Info.Name`, and an
ordinary operator has none — that gate is the entire thing this mod exists to
lift, so asking unmodified would answer "no" every time. So the required name
goes onto a **copy** of the character's tag container, the copy is what gets
asked, and the copy is destroyed before the answer is returned. Nothing is
written to the character. The game is never told anyone is Tel-Rea or Cly; it is
asked a hypothetical, and the answer is thrown away with the copy.

What that buys is precision. The module lifts exactly one requirement, by name,
and every other requirement a part declares — species, body rig, slot capability,
roster level, anything a future part brings — is still answered by the game
against the real character. An Astromech is still refused Mandalorian armour, as
authored, and the mod never learns it happened.

Which hero each part requires was read out of the cooked assets rather than
assumed. A row whose hero tag a build does not recognise disables itself rather
than falling back to appending blind.

**3. A tag-subsystem seam** (`FilterAssetDataByTags`) for the lists that are
assembled by asset query rather than by the catalogue call.

**All three features need UE4SS.** Every list the player picks from is assembled
in native code behind `GetCustomizationPartDefinitions` — the wardrobe, the
Armory and the specialization cards alike. A part that is not appended there is
never offered, however permissive its authored data. The eight containers that
ship alongside do only the things a container can do that the module cannot:
pair a primary specialization with a secondary, retune two widget floats, and
lift a tag query authored into a widget.

**The three features ship as one module, not three.** Two DLLs hooking the same
customization functions compete for the same seam, and whether they coexist
depends on both implementations. One module with every feature behind option
fragments cannot conflict with itself, and it halves the hooking surface.

## The one rule

**Change what the game offers, not who anyone is.**

Never write an `Info.Name.<Hero>` tag onto a character, faction or roster
definition. `Info.Name` is a lookup key rather than a restriction: parts are
found *via* the name tags a character carries, so granting one grants that
hero's whole content — body, animations and gear included. There is no inert
hero name, because a hero name is precisely what is not inert.

Granting names is the only route available to a mod that ships data alone, and
it works. It costs whatever else keys on that identity. Open Kit pays a different
price instead: it needs UE4SS, and it needs a hook.

Capability tags, where a capability is genuinely required, are fine. Identity is
not a capability. This rule holds whichever mechanism turns out to gate a given
part, which is why it is the rule and not the mechanism.

## Installation and documentation

| | |
|---|---|
| **Installing, for players** | [docs/manual-install.md](docs/manual-install.md) |
| **What was measured, and how** | [docs/INVESTIGATION.md](docs/INVESTIGATION.md) |
| **Seam addresses and signatures** | [docs/SEAM-MAP.md](docs/SEAM-MAP.md) |
| **What each container does, and why** | [container/README.md](container/README.md) |
| **What is verified, and what is not** | [tests/README.md](tests/README.md) |
| **Where the sources come from** | [docs/PROVENANCE.md](docs/PROVENANCE.md) |
| **Decision record** | [CHANGELOG.md](CHANGELOG.md) |
| **Contributing** | [CONTRIBUTING.md](CONTRIBUTING.md) |

## Architecture

```text
src/ZCOMOpenKit/           the native module — all three features
src/ZCOMOpenKitProbe/      Lua diagnostic: reads the picker's CDO back out
src/ZCOMOpenKitSeamProbe/  C++ diagnostic: observation-only seam instrumentation
container/                 the IoStore _P overrides
scripts/                   investigation tools, generators, build and validation
docs/                      investigation, seam map, player install guide
packaging/                 Nexus copy, FOMOD installer, ZCOM Mod Manager manifests
tests/                     test notes
dist/                      build output, not tracked
```

The catalogue tables in `src/ZCOMOpenKit/src/*.inc` are **generated, not written
by hand**. Each generator's header carries the rule behind every row it includes
and every part it excludes, so a table can be audited against the game rather
than trusted:

```bash
./scripts/generate-wardrobe-table.py   # -> src/ZCOMOpenKit/src/wardrobe_table.inc
./scripts/generate-armoury-table.py    # -> src/ZCOMOpenKit/src/armoury_table.inc
./scripts/generate-colour-table.py     # -> src/ZCOMOpenKit/src/colour_table.inc
```

FOMOD chooses which files land. The `options/` fragments choose how the module
behaves at runtime. Neither duplicates the other, so runtime toggles never become
install options a user has to reinstall to change.

## Building

```bash
./scripts/setup-native-toolchain.sh              # clang-cl + xwin, no root needed
./scripts/build-native.sh src/ZCOMOpenKit
./scripts/stage-native-mod.sh ZCOMOpenKit        # -> dist/ZCOMOpenKit.zip
```

The module cross-builds on Linux with `clang-cl` and `lld-link` against an
`xwin`-fetched MSVC CRT and Windows SDK. Two details cost an hour if missed:
`-fuse-ld=lld` is required, and `/winsysroot` does not work against an xwin tree.

Containers need `retoc`, the .NET 10 SDK, a `.usmap` and an installed copy of the
game:

```bash
export PATH="$PWD/../lab/upstream/dotnet:$PATH" DOTNET_ROOT="$PWD/../lab/upstream/dotnet"
OPENKIT_TAG_MODE=mapping ./scripts/build-container.sh
```

You build the containers from your own installation. They are **not committed to
this repository** — the built `.pak`, `.ucas` and `.utoc` files are gitignored,
because they are game-derived. They are included in the release archives, where
they carry only the definitions Open Kit modifies. No unmodified game asset,
executable, PDB or save is included anywhere.

### Packaging

```bash
./scripts/build-fomod.py            # -> dist/ZCOM-Open-Kit-v<version>-FOMOD.zip
./scripts/build-manual.py           # -> dist/ZCOM-Open-Kit-v<version>-Manual.zip
./scripts/check-fomod.py --stage dist/fomod
```

Two archives are built from the one source tree: a **FOMOD** for ZCOM Mod
Manager, Vortex and Mod Organizer 2, and a **Manual** archive with the final
layout already resolved. They exist separately because a FOMOD stores files under
option folders, so extracting one by hand produces the wrong layout — one archive
cannot honestly be both.

The FOMOD deliberately uses only the element subset all three managers implement,
so it behaves the same in each. `check-fomod.py` enforces that and blocks the
release build. `build-fomod-preview.py` packages the same installer against inert
placeholders, so the wizard can be walked in any manager without anything
reaching the game.

`build-manual.py` does not restate the layout. It reads `ModuleConfig.xml`,
replays the answers a player would give — the installer's own recommended ones by
default — evaluates its flag conditions, and copies what the installer would have
copied, from the same `dist/fomod` stage. So the two archives cannot carry
different payloads or install to different places, and a change to the installer
reaches the manual archive without anyone remembering to make it twice. Pass
`--all` for every switch, or `--core`, `--swap` and `--features` for a different
resolved configuration.

### Reproducing the lock analysis

```bash
./scripts/dump-part-tags.py CPD_TacticalSpec_Warrior CPD_TacticalSpec_Padawan
./scripts/dump-part-tags.py --diff CPD_TacticalSpec_Warrior CPD_TacticalSpec_Soldier
./scripts/resolve-symbols.sh 'UCustomizationStatics'
```

The first two read the installed game with `retoc`; the third reads the PDB the
game ships. Nothing is written to the game directory.

## Contributing and testing

Contributions are welcome — see **[CONTRIBUTING.md](CONTRIBUTING.md)** for the
setup, the house rules and what a change needs before it lands.

There is no automated test suite yet. What verification exists is built into the
build tools, each check added in response to a specific failure that shipped, and
[tests/README.md](tests/README.md) lists them along with the in-game checks no
test can replace. Adding real tests is the most useful thing an outside
contributor could do.

## Licence

MIT, in both directions. Open Kit may be forked, rebundled, or lifted wholesale
into another mod without asking. See [LICENSE](LICENSE).

The wardrobe and the colour palettes build on Sternab's MIT-licensed
[Zero Company Mandalorian Wardrobe](https://github.com/Sternab/ZeroCompanyMandoWardrobe)
and [Zero Company Expanded Colours](https://github.com/Sternab/ZeroCompanyExpandedColours),
adapted with attribution rather than reimplemented. If the wardrobe is all you
need, use those directly — they are MIT too, and they are the better starting
point for that alone. Full notices in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Disclaimer

This is an independent community project. It is not affiliated with or endorsed
by Electronic Arts, Lucasfilm, Disney, Bit Reactor, Nexus Mods or UE4SS. Star
Wars and related names and assets are the property of their respective owners.
See [TRADEMARKS.md](TRADEMARKS.md).
