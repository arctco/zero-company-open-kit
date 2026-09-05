# Changelog

## 1.0.0

First release. Core, the wardrobe and the Armory are built, tested and confirmed
in game; both archives — the FOMOD installer and the manual layout — are built
and validated.

Published at [Nexus mod 133](https://www.nexusmods.com/games/starwarszerocompany/mods/133).

Entries are grouped by feature rather than by date. Where a conclusion was
reached and later overturned, both are kept — the correction is usually the more
useful half, and a changelog that only records what survived is not much of a
record.

---

### Core — specializations, talents and sabers

**The kits work.** An ordinary operator can be offered the Padawan or Warrior
specialization, take it, keep it across save and load, use its abilities through
a mission, and respec out of it again. No identity tag is written, no character
is edited, and no requirement is bypassed.

```text
[ZCOM_OPEN_KIT] READY hooks_active=true build=24874058
                seam=GetCustomizationPartDefinitions-post
                seam2=DoesPartIdMeetRequirements-scoped
                seam3=FilterAssetDataByTags
                identity_tags_written=0 character_edits=0
                requirements=asked-per-character
                lifted=named-capability-or-hero-name-on-a-discarded-copy
```

**The module is one hook over a slot-keyed table.** The catalogue hook reads the
first tag of the container the game passes — which is the slot being filled —
and appends the rows registered for that slot. Robes, sabers, wardrobe items,
colours and Armory gear are all rows rather than separate hooks. That
architecture came from the game's shape rather than from design: because the
slot arrives as data, a table reaches every list in the game through one seam.

**The module asks the game before it offers anything.** Every candidate part is
put to `UCustomizationStatics::DoesPartIdMeetRequirements` against the real
character, and a "no" means the part is not offered.

The trick is what to ask. These parts are gated on a hero's `Info.Name`, and an
ordinary operator has none — that gate is the entire thing this mod exists to
lift, so asking unmodified would answer "no" every time. So the hero's name goes
onto a *copy* of the character's tag container, the copy is what gets asked, and
the copy is destroyed before the answer is returned. Nothing is written to the
character; the game is never told anyone is Tel-Rea or Cly. It is asked a
hypothetical and the answer is thrown away with the copy.

What that buys is precision. The module lifts exactly one requirement, by name,
and every other requirement a part declares — species, slot capability, roster
level, anything a future part brings — is still answered by the game against the
real character.

Which hero each part requires was read out of the cooked assets rather than
assumed: the Padawan family names Tel-Rea, the Warrior family names Cly, exactly
one each. A row whose hero tag a build does not recognise disables itself rather
than falling back to appending blind.

**Anakin's class was built, offered behind a switch, and removed — by the check
rather than by argument.** It never appeared, and the log said why in one line:
`declined part=CPD_TacticalSpec_Anakin reason=character-fails-other-requirements`.

Reading the asset says what the reason is, and it is the most useful thing this
project learned about the game's data. Tactical specializations come in two
authoring families, told apart by what their `AllowedSlots` demand:

| family | requires | members |
|---|---|---|
| player | `Slot.Character.Specializations.Tactical.Primary` | Soldier, Assault, Sniper, … **Padawan, Warrior**, Hero_Aurelio |
| scripted | `Accepts.Character.Specializations` | **Anakin**, **Rex**, and every `Enemy_*` |

Padawan and Warrior are ordinary *player* specializations reserved to one
character by name. That is the whole premise of the mod, confirmed from the
other side: the thing being lifted is a name, on content already built to be
worn by a player's operator.

Anakin and Rex are not that. Making one appear would mean scoping in
`Accepts.Character.Specializations` — the capability every enemy spec wants —
which claims an operator is a scripted pawn rather than lifting a name gate, and
is outside the rule. So the switch is gone rather than forced.

**The kits are primary specializations, and the game says so.** Offering them in
the second slot produced a Warrior with no abilities. Every stock specialization
ships a separate `CPD_TacticalSpec_<Name>_Secondary` part, and that is what the
picker equips when a choice lands in the second slot. Parsed from the asset
registry, all eight have one.

The two rows do draw the same eight icons, but the icon is the *choice* and the
part equipped behind it differs by slot. Same list, different part.

**The Padawan secondary exists and is called "Wayseeker".** Confirmed in game:
`SPEC 1: PADAWAN` and `SPEC 2: WAYSEEKER` on the same operator. It is
`CPD_TacticalSpec_PadawanExtended` — Lightsaber Throw, Force Pull, An Elegant
Weapon — and it declares `Tactical.Secondary` in its own `AllowedSlots`.

An earlier note here recorded that no Padawan secondary existed anywhere in the
game, on the strength of a search for the `_Secondary` suffix that the other
eleven specializations use. `PadawanExtended` is the one member of the family
that does not use that suffix, so the search missed it and the absence was read
as a fact. `AllowedSlots` says what slot a part is for, and consulting it was the
whole fix. `padawan-secondary.ini` is on by default now that it offers something
real.

**Warrior genuinely has no equivalent** — checked against the same list — so it
is first-slot only at any setting.

**Both saber grips are offered, because the game carries no notion of gender.**
The complaint was that a male operator holds Tel-Rea's two-handed saber
strangely. The obvious fix — give the feminine body Tel-Rea's grip and the
masculine one Anakin's — is not implementable: there is no gender anywhere in
the customization tag space. `Rig` distinguishes Humanoid, Astromech and BX;
`Species` is species; nothing carries sex. Confirmed by exhausting the tag
namespace rather than by failing to find it.

So `CPD_WeaponSpec_Melee_1H_Anakin` joins Tel-Rea's under the same switch and
the player keeps whichever animates correctly. A choice rather than a guess, and
it cost one table row.

**The saber is cosmetically wrong and stays that way.** The blade is permanently
ignited and held in the wrong grip. The animations that carry a lightsaber
correctly hang off the hero characters and are reached through their
`Info.Name`, and granting that is the one thing the rule forbids. The switch's
documentation says so rather than pretending otherwise.

**The sprint gap has an exact cause and a one-element fix**, found in the
animation dump. Anakin's saber breaks grip while sprinting because
`AS_Human_1HMelee_Anakin` has no `BitReactor.Animation.Run` entry at all — the
generic `AS_Human_1HMelee` maps that tag to `AM_1HMelee_Run_F`, and both hero
sets omit it. Tel-Rea's set is missing far more: 70 entries against the generic
set's 108, with every `Movement.JogFStart*` and `Movement.Turn*` gone, which is
what a set authored for scripted appearances looks like.

The fix is to append one entry to one map in one asset — the same shape of edit
as the picker container, writing no tag and touching no character. Not built,
and written down precisely enough to be an hour's work.

`CPD_WeaponSpec_Melee_1H` is a tempting alternative and should be tested rather
than adopted: a generic, un-named one-handed lightsaber requiring no hero,
carrying the same ability and attribute set as Tel-Rea's and pointing at the
complete animation set. It also has no `DisplayName`, no icon and no
`DefaultPart` gear kit — the shape of a part that equips and produces no visible
weapon.

**A tutorial softlock, introduced here and contained.** Creating a *new*
character with the Padawan kit produced a recruit who reached the tutorial
holding nothing, could not fire the shot the tutorial requires, and could not
continue.

The mechanism is the seam being more general than the feature. The hook appends
at `GetCustomizationPartDefinitions`, which serves *every* customization
surface, so the Weapon-slot row put Tel-Rea's saber into the weapon class step
of character creation. A pawn being created cannot satisfy that part: the card
refuses the selection, and the character is finished with no weapon class at
all.

The saber moved onto its own `padawan-saber.ini`, off by default. With it off, a
character created with the Jedi specialization keeps an ordinary blaster and the
tutorial runs to the end. That also separated the two causes: the specialization
rows are safe during creation, and the Weapon-slot row alone was the softlock.

**The softlock is not fully cured, and the remaining cause is unknown.** The
plan was to offer the kits only to a character already in the company — a
requirement rather than a tutorial special case, since the module cannot reliably
detect a tutorial and anything matching a level name breaks on the next patch.

Measured at the moment the kits are offered in the creator:
`character_tags=86 in_company=true`, against 82-85 for an ordinary operator. The
pawn already carries `Info.Faction.ZeroCompany`. There is nothing to gate on.

So the README's *Known issues* entry stands as the answer, and the open question
is recorded as open: nobody has found out why a Jedi-specialized created
character cannot finish the tutorial. The original diagnosis blamed the weapon-
class step and the saber row; that row is behind its own switch now, and the
remaining cause is unknown rather than known-and-accepted.

**A kit locks once taken, and the picker container is the fix.**
`BP_SpecializationSelectionVM` holds `SpecLockedQuery` and `TalentLockedQuery`,
both `ANY( br.Customization.Part.Character.Info.Name )`. Every hero kit part
carries an `Info.Name` tag — that is exactly how the game finds them — so the
query matches and the choice sticks. For Tel-Rea and Cly that is intended
authoring; for a recruit it is a trap.

The `-swap` container variants retarget both queries' single dictionary tag from
`br.Customization.Part.Character.Info.Name` to
`br.Customization.Slot.Character.Class` — a registered tag carried by
character-class parts and never by the specialization and talent parts these
queries are evaluated against, so nothing matches.

The tag is retargeted rather than the token stream rewritten, deliberately. The
stream stays byte-for-byte a valid ANY-of-one-tag query (`[0,1,1,1,0]`), so
nothing rests on having guessed `FGameplayTagQuery`'s encoding correctly.

It lifts the lock for hero characters too, which is a behaviour change beyond
"what is offered", and is why it is an optional container rather than part of
the module.

**Uninstall is safe, for a better reason than expected.** An operator kept the
Mandalorian kit in a save loaded with the mod removed. The earlier prediction —
that a modded-in part would persist as data the game could no longer resolve —
was wrong, and wrong because it misread what the mod does. Open Kit adds no
asset. `CPD_TacticalSpec_Warrior` ships with the game; the mod only stops hiding
it. A save referencing it is referencing stock content, so it resolves with or
without the mod present.

---

### Wardrobe

**The wardrobe is finished: 653 catalogue rows across three seams, a render fit,
and the colour palette.** Every gate the game applies to a garment is either
lifted or deliberately left alone, with the reason recorded for each.

**The uninstall passes, and the result is the best of the three it could have
given.** Remove the module and both containers, then load a save whose operators
wear modded outfits:

| | |
|---|---|
| the save | loads, no errors |
| the character | still wearing the modded top |
| the picker | exactly vanilla — the part is no longer offered |

An outfit behaves as a specialization does. The save records what was equipped
and resolves it from the game's own assets, which never went anywhere. The
character keeps the look and simply cannot choose it again, which is correct
behaviour rather than a compromise.

**Appending is not enough for an outfit.** The Mandalorian tiles appeared,
equipped on authored characters, and equipped on nobody else. Appending a part
to a list only makes the tile appear; the game asks
`DoesPartIdMeetRequirements` again, unscoped, when the player *picks* it. Core
got away with one hook because the game does not re-check a specialization on
selection. It re-checks an outfit.

So the requirement seam is hooked too, answering the same scoped question the
catalogue path asks, and only ever for the module's own part ids. Every other
part in the game goes straight through untouched, which matters because that
seam runs some 2,400 times a session. A failure to install it degrades rather
than refuses: the specializations still work, and the log says
`degraded ... effect=parts-are-offered-but-may-refuse-selection`.

**Capabilities come from the character's class part**, which is the cleanest
statement of the wardrobe lock this project reached:

| class | outfit families granted |
|---|---|
| `CPD_Char_Class_Hero_Humanoid` — Hawks and every recruit | 14 |
| `CPD_Char_Class_Hero_Umbaran` — Luco | 11 |
| `CPD_Char_Class_Hero_Mando` — Cly | **1**, only Mdo |
| `CPD_Char_Class_Hero_Padawan` — Tel-Rea | **0** |
| `CPD_Char_Class_NPC_Humanoid` | 22, Mdo among them |

There is no capability everyone has. The generator had been skipping 195 parts
as "already available to everyone" — true of Hawks, false of the people he
recruits alongside. Every outfit-family capability is now scoped rather than
assumed granted, which costs Hawks nothing: the catalogue hook already refuses to
append a definition the game has listed, and adding a tag a character already
carries changes no answer. 653 rows, up from 464.

That measurement cut both ways. The authored heroes had *less* wardrobe than
Hawks — the same defect reflected — because their classes are far poorer than an
operator's.

**What locks an outfit**, across 1,132 of the game's 1,253 outfit parts that
declare requirements:

| lock | parts |
|---|---|
| `Accepts.AuthoredOnly` | 605 |
| a capability (`Accepts.Outfit.*`) | 432 |
| a hero's `Info.Name` | 82 |
| nothing | 13 |

The 82 name-locked parts are the lift this mod already makes and are offered: 54
under `wardrobe-heroes.ini` across ten characters, and Anakin's four under
`wardrobe-anakin.ini`.

**`Accepts.AuthoredOnly` is not a capability a character can be handed.** A
sweep of every customization part in the game found no part that grants it — not
one. It is a mark meaning "assigned by authoring, never chosen by a player".
Where it is scoped in, it is scoped for parts listed by name only. It names
nobody, so unlike a hero's `Info.Name` it cannot pull one character's content
onto another, which is the failure the rule exists to prevent.

That also explains an early test report exactly: Trick, Luco and Tel-Rea could
wear Mandalorian armour Hawks could not, because authored characters are dressed
by preset rather than by passing this check.

**Helmets fit by species, and that gate is about fit rather than permission.**
75 helmets require `Accepts.Helm.Human`, and only four species grant it — Human,
Clone, Mirialan, Umbaran. Every other species carries its own
`Accepts.Helm.<Species>`, and Felshi, Tognath and the droids grant none at all:

```text
CPD_H_Species_Human      Accepts.Helm.Human
CPD_H_Species_Rodian     Accepts.Helm.Rodian
CPD_H_Species_Togruta    Accepts.Helm.Togruta
CPD_H_Species_Tognath    (none)
```

`wardrobe-helmets.ini` lifts it. The cost is honest and is the player's to
judge: a helmet shaped for a human skull now has to sit on montrals, lekku,
horns and snouts. That is why it is a switch rather than a fix.

**Two groups are deliberately not offered.** 66 parts requiring the Deluxe or
preorder edition, which are sold separately and are not this mod's to hand out;
and 116 parts gated on `Accepts.Unlocks.Story_*`, offered behind
`wardrobe-story.ini` but off by default — the campaign grants those as it is
played, so turning it on changes the game's pacing rather than lifting a lock.

**Tel-Rea's Jedi robes are offered, and the reasoning that excluded them did not
survive its own evidence.** They were left out because each piece ships a single
`SK_HSTF_*` mesh — short, thin, feminine — so there would supposedly be nothing
to draw on anyone else. But `Height.Average`/`Weight.Average` on a mesh option
had already been shown to be a best-match hint rather than a gate, from a stock
control (`CPD_H_Outfit_Wor001A_TORS_L`, an ordinary worker outfit the game
dresses everybody in, is authored identically). `Height.Short`/`Weight.Thin`/
`Body.Type.Feminine` is the same field. Upstream settles it from the other side:
Cly's feminine-only helmet meshes resolve on a masculine Hawks at runtime.

Ten rows behind `wardrobe-jedi.ini`, both variants across five slots.
`CPD_H_Outfit_TelRea_TORS` stays out on its own merits: it wants
`Accepts.AuthoredOnly` and sets `bReplacesBaseMesh` while overriding four other
slots, which is a whole-body costume rather than a wardrobe piece.

**The table is generated, not typed.** `scripts/generate-wardrobe-table.py`
emits the rows from an extracted asset tree, and every exclusion is a rule read
out of the asset rather than a judgement: no outfit slot; `Outfit.Pack.Mesh`
(backpacks, which a torso already carries in its own mesh fragment); and
`bReplacesBaseMesh` — five torsos that replace the wearer's *body* rather than
dressing it. Kabb's three are in that last group, and they would have put a
Herglic body on a human operator.

**Two data faults, both caught by cross-checking rather than by re-reading.**

Sixteen `CPD_H_Outfit_Clo001_BOOT_Tint*` parts come back from UAssetAPI with two
hundred tags naming three different slots. That is a container that did not
parse, not a part valid in three places, and taking the first slot would have put
a boot in the arms list. The generator now refuses any part naming more than one
slot, and reports how many it dropped. Every other part in the game parses in the
expected two to five tags.

Thirteen parts across `CPD_H_Outfit_Clo009_*` and `CPD_H_Outfit_PlagueTrooper_*`
declare `Outfit.Arms.Mesh` whatever they are, while being named and modelled as
torsos, helmets, boots and legs. These parse cleanly, so the first guard did not
catch them. The generator now cross-checks the declared slot against two
independent statements of what the part is — its own name, and the mesh it draws
— and **both** must agree with each other and disagree with the slot before a
part is dropped. That second condition is load-bearing: the mesh check alone
threw out `CPD_H_Outfit_Luco_TORS_MSA_HTALB`, a torso that legitimately draws a
helmet mesh, which is what the `H` in `HTALB` means.

**A cross-check rule that was right about outfits and wrong about weapons.**
"Drop a part whose `DisplayName` belongs to another part" caught the thirteen
mis-slotted outfit parts. Run against the Armory it threw out eleven, including
Cly's pistol and Hawks's, and reading them says why: a gear kit and its weapon
model share a `DisplayName` by design, and twenty pairs in the game do it.
Narrowed to collisions within one slot, it then catches nothing the other rules
miss. So it is reported per row as a caveat instead of acted on — what it finds
is real presentation ambiguity on working, distinct assets.

A rule that is right about one family of parts is not thereby right about
another. This one shipped against outfits and was a false positive eleven times
out of eleven against weapons.

**The helmet render fit is ported from upstream, and it works.** Sternab's
`helmet_render_fit.cpp`, adapted rather than reimplemented, with attribution. It
was left out of the first build on the argument that its two scale constants are
visual judgements that cannot be checked from the data; that argument was right
and is now spent, since the defect is real in game and upstream's measured
numbers are the only ones anybody has.

The one change of substance is a data-driven family table where upstream had two
hardcoded branches, because Open Kit offers four helmet families upstream never
did. Cly's, ClyB's and Tel-Rea's two are deliberately not in the table: a scale
for them would be a number nobody has measured, and fitting a helmet by an
invented constant is worse than not fitting it.

It also spent a round appearing never to have applied. Across 241 registry scans
it reported `target_components=0` every time. The counters could not distinguish
"nobody wore a fitted helmet" from "the scan cannot see helmet components at
all", so the scan line gained `helmet_meshes=`, counting components drawing any
`_HELM` mesh. The innocent explanation was the right one — the two fitted
helmets had not been worn while a scan ran. The counter stays in, because "it
says READY but does nothing" is a question that will be asked again.

**What the helmet fit is for, mechanically.** A stock helmet picks its mesh by
`Humanoid.Human.Head.Face.{Masculine,Feminine}` — the shape of the face it has
to fit. The Mandalorian helmets pick by `Body.Type` instead, so an operator
whose face shape and body type disagree wears a helmet never matched to the head
under it. Not proven, but it predicts where to look first.

**Implementation notes.** The row bitmask is gone — it capped the catalogue at
32, then at 64, and 91 rows would have been the third time; a flag per row costs
one byte and has no ceiling. The catalogue's size is deduced with `std::to_array`
rather than written, because it was wrong twice while it was a literal. The
`READY` line names the enabled switches instead of carrying one placeholder per
switch, because that count moved three times in a day.

---

### Colours

**135 outfit colours and 90 weapon paints, confirmed in game** at
`outfit_palette_size=135`, `additions=135`, with labels on all of them, and
`family=weapon-paint additions=90`.

**Colours arrive through a third seam.** Ported from Sternab's MIT-licensed Zero
Company Expanded Colours, adapted with attribution the way the helmet fit was.

The plan written an hour earlier was wrong, and reading the reference
implementation is what caught it. The recorded plan was to reach colour parts by
adding a rule to the catalogue hook — "if the slot being filled is a `*.Color.*`
slot, offer the colour parts". They are not reachable from there at all. Colours
never pass through `GetCustomizationPartDefinitions`; they come from
`UCustomizationPartsTagSubsystem::FilterAssetDataByTags`, which returns
`FPrimaryAssetId`s filtered by tag. That is *why* a colour part carries no slot
tag — a fact this project had already measured and drawn the wrong conclusion
from.

So the seam map gains a third entry, and the earlier finding that there is "one
enumerator, not four" is corrected in scope: one enumerator in
`UCustomizationStatics`, and a separate tag subsystem beside it.

Upstream's hook identifies which picker it is filling from what the game already
put in the list, which is neat and is why it needs no slot tag of its own. Both
addresses were re-resolved from this build's PDB; all 130 asset names were
checked against the game's data, and every one exists.

**Upstream's curation was checked rather than taken on trust, and it holds.**
Comparing every locked colour's linear RGB against the 119 a player can already
pick:

| locked outfit colours | |
|---|---|
| within 0.02 of one already available | 82 |
| within 0.05 | 126 more |
| genuinely distinct | 127 |

Offering all 335 would be a picker full of swatches a player cannot tell apart,
so upstream's 40 ship as they are.

**95 more outfit colours behind `colours-extra.ini`.** Of the 101 that are at
least 0.05 away in linear RGB from anything already available, six are excluded
by name and upstream's README named them first: `Tester_A/B/G/R` are developer
test ramps, and `SlugratRed`/`_Lite` belong to a character rather than to a
palette. Generated by `scripts/generate-colour-table.py` with labels derived from
the asset names.

The curated forty stay first in the table and the extra set extends it, so
"curated only" is a prefix rather than a second code path — one count decides
which, and the `colours_READY` line reports it.

**Weapon paints needed nothing: upstream offers all of them.** The game locks
exactly 90 and Expanded Colours ships all 90.

**A silent no-op that shipped, and the two cheap things that caught it.** The
first extra-colours build shipped with the switch permanently off, because the
line in `initialize()` that reads it was written as a string replacement against
an anchor an earlier reindent pass had already changed. The replace matched
nothing, did nothing, and was not asserted, so the build succeeded and looked
correct while `g_outfit_colour_count` kept its initialiser of forty.

Two things caught it. The palette size is a *field* in the log rather than the
literal `40` it used to be, so a binary containing 135 entries reporting 40 was
visible in one line; and the option file was recognised (no `option_ignored`),
which ruled out the name and pointed straight at the code.

This was the second silent no-op string replacement to ship in a day — the first
mis-parsed a wardrobe slot — and both would have been caught by asserting the
anchor. Scripted edits now assert their anchor, and any count or mode goes into
the log as a field rather than a literal.

---

### Armory

**Ten rows across three slots**, `armoury_rows=10/10`, with Rex's pistol, the
Padawan secondary, the Armory's lightsaber lock and the DC-17M all confirmed in
game.

**Two of the four planned Armory items had already shipped with the wardrobe.**
Clone Commando armour and Captain Rex's armour are *outfits*, and the wardrobe
sweep covered every outfit in the game:

| family | in the table |
|---|---|
| `CPD_H_Outfit_Clo010_*` — Clone Commando (`Accepts.Outfit.CloCommando`) | 8 of 8 |
| `CPD_H_Outfit_CaptainRexA_*` — Rex's armour | 5 of 6 |

The sixth is `CaptainRexA_PACK`, absent under the backpack rule rather than by
oversight. So the Armory is weapons, and it is three slots rather than one: the
weapon class, the gear kit carried, and the model a weapon is skinned as.

**The scripted authoring family does not reach the Armory.** The risk flagged as
most likely to sink this feature was `Accepts.Character.Specializations`, which
Anakin, Rex and every enemy tactical spec require, and which cost a built-and-
removed class once. Across all 208 parts in the three weapon slots that tag does
not appear once.

What the parts do require was censused rather than sampled: 110 want
`Accepts.AuthoredOnly`, one wants `Accepts.Unlocks.DC17M`, five want a hero by
name, and the rest want a weapon category or a rig — the game's own grouping,
which is not lifted, because it is what keeps rifles out of the pistol list.

**Rex's pistol works: the operator carries two and DualFire fires both.**
`CPD_WeaponSpec_Blaster_Pistol_Rex` grants the ability,
`CPD_GK_Pistol_DC-17_Rex` supplies `ItemType.Weapon.Primary.A` and `.B`, and the
operator holds a pistol in each hand.

Every earlier failure had the same cause: the stock single-pistol kit was
equipped, because the class's `DefaultPart` points at it and the two tiles were
both called "DC-17". The picker label is what fixed it — not a code path, a
name.

**The game's own authoring mispairs Rex's class and kit**, which is why the
pistol fired once before the label existed:

| | |
|---|---|
| `GK_Pistol_DC-17_Rex` | the **only** dual-wield kit in the game — two gear items, `ItemType.Weapon.Primary.A` and `.B` |
| `CPD_WeaponSpec_Blaster_Pistol_Rex` | the **only** part that grants `GA_DualFire` |
| its `DefaultPart` | `CPD_GK_Pistol_DC-17` — the **single**-pistol kit |

It never shows in the shipped game because no player can reach the class.
`GA_DualFire` itself references no socket and no gear item — it just fires twice
— so the empty hand is a weapon that was never attached, not an ability failing.

**Reading the save settles which kit is equipped**, which the log could not: a
`.sav` is a ZIP, and grepping its `SaveGame` member showed the encounter autosave
holding `CPD_WeaponSpec_Blaster_Pistol_Rex` and `CPD_GK_Pistol_DC-17`, and not
`CPD_GK_Pistol_DC-17_Rex`. Reading the save for equipped state, rather than
inferring it from what was offered, is the reusable part. The requirement hook
now also emits `pick_asked part= slot= answer=` when a row's answer changes.

**The label was necessary and not sufficient**, which play found and the log
could not. Equipping `CPD_WeaponSpec_Blaster_Pistol_Rex` still leaves the stock
single-pistol kit on, because the class's `DefaultPart` is authored to it — so
picking "Blaster (Rex)" is one step and choosing "DC-17 (Rex)" under Customize
Weapon is a separate second one, and an operator who stops after the first
carries one pistol. Recorded as a known issue rather than fixed: the shape of a
fix is a one-property container override retargeting that `DefaultPart` at
`CPD_GK_Pistol_DC-17_Rex`, which writes no identity and sits inside the pipeline
already built, weighed against shipping a container for a single weapon. The
label stays either way; it is what makes the second pick findable.

**The DC-17M works.** Trick's clone-commando rifle — three fire modes, its own
abilities, attributes, animations and icon — offered to an ordinary operator
under its authored name, "DC-17m Weapon System".

It was locked twice, by Trick's name and by `Accepts.Unlocks.DC17M`, a tag
required by exactly one part and granted by nothing in 3,363 customization parts
or 793 character definitions. No class grants it and no character carries it,
unlike `Unlocks.Story_*` and `Unlocks.Mission*`, which `CPD_Char_Class_NPC_Humanoid`
does grant. It is `AuthoredOnly` under another name, and it is lifted the same
way.

`CPD_GK_DC-17M` was deliberately never a row, on the argument that its only
requirement is the category its own weapon class grants, so the game would list
the gear kit unaided once the class was equipped. That held: lifting the lock on
the class was enough and the rest followed.

**The Armory's lightsaber lock is a gate in a *widget*, not in a part** — a
shape this project had not seen before. Change Weapon greys out for a lightsaber
user and stays greyed. `WBP_Menu_Armory_WeaponLanding` carries three
`FGameplayTagQuery` defaults, all three identical:

```text
ALL( ALL( BitReactor.Item.UIType ), NONE( BitReactor.Item.UIType.Lightsaber ) )
```

and all four lightsaber gear kits carry `BitReactor.Item.UIType.Lightsaber`. So
Change Weapon, Customize Weapon and Modify Weapon are switched off the moment a
saber is equipped. In the shipped game nobody can see it: Tel-Rea is the only
saber user and her hilt was never swappable.

`container/saber-armoury/` lifts `CanChangeWeaponQuery` on its own chunk,
deliberately not bundled — if that widget fails to construct, the Armory fails
completely rather than degrading. `OPENKIT_SABER_ARMORY=all` also lifts
Customize and Modify.

The token stream is decoded rather than guessed, and the builder asserts the
stock ten bytes and the exact tag pair before writing, refusing rather than
shipping an override that lifts nothing. Verified by re-extracting from the
mounted container: `CanChangeWeaponQuery` reads `[0,1,2,1,0]` —
`ALL( BitReactor.Item.UIType )` — and the other two are byte-for-byte stock.

**The melee gear-kit picker exists, and lifting all three queries is what opens
it.** With `CanChangeWeaponQuery` alone the Change Weapon list un-greys but shows
weapon *specializations* — all eight classes. The hilts are behind
Customize/Modify Weapon, so `OPENKIT_SABER_ARMORY` defaults to `all`.

Confirmed on Anakin's one-handed class, and the count is exactly right: the
MODEL list shows five entries — None, "Anakin's Lightsaber", and three blanks —
which is precisely the four `Melee.1H` gear kits the game ships, all stock, none
of them this module's rows.

This corrects a conclusion recorded earlier the same day. Both hilt rows had
been observed to reach `offered=true` zero times, and that was written up as
"melee has no gear-kit picker" and the rows removed. The rows were fine. The
screen that would enumerate the GearKit slot is gated off before it ever asks,
so the module was never consulted. An absence was explained by the first
mechanism that fit, rather than by going and finding the mechanism.

**The three blank melee tiles are named.** No melee gear kit in the game carries
a `DisplayName`, because until the lock was lifted no player could open that
screen to notice. Named from the weapon each actually equips, followed through
the gear kit to the mesh rather than from the part name:

| part | equips | label |
|---|---|---|
| `CPD_GK_MeleeWeapon_CoilStriker` | `SK_CoilStrikerWeapon` | Coil Striker |
| `CPD_GK_MeleeWeapon_Enemy_Coil_Scourge` | `GK_MeleeWeap_Scourger` | Scourger |
| `CPD_GK_MeleeWeapon_Enemy_Generic_Striker` | `SK_SlugratHook` | Slugrat Hook |

These are the game's own parts, not rows this module adds — they were always
available to anyone with a one-handed melee class. The module only gives them a
name so the screen is legible.

**"No `DisplayName` means a blank tile" was a rule right about one weapon family
and wrong about another.** It was measured on the stock *blaster* kits, every one
of which is named. No lightsaber gear kit in the game has a name — including
`CPD_GK_Lightsaber_TelRea`, the hilt a Padawan is already holding as her class's
`DefaultPart`. Namelessness is the norm in that list, not a defect, and the rule
now asks per weapon category.

That admits `CPD_GK_Lightsaber_Enemy_Imperial_Trilla` and Tel-Rea's own hilt
with it, which is load-bearing: hers appears in no list, so offering the
Inquisitor's alone would let a player switch away from the stock saber with no
way back.

**Settled: no tag this module can add makes an ordinary operator eligible for
either saber hilt.** Four differential probes at the requirement seam, on a
character carrying `Melee.2H` and 85 tags — as shipped, plus the category, plus
Tel-Rea's name, plus the slot tag. Every one `false`.

```text
pick_asked part=CPD_GK_Lightsaber_Enemy_Imperial_Trilla answer=false
    character_has_melee_2h=true character_tags=85 answer_if_category_scoped=false
```

The slot probe is the control that makes the rest mean anything: adding the slot
tag turned *passing* parts false, so the scoping mechanism provably changes
answers and the other refusals are real.

The asymmetry is still unexplained — the catalogue seam accepts
`AuthoredOnly`-only *outfits* for the same operator while the requirement seam
refuses `AuthoredOnly`-only *hilts*, from the same function with the same kind of
copy. Recorded as unexplained rather than guessed at.

The only route left is editing the *part* to drop `Accepts.AuthoredOnly`, which
is a decision about the project's rule and has not been taken. Two caveats if it
ever is: the requirement the game reads lives in `AssetRegistry.bin`, which a
`_P` container does not override, so editing the `.uasset` may not be enough; and
`remove` mode is currently listed among the superseded container experiments.

**A correction that stood for a while: `Accepts.AuthoredOnly` cannot be lifted by
scoping the tag.** This project claimed otherwise from the moment the wardrobe
landed. The original proof does not survive re-reading. It was that Anakin's
arms, boots and legs were *offered* once `AuthoredOnly` was scoped — but those
parts also require `Height.Tall`, `Body.Type.Masculine`, `Weight.Average` and
`Rig.Humanoid`, which a tall masculine human operator really does carry. The
offer proved those four, not the fifth. And the torso usually cited beside them
scopes Anakin's *name*, not `AuthoredOnly`.

The measured position after the four probes is narrower than either claim: the
tag is not liftable at the *requirement* seam, and the *catalogue* seam behaves
as though it is. The 243 wardrobe rows that scope `AuthoredOnly` work in game;
the hilts do not.

**The vibrosword is cut content, not locked content.** `UtilityItem_Vibrosword`
and `GK_Vibrosword` are in `/Game/__PendingDelete/`, and there is no `CPD_*` for
it among the 3,363 customization parts, so none of the three Armory seams can
reach it. Offering it would mean authoring a new part and gear kit, which is
adding an asset, which this mod has never done.

Two names pointed the wrong way and the mesh settled both.
`UtilityItem_BondedToTheBlade_T1` is live but equips `BP_Weapon_JediLogo` and is
a Jedi Force passive. The three unlocked 1H melee kits are reached by abilities
named `..._VibroBlade`, but they draw `SK_CoilStrikerWeapon`, the Scourger's mesh
and `SK_SlugratHook`. `BP_Vibroblade` is the only asset in the game that draws
`SM_Vibrosword`, and nothing reachable references it.

**The rows are generated, and 178 parts are excluded on rules read out of the
assets.** `scripts/generate-armoury-table.py` follows the wardrobe generator's
pattern: the header is the argument. The yardstick is the stock unlocked kits,
every one of which has a `DisplayName`, both images, at least one ability and a
`Part.Weapon.Type.*` tag; each exclusion is a part failing that in a way that
would show on screen.

```text
excluded  72  not an Armory slot
excluded  48  already available -- nothing this module lifts
excluded  34  no DisplayName -- a blank tile
excluded  14  requires an NPC or droid character class
excluded  11  AllowedSlots does not name exactly one slot
excluded   7  an _Enemy twin of a weapon already offered
excluded   1  a gear kit in no category appears in every list
excluded   1  no named gear kit exists in the category it grants
```

`CPD_WeaponSpec_Melee_1H` falls out of the third rule by itself — it has no
`DefaultPart`. `CPD_GK_Scattergun` falls out of the seventh: it declares no
category at all, so it would appear in *every* weapon class's list.

**Diagnostics that answered the wrong question, twice.** Armory rows were logged
once ever, so the same question was asked three times and answered wrongly twice
— every evaluation after the first was hidden. Rows are now logged when their
answer *changes*.

Separately, the probe fields are only computed for Armory rows and the arrays are
zero-initialised, so every other row logged `character_tags=0
answer_if_..._scoped=false`, which reads exactly like a measurement. For a moment
it looked as though passing parts were being asked with an empty container. They
now log `not-asked`. A default that is indistinguishable from a result is not a
default.

**`armoury_rows=` is a field in the `READY` line, not a literal**, for the same
reason the palette size is. Ten rows in a table of six hundred and seventy is
invisible in the total. Every part id, switch name, new field and new tag is
grepped out of the built DLL before it is installed, and the installed hash is
checked against the built one.

---

### User interface

**The specialization row overflowed its frame, and the fix is arithmetic.** Ten
cards outgrow a frame authored for eight.

What the row actually is: `Specializations`, a horizontal `BitReactorListView`
with `HorizontalEntrySpacing` 32, filling an `Overlay` inside a
`BitReactorWidgetSwitcher`. Its entry is `WBP_FocusTree_SpecializationCard`, a
fixed 90x168.

The budget, once the numbers exist. The frame,
`WBP_FocusTree_Specilization_Backing`, is a fixed 1042; the list's slot is inset
44 left and 32 right; the row therefore has **966**. Stock is eight cards:
8×90 + 7×32 = **944**, fitting with 22 to spare. Core makes it ten:
10×90 + 9×32 = **1188**, and the row grows past the frame. It shows on the
*left* because `WBP_FocusTree_AssignSpecialization_New` stacks its rows in a
right-aligned `VerticalBox`, so the surplus has nowhere else to go.

That same 944-in-966 settles a question that would otherwise have needed a
running game: the spacing falls **between** entries, not around each one. Around
each one, stock would need 8×(90+32) = 976 and would not fit. It fits.

So the row is refitted to the space it has: **ten cards at 80 wide with 16
between them is 944** — the same width stock gives eight, and the same slack.
Two floats, in two assets, shipping as `pakchunk98-ZCOMOpenKitUI_P`.

**Two alternatives were rejected on measurement.** Widening the frame: the art
would have allowed it — `BG` and `PanelFrame` are both `WBP_9Slice_C` and stretch
— but the screen would not. The ability panel to the left is a fixed 673 wide,
both columns live in one `Overlay`, and at a 1920 design width they already sit
23 apart. A frame wide enough for ten cards at stock spacing (1188 + 76 = 1264)
would overlap that panel by some 245 and cover it. Making it scroll: the list has
no width constraint anywhere above it and the `Overlay`s all size to their
content, so constraining it means *inserting* a `SizeBox` export rather than
editing one, and the fix does not justify synthesising widgets.

**An earlier entry blamed the wrong widget.** It described the cards as "a
`ListView` inside a `SizeBox`". A UAssetAPI dump says otherwise: the two
`SizeBox`es in `WBP_FocusTree_SpecializationCards` are 32x32 and 320 wide and
both belong to the *locked* page of the switcher — a lock icon and a message.
Neither has ever touched the cards. "Widen the SizeBox" would have widened a
message box on a page the player does not see while the row overflowed exactly as
before. That is what a `strings` pass buys: the class names were right and every
value was missing.

**The talent row was the real cause of the reported defect, and it was checked
and cleared wrongly first.** That check asked the row's *capacity* — 74-wide
cards, 20 spacing, a 1042 frame inset 44/32 leaving 966, which holds ten
(10×74 + 9×20 = 920) — and never asked what the mod makes the count. Core adds
two talents, so it is twelve: 12×74 + 11×20 = **1108**, which is 142 over.

It did not read as broken because the panels measured about 1175 rather than
1042: the nine-slice art had stretched and the talent row was driving the width
of the whole stack. So the backing's `SizeBox` does not hold the frame at 1042
when its slot fills; the art follows the widest row.

That stretch was the reported defect twice over. Pulling every panel 133 wider
drags their left edges under the fixed 673 ability panel beside them, which is
the "SPECIALIZATION ABILITIES" card sitting on top of the talent row and "TALENT
ABILITY" clipped to "LENT ABILITY". Seen from the other end, the same stretch is
the wide empty space to the right of the specialization cards. One cause, two
complaints, and the second would have been fixed as an alignment question if the
row had not been measured first.

Refitted the same way: twelve talents at 66 with 12 between is 924, against the
920 stock uses for ten. Width and height move together because a talent card is
square at 74x74 and the passive ones draw a circle in it, so shrinking one axis
would draw ellipses.

**Confirmed in game.** Every row fits 966, the panels are back to their authored
1042, the full "TALENT ABILITY" label is visible, the ability panel is clear of
them, and the passive talent icons are still round.

**One case is knowingly left unfixed.** The commander carries more talents than
an ordinary operator — 12 stock, 14 with the module — and 14 at 66/12 is 1080
against 966, so that screen stretches by 114 and the row sits flush against both
insets. Ordinary operators are unaffected.

The fix is the same two floats, but it is bought by taking size off the icons for
*every* character to correct one screen, and it would not be final: the count has
moved 10 → 12 → 14 already, and no ceiling is available from the data, because no
talent part is hero-gated — eligibility comes from background or origin, and the
player-facing pool is around 17. Fitting 16 would put icons at 52 against a stock
74. The count-proof answer is to cap the list so it scrolls, and that needs a
`SizeBox` synthesised into the widget rather than a property edited, which is not
a trade worth making for presentation.

**SPEC 2 is two cards shorter than SPEC 1**, because Padawan and Warrior are
primary-only, so about 214 of slack sits at the right end of the secondary row.
Left alignment is kept deliberately: the two rows list the same specializations
in the same order, so it keeps the first four lined up vertically between them,
which right alignment would offset by two and align nothing.

---

### Foundations — seams, tooling and the build

**The game ships its own PDB, and it names everything.**
`SWZeroCompany/Binaries/Win64/SWZeroCompany.pdb` — 267 MB beside the executable,
1,286,644 public symbols with full C++ signatures.
`scripts/resolve-symbols.sh` resolves any of them;
[docs/SEAM-MAP.md](docs/SEAM-MAP.md) records what matters.

Section `0001` offset + `0x1000` is the RVA, checked against all eight addresses
the MIT baseline pins by hand — every one lands on its documented target, so the
mapping is verified rather than assumed.

Named with addresses in one pass: the whole `UCustomizationStatics` API including
four enumerating functions where one had been assumed; every fragment class a
part definition can carry; a specialization availability system
(`UBrunoCrossTrainingConfiguration`, `UBrunoStrategyStatics`,
`ABrunoBondsCentral`) driven by an authored data asset;
`FStanceAnimationSetTagMatch::MatchingSpecializationTags`, the mechanism behind
weapon-driven animation selection; and `UCharacterCustomizationSaveGameStatics`
for the persistence questions.

The PDB answers most questions about this game outright. It is the first place
to look, not the last.

**A native seam probe, to answer the enumerator question by measurement.**
`src/ZCOMOpenKitSeamProbe/` is an observation-only UE4SS C++ mod: it hooks all
four customization enumerators plus the requirements gate, calls the original in
every case, and changes nothing. It exists because appending to the wrong
enumerator is a silent no-op — the failure that had already cost three shipped
containers.

Two of the eight watched parts are controls the stock game already offers.
Without them a silent seam is ambiguous between "wrong seam" and "part is
locked"; with them it is not. One of the two was initially miscast:
`CPD_H_Outfit_Man001A_TORS` was described as a stock control and is in fact a
locked Mandalorian part, so its absence was the expected result rather than a
fault. `CPD_TacticalSpec_Soldier` was the only real control, and it did its job.

**Measured: there is one enumerator, and the slot is the question.** Of the four
functions that can enumerate customization parts, three were never called once
across a whole session. `GetCustomizationPartDefinitions` took all 34 calls, and
it serves every picker, returning results of 8, 10, 94 and 120 entries as
different screens opened.

The container the caller passes turns out to be more useful than expected: its
**first tag is the slot being filled** —
`br.Customization.Slot.Character.Specializations.Tactical.Primary` in the
specialization case — and the rest is the character's own set. That is what makes
a data-driven module possible instead of a pile of special cases.

**The hero parts are never enumerated, rather than enumerated and refused.** All
eight watched definitions resolved, hero parts included, after only two
enumerator calls. Seventy-five further calls followed, covering every
specialization slot and the wardrobe, and the accumulated set never grew past
`CPD_TacticalSpec_Soldier`. The probe could see the hero parts; the game never
offered one. So appending is the whole job, and nothing needs relaxing.

`CPD_H_Outfit_Man001A_TORS` behaves identically: resolved, and absent from the
107 torsos the wardrobe returns. The wardrobe lock and the specialization lock
are one mechanism, which is why one module lifts both.

The slot map, which became the module's configuration table:

```text
Specializations.Tactical.Primary      8      Outfit.Torso.Mesh    107
Specializations.Tactical.Secondary    8      Outfit.Helmet.Mesh    58
Specializations.Talent               10      Body.Height 3, Body.Type 2
```

**Identity is a slot.** The requirements gate is called per slot, and
`br.Customization.Slot.Character.Info.Name` is one of them, alongside
`Info.Faction`, `Info.Voiceover`, `Info.PortraitPose`, `Class` and `Species`. A
character's name is the part equipped in a Name slot. That is the cleanest
statement of why granting `Info.Name.<Hero>` grants the whole character: it is
not a label, it is the identity part, and what hangs off that character follows
it. The project's one rule has a mechanism behind it rather than only a bug
report.

**An ordinary operator carries no `Info.Name` tag at all.** Two operators across
two sessions, a Rodian and a Human, 38 and 43 tags: not one `Info.Name` tag in
any captured container, at any slot, through either seam. Hero parts require a
name key and an ordinary operator supplies none, so they are never found.

Twenty of those tags are `Accepts.Outfit.*` capabilities, which is why the MIT
wardrobe's scoped-copy technique is visible as the right shape in the game's own
data.

**`DoesPartIdMeetRequirements` ran 2,358 times and was never asked about a single
watched part**, Soldier included. It is not the specialization filter; that
happens inside the enumerator. It *is* the filter for outfits on selection, which
the wardrobe later found the hard way.

**A crash the probe introduced, and the rule that came out of it.** Probe 0.2.0
crashed the customization system. It presented as "a crash screen but the game
loaded", which is exactly what a UE4SS-caught access violation looks like: UE4SS
writes a `.dmp` beside itself and execution continues, so there is no UE crash
report to read.

`scripts/read-minidump.py` reads those dumps — nothing on a Linux box did — and
both were identical:

```text
exception 0xC0000005, access violation: read of 0x360
SWZeroCompany.exe + 0x448CD10 -> UAssetManager::Get(void)
```

0.2.0 added a resolution poll to `on_update` that called
`GetCustomizationPartDefinitionFromPartId`, which reaches `UAssetManager::Get()`.
About a second after startup there is no asset manager, so it dereferenced null.
0.1.0 made the same call and never crashed because it only ever made it from
*inside* a customization hook, where the game is demonstrably already using the
asset manager. The bug was moving a call out of the context that made it safe.

It cost the whole run: the probe's last line is one second after READY. A
diagnostic that stops reporting the moment it starts is worse than one that
reports nothing, because the log looks like a quiet successful session.

Two fixes, the second of which generalises. Resolution happens only inside hooks
and `on_update` merely reports what they recorded; and the native call is
SEH-guarded, so a probe cannot take the game down however wrong the calling
context is. Both are now standing rules in the module: never call into the game
from outside a hook, never log from inside one.

**A row whose slot tag does not resolve disables its own row only.** Refusing the
whole mod would let one renamed tag take away kits that still work. The
`Specializations.Weapon` slot was exactly that case — the PDB declares it as a
native gameplay tag but the probe never saw it called, so it shipped allowed to
fail alone. It resolved and fired.

**Options are one file per switch in `options/`, contents ignored.** There are 19
of them. A FOMOD cannot merge files, only place them, so with a single
`config.ini` every independent toggle doubles the preset files the installer has
to carry: three checkboxes is eight presets, four is sixteen. One fragment per
option keeps it linear.

The requirements that puts on the implementation: absent file means off; the
merge is order-independent; a missing `options/` directory is valid and means the
defaults, so someone who unzips only the DLL still gets a working mod; and an
unrecognised fragment is logged and ignored, so an older DLL survives a newer
install.

**Shipping as a FOMOD, which collapses three artifacts into two.** The ZCOM Mod
Manager already parses FOMOD, and Vortex covers this game through the
[Zero Company extension](https://www.nexusmods.com/site/mods/2174), so one FOMOD
archive serves Vortex, MO2 and the ZCOM manager alike.

`packaging/fomod/ModuleConfig.xml` sticks to the element subset all three
implement, and omits `moduleDependencies`, `fileDependency` and `gameDependency`
— the manager parses none of them and Vortex's file-state handling is unreliable.
UE4SS presence is stated in the option text rather than detected.
`scripts/check-fomod.py` enforces the subset, catches flags that are set but
never read (and the reverse), and verifies every source path exists in the staged
archive.

A second, plain **Manual** archive still ships. A FOMOD stores files under option
folders, so extracting one by hand gives the wrong layout; one archive cannot
honestly serve both.

**Hero-kit swapping is an option, and pinned is the default.** Unlocking
specialization, talent and weapon changes *after* a hero kit is equipped is a
separate choice from unlocking the kits, because it carries a real hazard: a
character holding a hero part is only valid while the mod applies. When it stops
applying, the game re-checks them on load and shows the slot as locked and
unchangeable. Native mods pinned to a build are especially exposed, since a patch
turns the unlock off all at once. Under "pinned", no operator can hold a part the
base game disallows, so the failure has no mechanism.

**The build cross-compiles on Linux, and never needed Windows.** An earlier entry
said the native module could not be built without Windows. That was wrong.
`clang-cl`, `lld-link` and `cargo` are all present, and `xwin` fetches the MSVC
CRT and Windows SDK into `$HOME` without root. Verified end to end: a PE32+
x86-64 DLL with a correct export table, compiled and linked on Linux.
`scripts/setup-native-toolchain.sh` records the two things that waste an hour
otherwise — `-fuse-ld=lld` is required, and `/winsysroot` does not work against
an xwin tree.

**The MIT baseline builds unchanged on Linux**, which is the known-good control
this project wanted before generalising anything.
`scripts/build-native.sh ../lab/upstream/ZeroCompanyMandoWardrobe` produces a
PE32+ x86-64 DLL exporting `start_mod` and `uninstall_mod`, with identity strings
matching upstream's shipped release, from a source tree that was not touched.

Three obstacles, each of which fails without pointing at itself:

- RE-UE4SS's `deps/first/Unreal` submodule is `Re-UE4SS/UEPseudo`, which mirrors
  Unreal Engine headers and therefore requires EpicGames GitHub organization
  membership. Unauthenticated it answers as if the repository does not exist.
- The submodule is SSH-URL'd, so an HTTPS token alone is not enough.
- A clang-cl mod does load into the shipped MSVC-built UE4SS without complaint.

The installed UE4SS is Git SHA `a1e7f571` — the exact commit upstream builds
against — so mods built here are ABI-matched to the loader that runs them.

**The installed game is PE-identical to the build the MIT baseline pins** —
`TimeDateStamp 0xE10ABE56`, `SizeOfImage 0x0E354000`, Steam build `24874058` — so
its hardcoded addresses are valid here unmodified. Fifteen RVAs are pinned in
this module, all PDB-resolved and byte-verified before hooking. Signature
scanning would survive a game patch and is not built.

**`scripts/stage-native-mod.sh` lays a built DLL out the way UE4SS loads it** —
`ue4ss/Mods/<Name>/dlls/main.dll` plus `enabled.txt`, with the rename to
`main.dll` that is the usual reason a hand-installed C++ mod silently does not
load.

**A Lua probe, so a data-only container can be observed at all.**
`src/ZCOMOpenKitProbe/` reads the picker's class default object back out of the
running game and writes one line naming the entry count and both lock tags, so a
container's effect can be stated rather than guessed. Every read is `pcall`-
guarded and UE4SS's TMap support varies by build, so the probe reports *how* it
counted (`foreach`, `length`, `unreadable`) — a nil result is then
distinguishable from an empty map rather than being reported as failure.

**`ZCOM-5.6.1.usmap` parses fine.** An earlier note here claimed it did not parse
with standard tooling. It parses in UAssetAPI without complaint — 32,263 schemas.
The failure that suggested otherwise was a mis-ordered command line feeding it a
`.uasset`.

---

### Superseded — the data-only route for Core

Core was designed as a pak first. That route was built, shipped in three
variants, debugged through several wrong diagnoses, and finally rejected. The
reasoning is kept because it is the evidence behind the project's one rule, and
because it documents the game's data model in a way nothing else here does.

**The first containers claimed the wrong package path and overrode nothing.**
`retoc` derives each package's path from where the file sits under the input
root, and `build-container.sh` was writing the edited asset into a flat directory
instead of the tree it was extracted from. The resulting container overrode
`BP_SpecializationSelectionVM` at a path the game never looks at.

The verification that passed it was the problem. Re-extracting the container and
reading ten entries back proved the *content* was right and said nothing about
the *path*, and an asset packed at the wrong path reads back perfectly. The build
now copies the whole extracted tree and edits in place, asserts the packed path
equals the extracted path, and mounts the container against the full shipped set
to confirm the packed asset is the one that comes back — a real override test rather
than a self-consistency check.

**The picker's list is an explicit array, not a predicate.**
`BP_SpecializationSelectionVM` holds `SpecializationPartMapping`, a `TMap` from
primary specialization to secondary — the eight the focus tree offers. Warrior
and Padawan are simply not on it. Core appends to it, which is the one container
edit that survives into the shipped design.

**Listing a kit is not the same as being allowed to take it.** Eligibility lives
on each part's `AllowedSlots`:

```text
CPD_TacticalSpec_Warrior  [Info.Name.ClyKullervo, Slot...Tactical.Primary]
CPD_TacticalSpec_Soldier  [                       Slot...Tactical.Primary]
```

Stripping the hero-name entry from the six kit parts was tried. It changed
nothing, and the reason is the finding that ended the whole approach.

**`Info.Name` grants, it does not restrict.** Two independent confirmations. The
ordinary `CPD_WeaponSpec_Blaster_Rifle` and `_Pistol` that every operator uses
both carry `Info.Name.ClyKullervo` and `Info.Name.KabbUppercut`; if those tags
restricted, nobody but Cly and Kabb could hold a rifle. And ZZCArmoryAddon makes
Rex's pistol usable by *adding* `Info.Name.CaptainRex` while leaving that asset's
`Accepts.AuthoredOnly` in place.

So `AuthoredOnly` is the gate and `Info.Name` is the allowlist of who gets in
anyway. Candidate parts are found *by* the name tags an operator carries, and a
part with no `Info.Name` in its `AllowedSlots` is never looked up. That is why
removal changed nothing, and why substituting `Info.Faction.ZeroCompany` did not
work either — wrong branch of the tag tree.

**An invented tag does not work, and neither does the registered parent.**
`Info.Name.OpenKit` did nothing, because gameplay tags are validated against a
registry the game builds from data and a pak cannot add to it. The registered
parent `Info.Name` did nothing either, which rules out registration as the whole
story: a parent does not satisfy a match on a leaf.

**Borrowing a name means borrowing the character.** Kabb Uppercut was chosen as
the least-harm candidate: no `CPD_Char_Class_Hero_*`, no `TacticalSpec`, no
`TalentSpec`, and the only parts naming him already available to everyone. The
build did not unlock the kits *and* it replaced the recruited operator's body —
the new operator came out as Kabb himself, a Herglic.

That generalises, and it closes the approach rather than the candidate. Any
registered `Info.Name` leaf belongs to a character who has *something*: a kit, a
body, or both. Grant Tel-Rea's and Cly's names and you get their animations and
weapons; grant an alien's and you get his species. There is no inert hero name,
because a hero name is precisely what is not inert.

Kabb's name appearing on some 1,100 character assets was noted at the time the
candidate was chosen, and treated as a footnote rather than as the body those
assets describe.

**So Core cannot be data-only** — not because a pak cannot reach the list, which
it demonstrably can via hero tags, but because the only data route that works
requires granting an identity. The rule does not bend to fit the mechanism; the
mechanism is rejected. Core moved to the native module, appending at the
catalogue seam, where no identity is involved at all.

**Nothing in authored data enumerates what the screen offers.**
`CD_Character_Default`, the recruit's customization definition, references no
specialization part at all — it defines slots, not candidates. The parts are
enumerated by the native catalogue behind `GetCustomizationPartDefinitions`, and
a part not in it is never offered however permissive its `AllowedSlots` become.

The evidence chain, all of it reproducible:

1. The container applies — `core=applied specializations=10/8` at runtime.
2. The picker VM is not consulted for listing — five hooks armed, none ever
   called, with the assign widget confirmed constructed.
3. `BitReactorCustomizationSlotViewModel` holds an equipped part and no candidate
   list, so the cards cannot come from the view model.
4. `CD_Character_Default` enumerates no parts.
5. The MIT wardrobe source had to append to the catalogue for exactly this
   reason.

This was recorded about the *wardrobe* on the first day — the picker catalogue is
native, so no pak can add to it — and then not applied to specializations, which
are enumerated by the same catalogue. Three containers were shipped on that gap.

**The data route also only ever reaches operators recruited after install.** An
existing operator's tag set is already baked into the save, which is a property
of the mechanism rather than of any particular mod. The native route has no such
limit: an operator who is already specialized can respec into a kit.

**Container modes kept only for the record.** `neutral`, `hero`, `company` and
`remove` all write identity or edit requirements. `mapping` is the only shippable
mode, and it edits one property in one asset.

---

### Where the identity route leads, measured

Two mods that ship data alone reach parts of the same feature set. Both were
read as **shipped data only** -- pak contents diffed against stock with `retoc`,
which reads the tags an asset carries. Those tags are properties of Zero
Company's own data model, and they confirm several of the findings above. The
mechanism below is the reason for this project's one rule.

**Classes Unlocked (Nexus 39)** adds `Info.Name.Tel-ReaVokoss` and
`Info.Name.ClyKullervo` to `CPD_Faction_ZeroCompany`, the definition covering the
entire company, alongside a capability tag (`Accepts.Outfit.Mdo`) that is exactly
the right shape. It also changes the picker's mapping array count from 8 to 10
and appends four `FPackageIndex` values, a delta of exactly the 16 bytes by which
its `.uexp` is larger. Those resolve to `CPD_TacticalSpec_Padawan`,
`CPD_TacticalSpec_PadawanExtended`, `CPD_TacticalSpec_Warrior` and a null -- the
null being correct, since no `CPD_TacticalSpec_Warrior_Secondary` exists.

The mechanism worth taking from that: identity tags placed on the faction land on
every character in the company, so everything downstream that keys on hero
identity fires for everyone. The lightsaber melee animation set, the Mandalorian
arm cannon in place of the soldier rocket launcher, and hero identity held from
the first character -- which the tutorial does not survive -- all follow from the
tags rather than from anything going wrong.

That is the trade a data-only pak makes, not an oversight. It is the only route a
pak has, and the consequences are documented rather than hidden. Open Kit pays a
different price for avoiding it: it needs UE4SS.

**ZZCArmoryAddon (Nexus 58)** adds Clone Commando armour, Rex's armour, the
DC-17M and Rex's pistol as its own category. Its pak modifies two assets:

```text
CPD_WeaponSpec_Blaster_Pistol_Rex   + Info.Name.CaptainRex
                                    + Specializations.Weapon.Blaster.DC17Rex
CPD_GK_Pistol_DC-17_Rex             + Specializations.Weapon.Blaster.DC17Rex
```

The second is a new specialization category the base game does not have, which is
how Rex's pistol becomes its own Armory list rather than joining the generic
pistols.

**A finding recorded here was wrong, and the game's data corrected it.** An
earlier note said Rex's pistol becomes its own Armory list *because the game
groups it that way*, and that Open Kit would therefore offer it behind a switch
because it regroups the weapon lists. That was reading another mod's edit as if
it were a property of the game. Read from the game's own cooked assets,
`CPD_WeaponSpec_Blaster_Pistol_Rex` grants the **generic**
`Specializations.Weapon.Blaster.Pistol`, so it joins the pistol list and nothing
is regrouped — and it is gated on `Accepts.AuthoredOnly` alone, not on Rex's
name. Open Kit invents no tag and writes no identity to reach it. The one part in
the game that wants Rex by name is his *gear kit*.

Rex still gets his own switch, for a smaller reason: his class shares the stock
pistol class's `DisplayName` and his kit shares the stock DC-17's, so both read
alike on screen.

**The wardrobe and Armory are one module rather than two.** Two DLLs hooking the
same customization functions compete for the same seam, and whether they coexist
depends on both implementations -- which is not something this project can
guarantee for a mod it does not control. One module with both features behind
option fragments cannot conflict with itself, and it halves the hooking surface.

**Second-hand claims about other mods' behaviour were checked rather than built
on, and two of three came back false.** The most consequential was that equipping
Tel-Rea's outfit gives full saber animations to an ordinary operator. If it held,
the wardrobe would have stopped being cosmetic and become the fix for the badly
held saber, reached without granting anyone an identity. It does not hold, and it
fails twice over — see the next section.

---

### Answered: an outfit cannot give anyone saber animations

Worth its own section, because it was the wardrobe's original headline feature
and the answer is a clean no from two directions.

**Only four fragment classes can contribute a tag to a character.** The PDB names
every implementer of `GetOwnedGameplayTags`, and
`UCustomizationFragmentInstanceOverrideSlots` is pointedly not one of them.
Tel-Rea's five outfit pieces carry UIData, Foley, Mesh and OverrideSlots and
nothing else, so they contribute nothing to anybody.
`Info.Name.Tel-ReaVokoss` sits on them only in `AllowedSlots`, where it is the
requirement to wear them — never a grant for having worn them.

**And it would not have mattered if it had.** Animation selection reads the
*weapon*: the weapon-spec part names its own `AnimSetSoft` and `ProxyTableSoft`
directly, and the stance archetypes match on `Rig.*` and
`Specializations.Weapon.*`. Not one `FStanceAnimationSetTagMatch` in the game
keys on `Info.Name`, and none keys on anything worn.

**An outfit can grant something, and what it grants is posture.** The three
Mandalorian legs parts contribute `Part.Character.Stance.Wide`. No outfit part in
the game grants an identity.

---

### Not from Open Kit: Final Shot draws two blank ability rows

Reported as a mod bug against a Jae screenshot — two empty entries under
SPECIALIZATION ABILITIES, one with a magenta square where the icon belongs, one
with a grey one. It is stock game data, and the measurement is short enough to
keep so the next report can be closed without repeating it.

**Final Shot is `Vengeance`.** The talent is
`CPD_TalentSpec_TheBaroness`, gated on `Info.Name.JaeMordant`, and the ability
behind it is `br.AbilityID.Passive.Vengeance` — the FinalShot name survives only
on the assets around it (`SM_FinalShot`, `GE_Cooldown_FinalShot_T*`,
`T_UI_Abilities_FinalShot`).

**The talent grants seven abilities, and only two of them are meant to be seen.**

```text
GA_Vengeance_T1 .. _T4    the four tiers of Final Shot
GA_CoilVendetta_T1        Coil Vendetta
GA_FinalShot_Status       internal
GA_VengeanceListener      internal
```

One active tier, plus Coil Vendetta, plus the two internals, is exactly the four
rows on the screen.

**What separates a row that draws from one that does not** is
`FBitReactorAbilityUIData`, read off the CDOs with UAssetAPI against
`ZCOM-5.6.1.usmap`:

| ability | `UIData.Name` | `UIData.Description` | icon | `ShouldShowInUI` |
|---|---|---|---|---|
| `GA_Vengeance_T1` | `GA_Vengeance_T1_Name` | `GA_Vengeance_T1_Tactical` | `T_UI_Abilities_FinalShot` | `True` |
| `GA_CoilVendetta_T1` | `GA_CoilVendetta_T1_Name` | `GA_CoilVendetta_T1_Tactical` | `T_UI_Passives_CoilVendetta` | `True` |
| `GA_FinalShot_Status` | unset | unset | `ResourceObject = 0` | unset |
| `GA_VengeanceListener` | unset | unset | none — a 64×64 size and nothing to draw | unset |

`ShouldShowInUI` is a native bool with no author override anywhere up either
parent chain, so both internals inherit a visible default and are drawn with
nothing to draw. The fallback subtitle reads PASSIVE either way:
`GA_FinalShot_Status` still carries `br.AbilityType.Passive`, and
`GA_VengeanceListener` carries no type tag at all.

**Every other talent is clean, and that was checked rather than assumed.** All 24
`CPD_TalentSpec_*` assets were dumped: The Baroness is the only player-facing one
that puts internal abilities in the granted array. The Clone grants
`GA_CloneArmorTraining` plus `GA_ForMyBrothers_T1..T4`, the Survivor grants
`GA_Grit_T1..T4`, and so on — display abilities only.
`CPD_TalentSpec_TheLeader_Infected` also carries a `GE_CanReceivePlague` in that
array, but it is story-only and never reaches a roster screen. So the reason it
is this talent and no other is that Final Shot is the one whose implementation
needed a status ability and an event listener, and neither was hidden.

**Why it is not this mod's.** `CPD_TalentSpec_TheBaroness` ships with the game,
Core offers only `CPD_TalentSpec_TheLostPadawan` and
`CPD_TalentSpec_TheMandalorian` in the Talent slot, and the Baroness is name-gated
to a hero the module never names. Removing the `~mods` containers reproduces it.

**Fixable, and not fixed.** A two-asset override setting
`UIData.ShouldShowInUI = false` on `GA_FinalShot_Status` and
`GA_VengeanceListener` would close it — a display flag, no gameplay effect, no
identity written, well within the container pipeline already built. Not shipped,
because it is a cosmetic defect in content this mod does not offer, on a hero
every player already has. Recorded here so the choice is a choice.

What must **not** be done is dropping the two internals from the talent's array.
They are what makes Final Shot trigger.
