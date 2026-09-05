# Installing Open Kit

This page describes the install as it will ship.

Open Kit is one module with three features — Core, Wardrobe and Armory — plus a
few small optional pak files. You can turn any feature off, but they install
together.

## What you need first

[UE4SS for Zero Company](https://www.nexusmods.com/starwarszerocompany/mods/9),
installed before Open Kit.

All three features need it. Every list the game offers you — the specialization
cards, the wardrobe, the Armory — is built in the game's own code rather than
read from data files, so a pak alone cannot add to any of them. The pak files
Open Kit does ship only adjust things a pak *can* reach: the widget that pairs a
primary specialization with a secondary, two layout numbers, and one tag query.

## With a mod manager (easiest)

Download the **FOMOD** archive and add it to your manager. An installer wizard
asks which features you want and which kits and armour sets, then puts
everything in the right place.

- **ZCOM Mod Manager** — install from the archive; the wizard runs in the app.
- **Vortex** — needs the
  [Star Wars Zero Company extension](https://www.nexusmods.com/site/mods/2174).
  Nexus does not currently offer a "Mod Manager Download" button for this game,
  so download the archive manually and drag it into Vortex's Mods tab.
- **Mod Organizer 2** — install from the archive as usual.

The manager handles removal too, which is tidier than deleting files by hand.
The one thing it cannot do for you: if someone is wearing modded armour, put
them back in stock gear and save *before* you uninstall.

Everything below is for installing by hand from the **Manual** archive.

## The module

1. Close the game.
2. In Steam: right-click **Star Wars: Zero Company** > **Manage** >
   **Browse local files**.
3. Go to `SWZeroCompany\Binaries\Win64`.
4. Drag the `ue4ss` folder from the archive into it and merge.
5. You should end up with
   `...\Win64\ue4ss\Mods\ZCOMOpenKit\dlls\main.dll`.
6. Launch.

To remove: put anyone wearing modded armour or carrying modded gear back into
stock equipment and save first, then close the game and delete
`...\ue4ss\Mods\ZCOMOpenKit`. Do not delete the whole `ue4ss` folder; other mods
live there.

## Turning features on and off

The module reads a folder of small marker files. Each file present turns one
thing on; delete it to turn that thing off. The contents are ignored — an empty
file is fine.

```text
...\ue4ss\Mods\ZCOMOpenKit\options\
    padawan.ini               the Padawan specialization and its talent
    warrior.ini               the Warrior specialization and its talent
    padawan-secondary.ini     Wayseeker, the Padawan second specialization
    padawan-saber.ini         the lightsabers as a weapon class   (off by default)
    wardrobe.ini              the Mandalorian sets
    wardrobe-cly.ini          Cly's own armour
    wardrobe-jedi.ini         Tel-Rea's Jedi robes
    wardrobe-heroes.ini       the other characters' outfits
    wardrobe-anakin.ini       Anakin's outfit
    wardrobe-authored.ini     outfits reserved to authored characters
    wardrobe-helmets.ini      human-shaped helmets on any species
    wardrobe-families.ini     outfit families a class is not granted
    wardrobe-story.ini        outfits the campaign unlocks     (off by default)
    colours.ini               the locked outfit and weapon colours
    colours-extra.ini         95 further outfit colours
    armoury-dc17m.ini         the DC-17M
    armoury-rex-pistol.ini    Rex's twin DC-17s
    armoury-hero-weapons.ini  other characters' weapons
    armoury-sabers.ini        the lightsaber hilts
```

If the `options` folder is missing entirely you get the defaults, so unzipping
only the DLL still gives you a working mod. A file the installed version does
not recognise is ignored rather than treated as an error.

Two are off by default and the reasons differ. `padawan-saber.ini` is off
because of the character-creation issue below. `wardrobe-story.ini` is off
because the campaign hands those outfits out as you play, so turning it on
changes the game's pacing rather than lifting a lock.

## The optional pak files

Copy the ones you want into `SWZeroCompany\Content\Paks\~mods`, creating that
folder if it does not exist. To remove one, delete its three files.

| Files | What it does |
|---|---|
| `pakchunk98-ZCOMOpenKitUI_P` | **Recommended.** Refits the specialization and talent rows so the extra cards fit the frame. Without it the rows overflow. |
| `pakchunk99-ZCOMOpenKit_P` | Optional. Pairs Padawan with Wayseeker in the picker. The `-swap` variants also let a hero kit be swapped off again once taken. |
| `pakchunk97-ZCOMOpenKitSaberArmoury_P` | Needed for `armoury-sabers.ini`. Unlocks the Armory screens the game switches off whenever a lightsaber is equipped. |

## Do not pick the Jedi specialization during character creation

The character creator offers Padawan, and a character created with it cannot
complete the tutorial. The tutorial is fixed to four characters and cannot
recruit, so there is no way out except to start again.

Use the kits on an operator you already have: recruit first, then respec. That
path is tested and works — an existing operator can take a kit, keep it across
save and load, use its abilities through a mission, and respec out of it again.

If you want the option gone from the creator entirely, delete `padawan.ini` and
`warrior.ini`, create your character, then put them back. The module reads
`options` at startup, so this costs one restart each way.

This is the one known defect that can cost a playthrough. It is not a crash and
nothing is corrupted.

## Compatibility

Take care running Open Kit alongside another wardrobe or Armory mod that hooks
the game's customization system. Both reach for the same place, and whether they
coexist depends on how the other one is built — if something is missing from a
picker, try disabling the other mod first. This is also why Open Kit's own
wardrobe and Armory are one module rather than two.

The pak files claim only the specific assets they modify, so they coexist with
anything that does not touch those same assets.

## Uninstalling safely

Open Kit can put operators into states the base game would not have created. If
you remove it while someone is still in one of those states, that character can
come back with a slot the game shows as locked and refuses to let you change.

Before removing:

- Put anyone wearing modded armour or carrying modded gear back into stock
  equipment, and save.
- If you installed a `-swap` pak, put everyone back on a stock specialization,
  talent and weapon, and save.
- If you did not install a `-swap` pak, hero kits stay pinned once taken and
  there is nothing else to do.

Outfits are the safe case and it is worth knowing why. Open Kit adds no assets —
everything it offers already ships with the game, and the mod only stops hiding
it. A save that records a modded outfit is recording stock content, so it loads
fine with the mod gone. The character keeps the look and simply cannot choose it
again.

## "My class is locked" after a game update

This is the same failure arriving without anyone uninstalling anything.

A character holding a hero specialization is only valid while the mod that
unlocked it is applying. If it stops — the mod is removed, or a game update stops
it loading — the game re-checks that character on load, finds a part they do not
qualify for, and shows the slot as locked with no way to change it. The commander
is usually the first character it is noticed on, because he is the one people
give a hero kit to first.

Native UE4SS mods are exposed to this by design: they are pinned to a specific
game build and deliberately refuse to load against an unrecognised one, so a
patch turns the unlock off all at once. Open Kit is a native module and this
applies to it.

What limits the damage:

- Leaving hero kits pinned — the default, and what you get by not installing a
  `-swap` pak — means no operator can ever be holding a part the base game
  disallows in the first place, so the failure has no way to happen.
- If it has already happened: reinstall the mod that was providing the unlock,
  change the affected character back to a stock specialization, save, and only
  then remove it.
