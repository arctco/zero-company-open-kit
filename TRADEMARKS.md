# Trademarks and affiliation

Open Kit is an independent community project. It is not affiliated with,
authorized by, sponsored by, or endorsed by Electronic Arts, Lucasfilm Ltd., The
Walt Disney Company, Bit Reactor, Nexus Mods, or the UE4SS project.

**Star Wars** and all related names, characters, marks and assets are the
property of Lucasfilm Ltd. and its affiliates. **Star Wars: Zero Company** is
published by Electronic Arts and developed by Bit Reactor. **Steam** is a
trademark of Valve Corporation. All other trademarks are the property of their
respective owners.

Those names are used here only to identify the game this project modifies and the
in-game content it makes selectable. No claim of ownership is made or implied.

## What this project distributes

Open Kit's releases contain:

- The compiled native module, built from `src/`.
- The option marker files in `options/`.
- IoStore `_P` override containers, generated from an installed copy of the game.

The containers are game-derived: each holds modified copies of the specific
cooked definitions Open Kit changes, generated from an installed copy of the
game, and carries no asset Open Kit does not modify. They are built rather than
stored — this repository contains no game-derived binary at all. **No unmodified
game asset, executable, PDB, save file or bulk cooked package is contained in
this repository or in any release.** The build scripts read the installed game
and write nothing to the game directory.

Open Kit does not unlock, circumvent or bypass any purchase, entitlement or
digital rights measure. Everything it makes selectable is content already present
in an installed copy of the game and already authored by its developers. What
changes is which lists it appears in.

## Character names

Hero names such as Tel-Rea Vokoss, Cly Kullervo, Captain Rex and Trick appear in
this repository's documentation and source. They appear as the identifiers the
game's own data model uses — the values of `Info.Name` gameplay tags read out of
cooked assets — and are necessary to describe how the game gates content. They
are the property of their respective owners.

## Requests

If a rights holder objects to anything here, open an issue or contact the
maintainer and it will be addressed.
