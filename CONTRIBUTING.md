# Contributing to Open Kit

Open Kit is MIT licensed and meant to be reused. Forking it, lifting a seam out
of it, or rebundling it into your own mod needs no permission and no
conversation. This file is for changes you want to land *here*.

## Development setup

You need Linux or WSL, Python 3.11+, and an installed copy of the game. The
Windows toolchain is fetched for you, without root:

```bash
./scripts/setup-native-toolchain.sh    # clang-cl + lld-link via xwin
./scripts/build-native.sh src/ZCOMOpenKit
./scripts/stage-native-mod.sh ZCOMOpenKit
```

Working on containers additionally needs `retoc`, the .NET 10 SDK and a `.usmap`.
See [README.md](README.md#building).

**No game asset, executable, PDB, save or extracted package may be committed.**
The override containers are game-derived and are gitignored: build them with
`scripts/build-container.sh` from your own installation. They reach users through
the release archives, never through the repository.

## The one rule

**Change what the game offers, not who anyone is.**

No contribution may write an `Info.Name.<Hero>` tag onto a character, faction or
roster definition — not in the module, not in a container, not in a generator.
`Info.Name` is a lookup key rather than a restriction, so granting one grants
that hero's whole content: body, animations and gear included.

Capability tags, where a capability is genuinely required, are fine. Identity is
not a capability. The reasoning is in
[README.md](README.md#the-one-rule) and the evidence is in
[docs/INVESTIGATION.md](docs/INVESTIGATION.md).

A change that needs to lift a gate lifts it on a **discarded copy** of the
character's tag container, the way the existing seam does. If you find yourself
needing to modify the real character to make something work, that is the signal
to open an issue rather than a pull request.

## Architecture

One hook over a slot-keyed table. The catalogue hook reads the slot from the
container the game passes and appends the rows registered for that slot, so
**new content should be a row, not a new hook.** If a change adds a hook, the
pull request should say why a row could not do it.

Every candidate part is put to `DoesPartIdMeetRequirements` against the real
character before it is offered. Keep it that way: never append a part
unconditionally, and never widen a lift beyond the single named requirement a
part actually declares. A row whose tag a build does not recognise must disable
itself rather than append blind.

Containers do only what the module cannot. Before adding one, check that the
change genuinely cannot be made in native code — the container surface is the
part that breaks on a game update.

## Generated files

`src/ZCOMOpenKit/src/*.inc` are **generated — do not edit them by hand.** Change
the generator and re-run it:

```bash
./scripts/generate-wardrobe-table.py
./scripts/generate-armoury-table.py
./scripts/generate-colour-table.py
```

Each generator's docstring states the rule behind every row it includes and every
part it excludes. If you change what is included, change that rule and say so in
the docstring, so the table stays auditable against the game rather than trusted.

## Code style

- Match the surrounding code. It is C++ with UE4SS types; keep `STR()` literals
  and the existing hook install/unwind structure.
- Comments explain *why*, and cite the measurement where there was one. A comment
  asserting a fact about the game should say how that fact was established.
- Log fields are fields, not literals — `outfit_palette_size=135`, not a
  hard-coded number in a message. A wrong count must be visible in one line.
- Rows log when their answer changes, not once per call.
- Keep third-party derived files marked as derived, with the upstream URL,
  copyright and licence in the header.

## Testing

There is no automated suite yet. Before opening a pull request:

```bash
./scripts/build-native.sh src/ZCOMOpenKit    # must build clean
./scripts/check-fomod.py                     # if packaging changed
./scripts/check-fomod.py --stage dist/fomod  # if the payload changed
```

Then verify in game, and say so in the pull request. The log is the interface:
read `READY`, then `offered` / `declined` / `pick_asked`. Every behavioural
finding in the changelog was reached that way.

Two things that have cost a test cycle and are worth repeating:

- **Confirm the built binary matches the source.** Grep every new part id, switch
  name, log field and tag out of the built DLL before installing, and check the
  installed hash against the built one. A stale DLL looks exactly like a broken
  change.
- **Confirm a scripted edit landed.** Any string replacement must assert its
  anchor before writing. Two silent no-op replacements shipped in one day before
  that was enforced.

[tests/README.md](tests/README.md) lists what the build already enforces. Adding
real automated tests is the single most useful contribution available — starting
with a test that the containers write no identity tag, which is currently a
matter of care rather than a check.

## Pull requests

Say what the change does for a player, what you measured, and how you verified it
in game — including the build ID you tested against. Keep unrelated formatting out
of the patch.

- A change to what is offered should update the relevant generator, not the
  `.inc`.
- A change to behaviour should add a changelog entry. The changelog is the
  decision record, and it keeps conclusions that were later overturned, because
  the correction is usually the more useful half.
- A change to packaging must pass `check-fomod.py --stage`, and both archives
  must rebuild: `build-fomod.py` then `build-manual.py`.
- Do not hand-maintain the manual archive's layout. `build-manual.py` resolves
  `ModuleConfig.xml`, so a new option or container is added to the installer and
  the manual archive follows. If you find yourself editing both, something has
  been restated that should have been read.
- A finding that contradicts something already written should say so plainly and
  update it. Being wrong in the record is fine; being wrong and quiet is not.

## Reporting a game update that breaks it

The module refuses to hook a build it does not recognise, so the usual symptom of
a game update is that nothing happens at all. Open an issue with the Steam build
ID, the Open Kit version, which features you had on, and the `READY` line from
the log. Do not attach game assets, raw package lists, save files or Steam
credentials.

## Versioning

The version in `VERSION` covers the module and the containers. What it promises
is the **option interface**: the names of the files in `options/`, and the
install layout. Those are what a player's configuration and a manager's
deployment record depend on.

- **Patch** (1.0.x) — fixes, retuned constants, a rebuild against a new game
  build.
- **Minor** (1.x.0) — new content, new option fragments, new containers. Existing
  option files keep working and keep meaning what they meant.
- **Major** (x.0.0) — an option file is renamed or removed, or the install layout
  changes. Anything that makes an existing configuration mean something different.

`src/ZCOMOpenKitProbe` is a diagnostic and versions independently in
`packaging/zcom-mod.probe.json`; it is not part of the mod and does not follow
the release number.

Bump `VERSION`, both manifests in `packaging/` and `packaging/fomod/info.xml`
together, then rebuild both archives so their names follow.

## Licensing

By contributing, you agree to license your contribution under the MIT License
(`MIT`), the same terms as the project — see [LICENSE](LICENSE).

Do not paste in code whose licence you cannot name. Anything adapted from another
project must identify the upstream, its licence, the files affected and any
required notice, both in the pull request and in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Derived files carry their notice
in the file header, not only in the manifest.
