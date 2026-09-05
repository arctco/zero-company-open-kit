# Investigation

How the game locks the things this mod unlocks, and the evidence behind each
claim. Everything here was derived from the local game install — its cooked
assets, the PDB it ships, and runtime observation — plus public MIT source.

Companion document: [SEAM-MAP.md](SEAM-MAP.md), the customization,
specialization, animation and save systems named and addressed from the PDB.

**Sections marked *superseded* record reasoning that turned out to be wrong.**
They are kept because the correction is usually the more useful half, and because
several of them are the evidence behind the project's one rule. The changelog
carries the current state throughout.

## 1. There are two seams, not one

The original design assumed the mod ecosystem split cleanly into DLLs and paks:

| Surface | Seam | Assumed consequence |
|---|---|---|
| Wardrobe picker catalogue | Native: `GetCustomizationPartDefinitions` (append), `DoesPartMeetRequirements` (gate) | No pak can add a wardrobe tile. Every wardrobe mod is a UE4SS DLL. |
| Class / spec / talent / weapon | Authored data on `CPD_*` assets | Reachable with a plain IoStore `_P` override. No UE4SS. |

The first row holds. The second does not. Specializations are enumerated by the
same native catalogue as the wardrobe, so a pak cannot add to that list either —
see §8, and the *Superseded* section of the changelog for how three containers
were shipped on the gap. The whole mod is one native module; the containers that
remain do only the things a container can genuinely reach.

## 2. How the game locks a specialization

Locks are expressed as gameplay tags on the *part* definitions under
`/Game/Game/Customizations/Characters/Common/Specialization/`. Extracted with
`retoc` and dumped with `scripts/dump-part-tags.py`:

```text
CPD_TacticalSpec_Warrior        Part.Character.Info.Name.ClyKullervo
                                Part.Character.Specializations.Tactical.Warrior
                                Slot.Character.Specializations.Tactical.Primary

CPD_TacticalSpec_Soldier        Part.Character.Specializations.Tactical.Soldier
                                Part.Character.Species.Astromech
                                Slot.Character.Specializations.Tactical.Primary

CPD_TacticalSpec_Padawan        Part.Character.Info.Name.Tel-ReaVokoss
CPD_TalentSpec_TheMandalorian   Part.Character.Info.Name.ClyKullervo
CPD_TalentSpec_TheLostPadawan   Part.Character.Info.Name.Tel-ReaVokoss
CPD_WeaponSpec_Melee_2H_TelRea  Part.Character.Info.Name.Tel-ReaVokoss

CPD_TalentSpec_TheToughGuy      (no Info.Name tag)
```

The five hero-locked parts each name their hero. The freely available ones do
not. That is the lock.

The character classes carry a separate gate, `br.Customization.Accepts.AuthoredOnly`,
present on `CPD_Char_Class_Hero_Padawan`, `_Mando` and `_Humanoid` alike, and
the Padawan class is additionally categorised `Part.Character.Class.Exotic.Padawan`
where Mando and Humanoid are `Class.Default`.

### `Info.Name` grants, it does not restrict

This was open until the game's own data settled it. Two independent
confirmations:

1. `CPD_WeaponSpec_Blaster_Rifle` and `CPD_WeaponSpec_Blaster_Pistol` — the
   ordinary weapons every operator uses — both carry `Info.Name.ClyKullervo`
   **and** `Info.Name.KabbUppercut`. If those tags restricted, nobody but Cly and
   Kabb could hold a rifle.
2. ZZCArmoryAddon (Nexus 58) makes Rex's pistol usable by *adding*
   `Info.Name.CaptainRex` to `CPD_WeaponSpec_Blaster_Pistol_Rex`, and leaves that
   asset's `Accepts.AuthoredOnly` in place. It grants entry rather than removing
   the gate.

So the model is:

- **`br.Customization.Accepts.AuthoredOnly`** — the gate. Only authored
  characters may take this part.
- **`br.Customization.Part.Character.Info.Name.<Hero>`** — the allowlist of who
  gets in anyway. Additive.

### The picker carries an explicit list, and the heroes are not on it

`BP_SpecializationSelectionVM` holds an array of **(primary, secondary)
specialization pairs** — the specializations the focus tree offers. Stock, it has
exactly eight entries, and the picker's import table names exactly eight
primaries with their secondaries:

```text
Assault  Gunslinger  Heavy  Medic  Scoundrel  Scout  Sniper  Soldier
```

Warrior and Padawan are simply absent. That is the lock: not a predicate that
rejects them, just a list they are not on.

The array is at `0x844` in the stock `.uexp` as an `int32` count of `8` followed
by sixteen `FPackageIndex` values — eight pairs. Classes Unlocked changes the
count to `10` and appends four more indices, a delta of exactly the 16 bytes by
which its `.uexp` is larger:

```text
stock  0x840: 0000 0000 0800 0000 f7ff ffff eeff ffff ...
mod    0x840: 0000 0000 0a00 0000 f7ff ffff eeff ffff ...
mod    0x888:                     77ff ffff 76ff ffff   (Padawan, PadawanExtended)
mod    0x890: 75ff ffff 0000 0000                       (Warrior, null)
```

Re-extracted with the full game containers present, its import table resolves
those to `CPD_TacticalSpec_Padawan`, `CPD_TacticalSpec_PadawanExtended` and
`CPD_TacticalSpec_Warrior`. The null second element is correct rather than a
mistake: there is no `CPD_TacticalSpec_Warrior_Secondary` anywhere in the game,
so Warrior genuinely has no secondary specialization.

### About the tag query

The same asset also carries two `FGameplayTagQuery` fields described as
`ANY( br.Customization.Part.Character.Info.Name )`, applied to the
Tactical.Primary, Tactical.Secondary and Talent slots. Classes Unlocked leaves
their token streams alone.

The previous revision of this document called those queries the gate. That was
wrong — they were the most visible thing near the change, not the change. They
presumably still filter something (marking hero parts, or gating a broader pool
elsewhere), but they are not what keeps Warrior and Padawan out of the list, and
Core does not need to touch them.

### What this means for Core

Appending to that array is necessary and is not sufficient. It is what makes the
picker *pair* a primary specialization with a secondary, and that is the one
edit that survives into the shipped design, as `container/{both,padawan,warrior}`:

```text
both      + (Padawan, PadawanExtended) + (Warrior, null)
padawan   + (Padawan, PadawanExtended)
warrior   + (Warrior, null)
```

What it does not do is make the cards appear. The list the focus tree draws is
assembled in native code before the mapping is consulted — §8 — so the module
appends the parts at the catalogue seam and the container only pairs them. The
container touches no character, faction or part definition, and no tag query.

## 3. The identity route, and what it costs

This section documents the mechanism that the project's one rule exists to avoid,
using the mod that demonstrates it. It is here because it is *evidence* — the
clearest available demonstration that `Info.Name` propagates a whole character —
and not as a criticism. Granting identity is the only route a data-only pak has.
It works, its author documents the consequences, and Open Kit avoids those
consequences by paying a different price: it needs UE4SS and a hook.

Read as shipped data only: the pak contents diffed against stock with `retoc`.

Classes Unlocked (Nexus 39) is two `_P` containers and seven packages, of which
`DT_CostsTable_v4` and `STRUCT_FX_DS_Destroy` are byte-identical to stock —
inert, but still enough to make the container claim ownership and collide with
any other mod that touches them. The five real changes are:

- `CPD_Faction_ZeroCompany` — tag array grows 1 → 4
- `CPD_H_Outfit_{Cly,ClyB,Man001A,Man002A}_HELM` — one array element each
- `BP_SpecializationSelectionVM` — the focus-tree picker

Stock, the faction definition carries exactly one tag,
`Part.Character.Info.Faction.ZeroCompany`. The mod adds three:

```text
br.Customization.Part.Character.Info.Name.Tel-ReaVokoss
br.Customization.Part.Character.Info.Name.ClyKullervo
br.Customization.Accepts.Outfit.Mdo
```

Two of those are *identity* tags, applied to the faction definition that covers
every character in the company. After installing, every recruit asserts it is
Tel-Rea Vokoss **and** Cly Kullervo simultaneously.

Everything downstream that keys on hero identity then fires for everyone, and
each observed behaviour follows from a tag rather than from anything going
wrong. Because every character carries `Info.Name.Tel-ReaVokoss`, all melee uses
the lightsaber animation set. Because every character carries
`Info.Name.ClyKullervo`, soldiers get the Mandalorian arm cannon in place of the
rocket launcher. And because Hawks holds hero identity from character one, the
tutorial cannot be completed with a lightsaber taken.

That correspondence is the finding worth keeping: it is the strongest available
evidence that an `Info.Name` tag carries a whole character with it, which §8
later confirms from the game's own side — identity is a customization *slot*, and
a name is the part equipped in it.

The third added tag, `br.Customization.Accepts.Outfit.Mdo`, is a *capability*
tag and is the right shape — it is what the Mando class itself carries.

## 4. The thesis

**Change what the game offers, not who anyone is.**

Never write an `Info.Name.<Hero>` tag onto a character, faction or roster
definition. Leave identity alone and nothing keyed on "is this Tel-Rea" changes
behaviour, so the animation, weapon and tutorial regressions have no mechanism by
which to occur.

The rule was deliberately stated in a form that does not depend on which
mechanism turns out to be the gate, and that was worth doing: the mechanism was
identified wrongly three times — the parts, then the picker's tag queries, then
the picker's mapping array — before §8 settled it. The rule held through all
three.

An earlier draft stated it as "edit the parts, not the characters", which named a
mechanism rather than a principle. Identity is what was actually load-bearing.
§8 explains why, from the game's side: identity is a customization slot, so a
name is not a label but the part equipped in that slot, and everything hanging
off that character follows it.

Granting capability tags where a capability is genuinely required is fine.
Granting identity tags is what this project does not do.

## 5. Reusable upstream

`https://github.com/Sternab/ZeroCompanyMandoWardrobe` — MIT, full buildable C++,
carrying the 19 exact part IDs, the catalogue seam and the helmet render-fit
maths. The wardrobe half builds on this with attribution rather than
reimplementing it. See THIRD_PARTY_NOTICES.md.

Upstream pins eight hardcoded RVAs to Steam build 24874058 and byte-verifies each
before hooking. Open Kit does the same thing with fifteen, all resolved from the
game's own PDB. That is safe and it breaks on every game patch; signature
scanning would survive updates and is not built.

## 6. The Armory

ZZCArmoryAddon (Nexus 58) adds gear the Armory does not normally offer: Clone
Commando armour, Captain Rex's armour, the DC-17M, and Rex's pistol as its own
category. It ships as a pak plus a 977 KB DLL — the pak carries the data edits,
the DLL injects the Armory lists.

Its pak modifies exactly two assets, and the diff against stock is instructive:

```text
CPD_WeaponSpec_Blaster_Pistol_Rex   + Info.Name.CaptainRex
                                    + Specializations.Weapon.Blaster.DC17Rex
CPD_GK_Pistol_DC-17_Rex             + Specializations.Weapon.Blaster.DC17Rex
```

The second added tag is a **new specialization category** the base game does not
have, which is how Rex's pistol becomes its own Armory list rather than joining
the generic pistols.

### Rex's pistol is not regrouped by the game

The paragraph that used to end this section said Open Kit would offer Rex's
pistol behind its own switch *because* it regroups the weapon lists. That was
reading the addon's edit as if it were a property of the game. It is not.

Read from the game's own cooked assets, `CPD_WeaponSpec_Blaster_Pistol_Rex`:

| | |
|---|---|
| slot | `Slot.Character.Specializations.Weapon` |
| requires | `Accepts.AuthoredOnly` -- **and nothing else** |
| grants | `Specializations.Weapon.Blaster.Pistol`, the **generic** category |
| DefaultPart | `CPD_GK_Pistol_DC-17`, the stock model |

So in the shipped game Rex's pistol class is an ordinary pistol class carrying
his own abilities (`GA_StandardShot_BlasterPistol_Rex`, `GA_Overwatch_Pistol_Rex`,
`GA_DualFire`, `GA_GunDown`) and his own attributes. It joins the pistol list;
nothing is regrouped. It is gated on `AuthoredOnly`, which this project has
already proven liftable, and **not** on Rex's name.

`CPD_GK_Pistol_DC-17_Rex` is the part that wants `Info.Name.CaptainRex`, and it
is a gear kit -- his own `GK_Pistol_DC-17_Rex` model.

Open Kit therefore reaches both without inventing a tag and without writing an
identity. Rex still gets his own switch, but for a different and smaller reason:
his class shares the stock pistol class's `DisplayName` and his kit shares the
stock DC-17's, so both read alike on screen.

## 6b. The Armory, measured

Every `CPD_WeaponSpec_*`, `CPD_GK_*`, `CPD_GearKit_*` and `CPD_WP_*` in the game
was extracted with `retoc to-legacy --filter CPD_` and dumped with UAssetAPI.
208 parts. `scripts/generate-armoury-table.py` is the executable form of what
follows.

### Clone Commando and Rex's armour arrive with the wardrobe

The Armory was planned as Clone Commando armour, Rex's armour, the DC-17M and
Rex's pistol. The first two are **outfits**, and the wardrobe swept every outfit
in the game:

| family | parts | where |
|---|---|---|
| `CPD_H_Outfit_Clo010_*` -- Clone Commando (`Accepts.Outfit.CloCommando`) | 8 | `wardrobe_table.inc`, `wardrobe-authored.ini` |
| `CPD_H_Outfit_CaptainRexA_*` -- Rex's armour | 5 of 6 | same |

The sixth is `CaptainRexA_PACK`, absent under the backpack rule rather than by
oversight. So the Armory is weapons.

### It is three slots, not one

```text
Slot.Character.Specializations.Weapon           the weapon CLASS
Slot.Character.Specializations.Weapon.GearKit   the weapon MODEL carried
Slot.Weapon.Type                                the model a weapon is skinned as
```

A weapon class grants a category tag; every gear kit in that category *requires*
it. **That is not a lock and it is not lifted.** It is how the game keeps rifles
out of the pistol list, and a character earns it by equipping the class.

### The scripted authoring family does not reach the Armory

Recorded as the single biggest risk to this work, because
`Accepts.Character.Specializations` is what Anakin, Rex and every enemy tactical
spec require, and building around it cost a class once. Across all 208 parts in
these three slots the tag **does not appear once**. The census of every
requirement they do declare:

```text
110  Accepts.AuthoredOnly
  1  Accepts.Unlocks.DC17M
 14  Part.Character.Class.{Battledroid.*,Imperial.Grenadier}   NOT lifted -- an NPC's class
  5  Part.Character.Info.Name.{Anakin,CaptainRex,ClyKullervo,Tel-ReaVokoss,Trick}
 21  Part.Character.Rig.{Humanoid,Astromech}                   the game's call
 34  Part.Character.Specializations.Weapon.*                   the category, above
 25  Part.Weapon.Type.*                                        which weapon a model fits
```

### `Accepts.Unlocks.DC17M` is a gate nothing satisfies

`CPD_WeaponSpec_Blaster_DC-17M` -- Trick's clone-commando rifle -- requires
`Info.Name.Trick` **and** `Accepts.Unlocks.DC17M`. Grepping all 4,415 extracted
`CPD_*` files, that tag appears in exactly one asset: the part that requires it.
No class grants it, no character carries it, and unlike `Unlocks.Story_*` and
`Unlocks.Mission*` -- which `CPD_Char_Class_NPC_Humanoid` does grant -- nothing
in the customization system hands it out. It is `AuthoredOnly` under another
name, and it is lifted the same way.

The weapon itself is complete: three fire modes (`GK_Longarm_DC-17M`,
`GK_Repeater_DC-17M`, `GK_Launcher_DC-17M`), its own ability set, attributes,
animations, UI icon and weapon-customization model (`CPD_WP_Type_Exotic_DC-17m`,
reached through `CPD_Weapon_Exotic`, which grants `Weapon.Type.2HRifle.Exotic`).

### What a finished player weapon looks like

Read off the stock unlocked kits, every one of which has all four: a
`DisplayName`, both images, at least one ability, and a `Part.Weapon.Type.*` tag.
That is the yardstick the generator's exclusions are measured against, and each
exclusion is a part failing it in a way that would show on screen -- a blank
tile, a weapon class that draws no weapon, a gear kit with no category that
appears in every list, or an `_Enemy` twin of a gun already offered.

Four offered parts carry a name and no icon (`CPD_GK_Pistol_Hawks`,
`CPD_GK_Pistol_DC-17_Rex`, `CPD_WP_Type_Pistol_Hawks`, `CPD_WP_Type_Pistol_S-5`).
Unversioned serialization omits a property at its default, so that is an empty
brush, not a parse failure. Reported, not hidden.

### A cross-check that does not survive the Armory

"Drop a part whose `DisplayName` belongs to another part" is the rule that caught
thirteen mis-slotted outfit parts. Run against the Armory it threw out eleven,
and reading them says why: **a gear kit and its weapon model share a DisplayName
by design** -- `CPD_GK_Pistol_Cly` and `CPD_WP_Type_Pistol_Cly` are both "434",
and twenty pairs do this. Narrowed to collisions within one slot it then catches
nothing the other rules miss.

So it is reported as a caveat rather than acted on. What it finds is real and
worth knowing: Rex's class is called "Blaster Pistol" exactly like the stock
class, and his kit is called "DC-17" exactly like the stock kit.

The generalisable point, and it is the second time this project has hit it: a
rule that is right about one family of parts is not thereby right about another.
The wardrobe rule was checked against outfits and shipped; the same rule against
weapons is a false positive eleven times out of eleven.

### The names, resolved from the string table

Asked whether Rex's pistol has a proper name of its own the way the DC-17M does.
It does not, and the string table settles it --
`/Game/Game/UI/Localization/Design_WeaponSpecs_Strings`:

| part | key | resolves to |
|---|---|---|
| `CPD_WeaponSpec_Blaster_DC-17M` | `Blaster_DC17m_Header` | **DC-17m Weapon System** |
| `CPD_WeaponSpec_Blaster_Pistol_Rex` | `BlasterPistol_Header` | **Blaster Pistol** -- the stock class's key |
| `CPD_GK_Pistol_DC-17_Rex` | `WP_Model_DC17_Name` | **DC-17** -- the stock kit's key |
| `CPD_GK_Pistol_Cly` | `WP_Model_434_Name` | Model 434 |
| `CPD_GK_Pistol_Hawks` | `WP_Model_Hawks_Name` | Hawks Custom |
| `CPD_WP_Type_Pistol_S-5` | `WP_Model_S5_Name` | S-5 |
| `CPD_WP_Type_Rifle_E-5` | `WP_Model_E5_Name` | E-5 |

Nothing anywhere in that table mentions Rex or Captain. So "DC-17m Weapon
System" is the game's own authored name working correctly, and Rex's two parts
genuinely borrow the stock keys -- consistent with assets that were never meant
to reach a player. The module's picker labels are the only names they will have.

`CPD_WeaponSpec_Melee_1H_Anakin` has no `DisplayName` at all and the picker still
identifies it, so **an empty DisplayName falls back to the asset name** rather
than drawing blank. That weakens "no DisplayName is a blank tile" further than
the per-category narrowing already did; it is a *poor* label, not a missing one.

### `Accepts.AuthoredOnly` cannot be lifted by scoping the tag

**This corrects the most load-bearing claim this project has made about that
tag.** The changelog and the wardrobe generator both say `AuthoredOnly` is
"liftable, proven in game via Anakin's outfit". Measured directly at the
requirement seam, it is not.

A controlled comparison in game found it first: with the two-handed Padawan
lightsaber equipped, **Tel-Rea and Trick can select the Inquisitor hilt and an
ordinary operator cannot**. Same weapon class, same category, different
characters. A differential probe was added to settle it -- the requirement is put
to the game twice per ask, once as shipped and once with the category added as
well, the second answer recorded and discarded:

```text
pick_asked part=CPD_GK_Lightsaber_Enemy_Imperial_Trilla answer=false
    character_has_melee_2h=true character_tags=85 answer_if_category_scoped=false
```

So the character **has** the category, adding it again changes nothing, and
`Accepts.AuthoredOnly` is already scoped onto the copy. The game still refuses.

`AuthoredOnly` is therefore **not a capability a character can be handed**. The
requirement check reads something about the character that adding a tag cannot
fake -- which is exactly what the name says: assigned by authoring. Consistent
with the sweeps, which found nothing granting it in **3,363 customization parts**
or **793 `CD_Char_*` character definitions**.

### Corrected: the wardrobe is fine, and the seam is the difference

The paragraph above claimed 59 wardrobe rows were probably inert. **They are
not.** Opening the wardrobe and reading the log settles it: of the 59 rows whose
only lift is `AuthoredOnly`, **14 were offered and none declined** --
`CPD_H_Outfit_CaptainRexA_TORS` among them, whose registry tags are exactly
`AuthoredOnly` + `Rig.Humanoid` + slot.

So the same function, `guarded_meets_requirements`, with the same tag scoped,
returns **true** at the catalogue seam for an outfit and **false** at the
requirement seam for a saber hilt. The tag is liftable. The seams disagree.

That moves the suspicion from the tag to the **container**. §8 measured that the
catalogue seam is called with the slot tag as the container's *first entry*,
ahead of the character's own tags; the pick path may pass the character's tags
alone. If `DoesPartIdMeetRequirements` needs the slot tag present to match a
part's `AllowedSlots`, every catalogue-seam success and every pick-seam failure
follows without `AuthoredOnly` being special at all.

### No tag this module can add makes an operator eligible

Three differential probes, all at the requirement seam, all on a character who
carries `Melee.2H` and 82-85 tags:

| what was added to the discarded copy | answer |
|---|---|
| `Accepts.AuthoredOnly` (as shipped) | **false** |
| + `Specializations.Weapon.Melee.2H` | **false** |
| + `Info.Name.Tel-ReaVokoss` | **false** |
| + the part's own slot tag | **false** |

The slot probe is the control that makes the rest trustworthy: adding the slot
tag turned *passing* parts false -- `CPD_TacticalSpec_Padawan` and
`CPD_WeaponSpec_Melee_2H_TelRea` both flip -- so the scoping mechanism provably
changes answers, and a `false` everywhere else is a real refusal rather than an
inert copy.

So `guarded_meets_requirements` cannot make an ordinary operator eligible for
either saber hilt, by any tag combination available to it. The player's original
reading stands: it works on Tel-Rea and Trick, who are authored, and on nobody
else.

**What is still not explained** is why the catalogue seam accepts
`AuthoredOnly`-only outfits for the same operator while the requirement seam
refuses `AuthoredOnly`-only hilts. Both call the same function with the same kind
of copy. That asymmetry is unexplained and should not be papered over.

### The one route left, and it is a policy decision

Scoping adds a tag to the *character*; it cannot work if the check reads
authored-ness from somewhere other than the tag container. **Removing the
requirement from the *part* is a different operation and would work** -- and the
container builder already has `EditPart`, which does exactly that for hero names.

It is not free of doubt: `GetAllowedSlotsForPartAssetData` takes an `FAssetData`,
so the requirement the game reads lives in `CustomizationPartTags` inside
`AssetRegistry.bin`, which a `_P` container does not override. Whether editing
the `.uasset` is enough is untested.

And it is against the project's stated rule as written -- `OPENKIT_TAG_MODE`'s
`remove` mode is listed among the "superseded experiments that write identity or
edit requirements, which is what this project exists not to do". The argument for
an exception is that deleting `AuthoredOnly` from one gear kit grants nobody an
identity and pulls no character's content onto anyone: it makes one hilt
choosable and nothing else. That is a decision for the project owner, not one to
take quietly.

**Two lessons, and the second is the expensive one.** The registry is the right
place to read these requirements -- `GetAllowedSlotsForPartAssetData` and
`GetCustomizationDisallowedPartTagsForPartAssetData` both take `FAssetData`, so
`CustomizationPartTags` and `CustomizationDisallowedPartTags` in
`AssetRegistry.bin` are what the game consults, not the export data this project
had been reading. And a claim was corrected here twice in one hour, in opposite
directions, because each correction was written from the first evidence that fit
rather than from evidence that could distinguish the alternatives.

The original "proof" does not survive re-reading. It was that Anakin's arms,
boots and legs were *offered* after scoping `AuthoredOnly`. But
`CPD_H_Outfit_Anakin_ARMS` also requires `Height.Tall`,
`Body.Type.Masculine`, `Weight.Average` and `Rig.Humanoid` -- all of which a tall
masculine human operator genuinely carries -- so an offer proves those four were
satisfied, not that the fifth was. And `CPD_H_Outfit_Anakin_TORS`, the part most
often cited alongside them, scopes `kAnakinName` and not `AuthoredOnly` at all.

**Not yet settled**: whether an ordinary operator can actually *wear* a part whose
only lift is `AuthoredOnly`. One such part is present in the current save
(`CPD_H_Outfit_AurelioA_HELM`), but Aurelio is an authored character and may
simply be wearing his own. The test is to put one on a recruit and read
`pick_asked`.

### The vibrosword is cut, not locked

Three separate names point at it, and none of them delivers it.

| asset | where it lives |
|---|---|
| `UtilityItem_Vibrosword` | **`/Game/__PendingDelete/`** |
| `GK_Vibrosword` | **`__PendingDelete`**, and not in the asset registry at all |
| `SM_Vibrosword`, `MI_Vibrosword_01` | live |
| `BP_Vibroblade` | live -- and the **only** asset that draws `SM_Vibrosword` |
| `T_UI_Abilities_VibroswordStrike`, `T_UI_Abilities_VibroKnife` | live |

There is **no `CPD_*` for it anywhere in the 3,363 customization parts**, so none
of the three Armory seams can reach it. Offering it would mean authoring a new
customization part and gear kit -- adding an asset, which this mod has never done
and which is why removing the mod takes nothing away from a save.

Two false leads, both checked rather than assumed:

- **`UtilityItem_BondedToTheBlade_T1` is not the surviving vibrosword.** It is
  live, and the name suggests it inherited the cut item -- but it equips
  `BP_Weapon_JediLogo`, is tagged `ItemType.Force` and
  `br.CharacterClass.Bespoke.Jedi.TelRea`. It is a Jedi passive.
- **The reachable melee kits do not draw it either.** `GAM_Melee_Enemy_Coil_
  Scourge_VibroBlade` and `..._Striker_VibroBlade` are named for it, and the
  three unlocked 1H melee gear kits behind them draw `SK_CoilStrikerWeapon`,
  the Scourger's mesh and `SK_SlugratHook`. Not `SM_Vibrosword`.

Checking the mesh is what settled it; the names alone would have given the wrong
answer twice.

### `CPD_GK_DC-17M` is deliberately not a row

Its only requirement is `Specializations.Weapon.Blaster.DC17M`, the category its
own class grants. A character who has equipped the class carries it, so the
game's own enumeration should list the kit without help. **That is a prediction,
and the test checks it**: a DC-17M weapon class whose Armory list is empty is
what being wrong looks like.

Both facts are properties of the game's own data, read from cooked assets with
`retoc`.

Open Kit puts the Armory and the wardrobe in **one native module**, not two. Two
DLLs hooking the same customization functions compete for the same seam, and
whether they coexist depends on both implementations; a single module with both
features behind option fragments cannot conflict with itself.

## 6c. The Armory's lightsaber lock -- a gate in a *widget*, not in a part

Found after Change Weapon was seen to grey out for a
lightsaber user and cannot be un-greyed by leaving and re-entering.

`WBP_Menu_Armory_WeaponLanding` carries three `FGameplayTagQuery` defaults, and
all three are authored identically:

```text
CanChangeWeaponQuery     ALL( ALL( BitReactor.Item.UIType ), NONE( BitReactor.Item.UIType.Lightsaber ) )
CanCustomizeWeaponQuery  ALL( ALL( BitReactor.Item.UIType ), NONE( BitReactor.Item.UIType.Lightsaber ) )
CanModifyWeaponQuery     ALL( ALL( BitReactor.Item.UIType ), NONE( BitReactor.Item.UIType.Lightsaber ) )
```

and all four lightsaber gear kits -- Anakin's, Tel-Rea's two hands, and the
Second Sister's -- carry `BitReactor.Item.UIType.Lightsaber`, where a blaster
carries `UIType.LightPistol`, `UIType.Rifle` and so on.

So the game **deliberately** switches off Change Weapon, Customize Weapon and
Modify Weapon whenever a lightsaber is equipped. Nothing in the shipped game
exposes it: Tel-Rea is the only saber user and her hilt was never swappable.

### It is also why no saber gear kit could ever be offered

This corrects a conclusion recorded here the same day. The saber hilt rows were
observed to reach `offered=true` zero times and that was written up as "melee has
no gear-kit picker", with the rows removed. **The rows were fine.** The screen
that would enumerate `Slot.Character.Specializations.Weapon.GearKit` is gated off
before it ever asks, so the module was never consulted. The rows are restored.

The mistake is worth naming because it is the session's fourth of one kind: an
absence was explained by the first mechanism that fit, rather than by finding the
mechanism. "Never offered" was read as "cannot be offered".

### The token stream, decoded

`FGameplayTagQuery` serializes as a compiled byte stream plus a tag dictionary.
The stock stream is `0, 1, 5, 2, 2, 1, 0, 3, 1, 1`:

| bytes | meaning |
|---|---|
| `0` | stream version |
| `1` | has-root flag |
| `5` | `AllExprMatch`, followed by a count |
| `2` | two sub-expressions |
| `2 1 0` | `AllTagsMatch`, one tag, `TagDictionary[0]` = `BitReactor.Item.UIType` |
| `3 1 1` | `NoTagsMatch`, one tag, `TagDictionary[1]` = `...UIType.Lightsaber` |

Dropping the `NONE` clause is `0, 1, 2, 1, 0` -- `ALL( BitReactor.Item.UIType )`
-- which a lightsaber satisfies, because gameplay tag matching is hierarchical
and `UIType.Lightsaber` is a child of `UIType`. The dictionary is left intact;
an unreferenced entry costs nothing.

`scripts/container-builder/Program.cs` asserts the stock ten bytes and the exact
tag pair before writing, and refuses rather than guessing if either has changed.
`container/saber-armoury/` is the result, on its own chunk, deliberately not
bundled: if this widget fails to construct, the Armory fails completely rather
than degrading.

## 7. Build path

Containers are built by `scripts/build-container.sh`, which drives
`scripts/container-builder/` — a small C# program against UAssetAPI, run on the
.NET 10 SDK. It runs on Linux.

`retoc` handles the container ends and round-trips losslessly where it matters.
Verified on `BP_SpecializationSelectionVM`: extracted, repacked with `to-zen`,
extracted again, and the `.uexp` came back byte-identical at 31,388 bytes. The
`.uasset` header differs — retoc regenerates package metadata — but the export
data, which is where the tag queries live, survives untouched.

```text
retoc to-legacy     extract from the shipped containers
UAssetAPI           parse, edit the property, reserialize
retoc to-zen        pack the _P container
retoc verify        and mount against the full shipped set
```

**Byte surgery alone is not sufficient for the mapping edit.** Appending pairs
means referencing assets the picker does not currently import, so the `.uasset`
import and name tables must grow and every offset the header records shifts with
them. That is a structured change to the package, not a patch to the export data,
and it is why a real package writer is needed rather than a hex edit.

Two claims recorded here earlier were wrong and are corrected. There is no
obstacle to running .NET here — installing it was the answer, and it
took less effort than either alternative considered. And `ZCOM-5.6.1.usmap` parses
in UAssetAPI without complaint, at 32,263 schemas; the failure that suggested
otherwise was a mis-ordered command line feeding it a `.uasset`.

The native module is a separate toolchain entirely: `clang-cl` and `lld-link`
against an `xwin`-fetched MSVC CRT and Windows SDK, cross-building a PE32+ DLL on
Linux. See `scripts/setup-native-toolchain.sh`.

## 8. One enumerator, and the slot is the question

The seam probe ran in the game. Five hooks, observation only,
against a save with a freshly recruited operator. It settles what three rounds of
reasoning could not.

### There is one enumerator, not four

The PDB names four functions that can enumerate customization parts. Across a
whole session — 34 calls to the first, 2,358 to the requirements gate — the other
three were **never called once**:

```text
GetCustomizationPartDefinitions          34 calls
GetCustomizationPartIds                   0
GetAllCustomizationPartIdsMatchingTags    0
GetCustomizationPresetsForCharacter       0
DoesPartIdMeetRequirements            2,358
```

And it serves every picker in the game, not just the wardrobe. The same function
returned results of 8, 10, 94 and 120 entries as different screens were opened.

It returned `CPD_TacticalSpec_Soldier`, so the specialization cards genuinely
come through it. That is G1 answered by measurement: **append at
`GetCustomizationPartDefinitions` and every list is reachable from one hook.**

### The first tag is the slot being filled

The `FGameplayTagContainer` the caller passes is not just the character's tags.
Its first entry is the slot under question, and the rest is the character's own
set. Two slots were captured:

```text
br.Customization.Slot.Character.Specializations.Tactical.Primary   (8 results)
br.Customization.Slot.Character.Info
```

That is what makes a data-driven module possible rather than a pile of special
cases: the hook reads the slot tag and decides what, if anything, to append.

### An ordinary operator carries no `Info.Name` tag at all

The captured container for a stock Rodian operator holds 38 tags and 40 parent
tags, and **not one of them is an `Info.Name` tag**:

```text
br.Customization.Part.Character.Info.Archetype.Firebrand
br.Customization.Part.Character.Info.Faction.ZeroCompany
br.Customization.Part.Character.Class.Default
br.Customization.Part.Character.Species.Rodian
br.Customization.Part.Character.Specializations.Tactical.Scout
br.Customization.Accepts.Outfit.{Wor,Sin,Mtc,Mfr,Lar,Har,Gra,For,Fli,Clo,Coi,Civ,Pyk,Bsp}
br.Customization.Accepts.Helm.Rodian
br.Customization.Accepts.Unlocks.{Story_01_1,...,Deluxe,Preorder}
br.Customization.Accepts.Fact.InHub
```

This is the direct confirmation of the lookup-key model recorded in the
changelog. Hero kit parts require an `Info.Name.<Hero>` entry; an ordinary
operator supplies none, so they are never found. It also shows exactly what a
data mod is doing when it writes hero names onto the faction — it is adding the
missing key, and inheriting the whole character with it.

Note the twenty `Accepts.Outfit.*` and `Accepts.Unlocks.*` entries. Capability
tags of precisely this shape are what the MIT wardrobe adds to a *copy* of this
container for the duration of one requirements call. The mechanism Open Kit needs
is visible in the game's own data, on the game's own characters.

### The requirements gate is not the specialization filter

`DoesPartIdMeetRequirements` ran 2,358 times and was **never asked about any
watched part** — not the six hero parts, and not `CPD_TacticalSpec_Soldier`,
which is offered to everyone. A part the player is being shown is not passing
through that gate on its way to the screen.

So for specializations the filtering happens inside `GetCustomizationPartDefinitions`
itself. The gate is still worth hooking — it is what the wardrobe path uses, and
it is where a scoped requirement answer belongs — but it is not what hides the
hero kits.

### The hero parts are never enumerated

Probe 0.2.1 closed the ambiguity. All eight watched definitions resolved —
including every hero part — and they resolved after only **two** enumerator
calls:

```text
definitions resolved=Soldier|Man001A_TORS|Padawan|PadawanExtended|Warrior|
                     TheLostPadawan|TheMandalorian|Melee_2H_TelRea
tried_and_failed=none  not_tried_yet=none
```

Seventy-five further enumerator calls followed, covering every specialization
slot and the wardrobe, and the accumulated `watched` set never grew beyond
`CPD_TacticalSpec_Soldier`. The probe could see the hero parts perfectly well;
the game never put one in a candidate list.

So it is not enumerate-then-refuse. **The hero parts are not candidates at all**,
and the fix is to append them, not to relax anything.

`CPD_H_Outfit_Man001A_TORS` behaves identically — it resolved, and it is absent
from the 107 torsos the wardrobe offers. The wardrobe lock and the specialization
lock are the same mechanism, which is why one module can lift both.

### The slot map

Every list in the game, with what stock returns:

```text
Slot.Character.Specializations.Tactical.Primary      8
Slot.Character.Specializations.Tactical.Secondary    8
Slot.Character.Specializations.Talent               10
Slot.Character.Appearance.Humanoid.Body.Height       3
Slot.Character.Appearance.Humanoid.Body.Type         2
Slot.Character.Outfit.Torso.Mesh                   107
Slot.Character.Outfit.Helmet.Mesh                   58
```

This is the module's configuration table: slot tag in, list of part ids to
append out. Adding robes, sabers, wardrobe items or weapons later is a row, not
another hook.

### Identity is a slot

The requirements gate is called per slot too, and its slots are revealing:

```text
Slot.Character.Info            Slot.Character.Info.Name
Slot.Character.Info.Faction    Slot.Character.Info.Voiceover
Slot.Character.Info.PortraitPose  Slot.Character.Info.DilemmaArchetype
Slot.Character.Class           Slot.Character.Species
Slot.Character.Appearance      Slot.Character.Appearance.Humanoid.Body{,.Height,.Type}
```

`br.Customization.Slot.Character.Info.Name` is **a customization slot**, and a
character's name is the part equipped in it. That is the cleanest statement yet
of why granting `Info.Name.<Hero>` grants the whole character: it is not a label,
it is the identity part, and everything hung off that character follows it.

Note also what the gate is *never* asked about: no specialization part, no
outfit part, not even `CPD_TacticalSpec_Soldier`, which everyone is offered. It
governs the identity and appearance slots. Whatever filters the specialization
and outfit lists is inside `GetCustomizationPartDefinitions` itself.


### Two operators, no `Info.Name`

Across both sessions — a Rodian and a Human, 38 and 43 tags respectively — not a
single `Info.Name` tag appeared in any captured container, at any slot, through
either seam.

### Also established

A mod cross-compiled with clang-cl loads into the shipped MSVC-built UE4SS
without complaint. The installed UE4SS is Git SHA `a1e7f571`, the same commit the
module builds against.

## 9. An outfit cannot give anyone saber animations

A second-hand claim -- that equipping Tel-Rea's outfit gives a recruit full
lightsaber animations -- was tested against the cooked data and **does not
hold**. It fails in both directions: an outfit cannot
contribute the tag, and the animation system would not read it if it could.

### Only four fragment types contribute a tag to a character

A character's `FGameplayTagContainer` is the union of what its equipped parts
report through `GetOwnedGameplayTags`, and the PDB names every implementer:

```text
UCustomizationInstance::GetOwnedGameplayTags
UCustomizationFragmentInstanceGameplayTags::GetOwnedGameplayTags
UCustomizationFragmentInstanceSlot::GetOwnedGameplayTags
UCustomizationFragmentInstanceVariantSelector::GetOwnedGameplayTags
UCustomizationFragmentInstanceVoiceover::GetOwnedGameplayTags
```

`UCustomizationFragmentInstanceOverrideSlots` exists and is **not** on that list.
That matters, because OverrideSlots is what an outfit part actually carries.

The mechanism checks out against the live capture. A stock Rodian's container
holds `Part.Character.Info.Faction.ZeroCompany`, and `CPD_Faction_ZeroCompany`
carries exactly one `CustomizationFragmentGameplayTags` holding exactly that. It
holds `Part.Character.Species.Rodian` and `Accepts.Helm.Rodian`, and
`CPD_H_Species_Rodian` carries one fragment holding exactly those two. This is
also, precisely, the fragment a data-only unlock has to grow from one entry to
four.

### Tel-Rea's outfit parts carry no such fragment

Dumped with UAssetAPI against the usmap. All five pieces --
`CPD_H_Outfit_TelReaA_{TORS_MSA_TA,ARMS,LEGS,BOOT,HELM}` -- carry the same four
fragment types and no others:

```text
BitReactorUIDataFragment   CustomizationFragmentFoley
CustomizationFragmentMesh  CustomizationFragmentOverrideSlots
```

None of the four contributes a tag. `Info.Name.Tel-ReaVokoss` appears on these
parts only inside `AllowedSlots`, where it is the *requirement* to wear them, not
a grant for wearing them. Equipping her outfit contributes nothing to anybody's
tag set, so there is no aggregation to aggregate.

The control confirms the shape rather than the accident: `CPD_H_Outfit_Man001A_TORS`,
an ordinary offered Mandalorian torso, has no tag fragment either.

**One nuance worth keeping.** Three outfit parts *do* carry a tag fragment --
`Man001A_LEGS`, `Man002A_LEGS` and `Cly_LEGS` -- and what they contribute is
`br.Customization.Part.Character.Stance.Wide`. So an outfit can grant something;
what it grants is posture. No outfit part in the game grants an `Info.Name`.

### And animation is chosen by the weapon, not the wearer

Even a contributed name would not do it. Animation selection reads two things,
and neither is an outfit or an identity.

The weapon-specialization part names its own animation data directly, through two
fragments:

| part | `AnimSetSoft` | `ProxyTableSoft` |
|---|---|---|
| `CPD_WeaponSpec_Melee_1H` | `AS_Human_1HMelee` | `PxTable_Wep_1HMelee` |
| `CPD_WeaponSpec_Melee_1H_Anakin` | `AS_Human_1HMelee_Anakin` | `PxTable_Wep_1HMelee_Anakin` |
| `CPD_WeaponSpec_Melee_2H_TelRea` | `AS_TelRea` | `PxTable_Wep_2HSaber` |

And the stance set comes from a `StanceAnimationSetArchetype`, whose
`FStanceAnimationSetTagMatch` entries key on exactly two fields --
`MatchingAnimRigTag`, which takes `Part.Character.Rig.*`, and
`MatchingSpecializationTags`, which takes `Part.Character.Specializations.Weapon.*`:

```text
SASArch_2HRifle   SAS_Human_2HRifle  (no tag -- the default)
                  SAS_Kabb_2HRifle   Rig.Kabb
                  SAS_B1Battledroid  Rig.B1
                  SAS_BX_1HRifle     Rig.BX
SASArch_Vambrace  SAS_Kabb_1HPistol  Rig.Kabb + Specializations.Weapon.Blaster.Pistol
```

No archetype in the game matches on `Info.Name`, and none matches on anything
worn. `SAS_TelRea` is not reachable from `SASArch_1HMelee` at all -- that
archetype has exactly one version, the generic `SAS_Human_1HMelee`.

**So the wardrobe is cosmetic, and that is the finding.** It was worth an hour to
establish: the roadmap had it as possibly the fix for the worst-looking thing in
Core, and it is not.

### The same dump locates the sprint gap

The reported defect is that Anakin's one-handed saber reads correctly walking and
fighting but breaks grip while sprinting. The animation sets say why, in one row:

```text
BitReactor.Animation.Run   AS_Human_1HMelee         present
                           AS_Human_1HMelee_Anakin  ABSENT
                           AS_TelRea                ABSENT
```

`AS_Human_1HMelee` maps `BitReactor.Animation.Run` to
`/Game/Game/Characters/Humanoid/_Average/Anim/1HMelee/AM_1HMelee_Run_F`. Both
hero sets simply have no entry for that tag, so the run state has nothing to play
and falls back. The hero sets are also smaller in general -- Tel-Rea's has 70
entries against the generic set's 108, missing every `Movement.JogFStart*` and
`Movement.Turn*` -- which is consistent with sets authored for a character who is
only ever seen doing scripted things.

Two ways to close it, both inside the rule, neither yet built:

1. **Add the row.** Append one entry to `AS_Human_1HMelee_Anakin`'s `Animations`
   map -- tag `BitReactor.Animation.Run`, value `AM_1HMelee_Run_F`, the same
   montage the generic set uses. One array element in one asset, the same shape
   of edit as Core's picker container. Writes no tag and touches no character.
2. **Offer the generic part instead.** `CPD_WeaponSpec_Melee_1H` exists, requires
   no hero name at all (`Slot.Character.Specializations.Weapon` plus
   `Accepts.AuthoredOnly`), carries the same `GA_LightsaberStrike_T1` and
   `Attributes_Lightsaber_Standard` as Tel-Rea's, and points at the complete
   animation set. It is tempting and it is not obviously safe: it has no
   `DisplayName`, no icon and, unlike both hero sabers, **no `DefaultPart` gear
   kit** -- which is the shape of a part that equips and produces no visible
   weapon. That is the "holding nothing" symptom, so it needs testing rather than
   assuming.

Option 1 is the smaller claim and should be tried first.

## 10. The wardrobe's meshes, and what the helmet fit is for

Two questions the wardrobe raised, both answered from the cooked assets so the
test pass does not have to spend itself on them.

### `Height.Average` on a mesh option is not a restriction

Every Mandalorian mesh option is tagged `Appearance.Body.Height.Average` and
`Weight.Average`, which reads alarmingly like "only average operators get a
mesh". It is not, and the control settles it: `CPD_H_Outfit_Wor001A_TORS_L`, an
ordinary worker outfit offered to everybody, is authored identically --

```text
Wor001A_TORS_L   Height.Average, Weight.Average, Body.Type.Masculine  SK_HAAM_Wor001A_TORS
                 Height.Average, Weight.Average, Body.Type.Feminine   SK_HAAF_Wor001A_TORS
Man001A_TORS     Height.Average, Weight.Average, Body.Type.Feminine   SK_HAAF_Man001A_TORS
                 Height.Average, Weight.Average, Body.Type.Masculine  SK_HAAM_Man001A_TORS
```

The game visibly dresses short and heavy operators in Wor001A, so those tags are
a best-match hint or the body proportions are a deformation applied over one base
mesh. Either way the Mandalorian armour is authored exactly like stock armour and
carries no extra risk from it.

Tel-Rea's outfit is the genuine counter-example, and the contrast is the point:
her pieces ship *one* mesh each, `SK_HSTF_*` -- short, thin, feminine -- with no
masculine variant at all. That is a real reason not to offer her outfit, and it
is a different reason from the tag gate.

### Why the helmets and not the torsos

Upstream fits `Man001A_HELM` and `Man002A_HELM` by scaling them 1.07 and 1.06
horizontally at the head pivot, and leaves Cly's helmets alone. The mesh options
suggest the mechanism:

| helmet | selected by |
|---|---|
| `Wor001A_HELM` (stock) | `Appearance.Humanoid.Human.Head.Face.{Masculine,Feminine}` |
| `Man001A_HELM`, `Man002A_HELM` | `Appearance.Humanoid.Body.Type.{Masculine,Feminine}` |
| `Cly_HELM`, `ClyB_HELM` | `Body.Type.Feminine` only |

A stock helmet is chosen by the shape of the **face** it has to fit. The
Mandalorian helmets are chosen by **body type** instead, so an operator whose
face shape and body type disagree gets a helmet that was never matched to the
head under it. That is a plausible mechanism for the misfit upstream corrects by
scale, and it predicts the defect is worst on exactly the mismatched combination
rather than uniform.

Not proven -- a scale correction is a visual judgement and upstream's constants
were measured by looking at the game. It is recorded so the test pass knows which
operator to look at first.

## 11. Secondary specializations are a separate authored part

Offering the kits in the second specialization slot produced a Warrior with no
abilities. The registry settles why. Parsed from the
game's own `AssetRegistry.bin` with `retoc asset-registry`:

```text
Assault     primary yes   secondary yes      Scoundrel  primary yes   secondary yes
Gunslinger  primary yes   secondary yes      Scout      primary yes   secondary yes
Heavy       primary yes   secondary yes      Sniper     primary yes   secondary yes
Medic       primary yes   secondary yes      Soldier    primary yes   secondary yes

Warrior     primary yes   secondary NO
Padawan     primary yes   secondary NO
```

Every one of the eight stock specializations ships a second part,
`CPD_TacticalSpec_<Name>_Secondary`, and that is what `SpecializationPartMapping`
points at: the picker offers the primary part as the choice and equips the
`_Secondary` variant when the choice lands in the second slot. `Baker` and
`Astromech` have them too, so it is the general pattern and not a property of the
eight.

**Padawan and Warrior are the only two specializations in the game with no
`_Secondary` part.** Offering them in that slot therefore yields a specialization
with nothing in it — which is what happened. They are primary specializations by
authoring, and Open Kit now treats them that way.

`CPD_TacticalSpec_PadawanExtended` is *not* `Padawan_Secondary`; it is its own
asset. Mapping Padawan to it is a judgement call — the nearest thing the game
ships — and is what the pairing container does, behind an option that is off by
default. Warrior has no candidate at all.

This is also the correction of an earlier misreading. Seeing eight cards in the
secondary row, this document's author concluded the pairing map was starving it
and built a container for that. The eight cards were the stock pool with nothing
appended; the pairing map turned out to matter for a different reason entirely,
one slot further on.
