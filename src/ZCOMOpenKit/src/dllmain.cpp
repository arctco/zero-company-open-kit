// Open Kit -- hero kits for everyone in Star Wars: Zero Company.
//
// One hook. The game assembles every list the player picks from in
// UCustomizationStatics::GetCustomizationPartDefinitions, and the first tag of
// the container it is passed is the slot being filled. So this appends parts to
// a list, keyed by slot, from a table:
//
//     Specializations.Tactical.Primary   <- Padawan, Warrior
//     Specializations.Talent             <- TheLostPadawan, TheMandalorian
//     Specializations.Weapon             <- Melee_2H_TelRea, Melee_1H_Anakin
//     Outfit.{Torso,Legs,Arms,Boots,Helmet}.Mesh
//                                        <- the Mandalorian wardrobe
//
// Adding robes, sabers, wardrobe items or weapons later is a row in that table,
// not another hook. The wardrobe was the first thing to prove that: it is
// sixteen rows and one extra scoped tag, and it needed no new seam.
//
// The one rule: change what the game offers, not who anyone is. Nothing here
// writes a tag to any character, faction, roster or save. The game's own
// requirements are left exactly as authored -- measurement showed the hero parts
// are never enumerated at all rather than enumerated and refused, so there is
// nothing to defeat, only something to offer. See docs/INVESTIGATION.md.
//
// Structure and safety practice follow Sternab's MIT-licensed Zero Company
// Mandalorian Wardrobe (https://github.com/Sternab/ZeroCompanyMandoWardrobe,
// copyright (c) 2026 Sternab): refuse an unverified build before hooking
// anything, guard native calls, and never log from inside a hook.
//
// Copyright (c) 2026 Open Kit contributors. MIT.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/FPrimaryAssetId.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <polyhook2/Detour/x64Detour.hpp>

#include "expanded_colours.hpp"
#include "helmet_render_fit.hpp"

namespace
{
    using RC::Unreal::FName;
    using RC::Unreal::FPrimaryAssetId;
    using RC::Unreal::FPrimaryAssetType;
    using RC::Unreal::TArray;
    using RC::Unreal::UObject;

    static_assert(sizeof(FName) == 8, "requires an eight-byte FName ABI");
    static_assert(sizeof(FPrimaryAssetId) == 16, "requires a 16-byte FPrimaryAssetId ABI");

    // Steam build 24874058. Addresses resolved from the PDB the game installs
    // beside its executable; scripts/resolve-symbols.sh reproduces them.
    constexpr std::uint32_t kExpectedPeTimestamp = 0xE10ABE56;
    constexpr std::uint32_t kExpectedImageSize = 0x0E354000;

    constexpr std::uintptr_t kGetCustomizationPartDefinitionsRva = 0x63C54D0;
    constexpr std::array<std::uint8_t, 16> kGetCustomizationPartDefinitionsBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48,
    };

    constexpr std::uintptr_t kGetPartDefinitionFromPartIdRva = 0x63C5660;
    constexpr std::array<std::uint8_t, 16> kGetPartDefinitionFromPartIdBytes{
        0x4C, 0x8B, 0xDC, 0x53, 0x55, 0x48, 0x81, 0xEC,
        0xB8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x01, 0x33,
    };

    // Asking the game whether a character may actually take a part, rather than
    // appending and hoping. All four verified against this build's PDB and the
    // bytes read out of the shipped executable; see THIRD_PARTY_NOTICES.md.
    //
    // The technique -- copy the character's tag container, add to the *copy* the
    // one tag being deliberately overridden, put the copy to the game's own
    // requirement check, destroy the copy -- is taken from Sternab's
    // ZeroCompanyMandoWardrobe, MIT licensed, with thanks. See
    // THIRD_PARTY_NOTICES.md.
    constexpr std::uintptr_t kDoesPartIdMeetRequirementsRva = 0x63C6400;
    constexpr std::array<std::uint8_t, 16> kDoesPartIdMeetRequirementsBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
        0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18, 0x4C,
    };

    constexpr std::uintptr_t kTagContainerCopyCtorRva = 0x40F7B20;
    constexpr std::array<std::uint8_t, 16> kTagContainerCopyCtorBytes{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x33, 0xC0,
        0x48, 0x8B, 0xD9, 0x48, 0x89, 0x01, 0x48, 0x89,
    };

    // Shared with every other trivially-destructible struct in the image by
    // identical code folding, which is why the PDB lists a dozen names at this
    // address. That is expected and harmless.
    constexpr std::uintptr_t kTagContainerDtorRva = 0x16C2E60;
    constexpr std::array<std::uint8_t, 16> kTagContainerDtorBytes{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
        0xD9, 0x48, 0x8B, 0x49, 0x10, 0x48, 0x85, 0xC9,
    };

    constexpr std::uintptr_t kTagContainerAddTagRva = 0x41017A0;
    constexpr std::array<std::uint8_t, 16> kTagContainerAddTagBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x48, 0x8B, 0x02, 0x48, 0x8B, 0xF9,
    };

    // The kit parts each name exactly one hero in their AllowedSlots, read from
    // the cooked assets rather than assumed: the Padawan family requires
    // Tel-Rea, the Warrior family requires Cly.
    constexpr const RC::Unreal::TCHAR* kTelReaName =
        STR("br.Customization.Part.Character.Info.Name.Tel-ReaVokoss");
    constexpr const RC::Unreal::TCHAR* kClyName =
        STR("br.Customization.Part.Character.Info.Name.ClyKullervo");
    // Anakin's saber only. His *class* is deliberately not offered, and the
    // reason is worth keeping: the game authors tactical specs in two families,
    // and they are told apart by what their AllowedSlots require.
    //
    //   player   Slot.Character.Specializations.Tactical.Primary
    //            Soldier, Assault, Sniper ... and Padawan and Warrior
    //   scripted Accepts.Character.Specializations
    //            Anakin, Rex, and every single Enemy_* spec
    //
    // Padawan and Warrior are player specializations that happen to be gated on
    // a hero's name, which is exactly why lifting that name is a legitimate
    // thing for this mod to do. Anakin and Rex are not player specializations at
    // all; they are assigned to scripted pawns. Offering one would mean scoping
    // in `Accepts.Character.Specializations`, the same capability every enemy
    // spec requires -- claiming an operator is an NPC pawn rather than lifting a
    // name gate, and outside the rule.
    //
    // The requirement check caught this on its own, which is the argument for
    // having built it: `declined part=CPD_TacticalSpec_Anakin`, before the card
    // could reach a player and do something undefined.
    constexpr const RC::Unreal::TCHAR* kAnakinName =
        STR("br.Customization.Part.Character.Info.Name.Anakin");

    // The wardrobe's capability tag, and the reason the wardrobe is a different
    // shape of lift from the kits. Every Mandalorian outfit part requires
    // `Accepts.Outfit.Mdo`, which is not a name -- it is the same kind of tag as
    // the fourteen `Accepts.Outfit.*` entries an ordinary operator already
    // carries (Wor, Sin, Mtc, Mfr, Lar, Har, Gra, For, Fli, Clo, Coi, Civ, Pyk,
    // Bsp). Mdo is simply not among them. Scoping it in says "suppose this
    // operator could wear Mandalorian armour", which is a statement about
    // wardrobe capability and about nothing else.
    //
    // Nine of the nineteen parts additionally require Cly's name, and those get
    // the same one-name lift the kits already use.
    constexpr const RC::Unreal::TCHAR* kMdoOutfit =
        STR("br.Customization.Accepts.Outfit.Mdo");

    // The rest of the company's names, for the hero wardrobe. Every one is a
    // name a part asks for in its own AllowedSlots, read out of the cooked
    // assets by scripts/generate-wardrobe-table.py rather than typed.
    constexpr const RC::Unreal::TCHAR* kHawksName =
        STR("br.Customization.Part.Character.Info.Name.Hawks");
    constexpr const RC::Unreal::TCHAR* kKabbName =
        STR("br.Customization.Part.Character.Info.Name.KabbUppercut");
    constexpr const RC::Unreal::TCHAR* kJaeName =
        STR("br.Customization.Part.Character.Info.Name.JaeMordant");
    constexpr const RC::Unreal::TCHAR* kBakerName =
        STR("br.Customization.Part.Character.Info.Name.Baker");
    constexpr const RC::Unreal::TCHAR* kDozenName =
        STR("br.Customization.Part.Character.Info.Name.Dozen");
    constexpr const RC::Unreal::TCHAR* kSawtoothName =
        STR("br.Customization.Part.Character.Info.Name.Sawtooth");
    constexpr const RC::Unreal::TCHAR* kLucoName =
        STR("br.Customization.Part.Character.Info.Name.LucoBronc");
    constexpr const RC::Unreal::TCHAR* kCommanderName =
        STR("br.Customization.Part.Character.Info.Name.TheCommander");
    constexpr const RC::Unreal::TCHAR* kTrickName =
        STR("br.Customization.Part.Character.Info.Name.Trick");
    constexpr const RC::Unreal::TCHAR* kVisserName =
        STR("br.Customization.Part.Character.Info.Name.Visser");
    // Rex is here rather than with the wardrobe names because his *armour* does
    // not ask for it -- CPD_H_Outfit_CaptainRexA_* are gated on AuthoredOnly and
    // have shipped in the wardrobe since it landed. The only part in the game
    // that wants Rex by name is his gear kit.
    constexpr const RC::Unreal::TCHAR* kCaptainRexName =
        STR("br.Customization.Part.Character.Info.Name.CaptainRex");

    // The one gate in this game that nothing satisfies.
    //
    // 623 outfit parts require `Accepts.AuthoredOnly`, and a sweep of all 3,362
    // customization part definitions found **no part anywhere that grants it**.
    // It is not a capability a character can acquire; it is a mark on content
    // meaning "assigned by authoring, never chosen by a player", and the
    // requirement check answers no to it for everybody. That is consistent with
    // what the game showed: authored characters could wear Mandalorian armour
    // that Hawks could not, because authored characters are dressed by preset
    // rather than by passing this check.
    //
    // It is scoped in for four parts only -- Anakin's arms, boots and legs, and
    // the Dozen, Sawtooth and Commander sets, which ask for it alongside a name.
    // It is a category, not a person: unlike a hero's `Info.Name` it names
    // nobody, so it cannot pull one character's content onto another, which is
    // the failure the project's rule exists to prevent. It is still the widest
    // thing this module lifts, so it is lifted only for parts that are listed
    // here by name and never as a blanket.
    constexpr const RC::Unreal::TCHAR* kAuthoredOnly =
        STR("br.Customization.Accepts.AuthoredOnly");

    // Each option is one file in options/. A FOMOD can place files but cannot
    // merge them, so one file per switch keeps the installer's preset count
    // linear instead of doubling with every new toggle.
    enum Option : std::size_t
    {
        kOptionPadawan = 0,
        kOptionWarrior,
        kOptionPadawanSecondary,
        kOptionPadawanSaber,
        kOptionWardrobe,
        kOptionWardrobeCly,
        kOptionWardrobeJedi,
        kOptionWardrobeHeroes,
        kOptionWardrobeAnakin,
        kOptionWardrobeAuthored,
        kOptionWardrobeHelmets,
        kOptionWardrobeFamilies,
        kOptionWardrobeStory,
        kOptionColours,
        kOptionColoursExtra,
        kOptionArmouryDC17M,
        kOptionArmouryRexPistol,
        kOptionArmouryHeroWeapons,
        kOptionArmourySabers,
        kOptionCount,
    };

    struct OptionSpec
    {
        const RC::Unreal::TCHAR* file_name;
        bool on_by_default;
    };

    constexpr std::array<OptionSpec, kOptionCount> kOptions{{
        {STR("padawan.ini"), true},
        {STR("warrior.ini"), true},
        // On, and it used to be off because the row under it named the wrong
        // part. The game offers two Jedi specializations -- only one was
        // showing, and the other is the one carrying Lightsaber Throw.
        //
        // Both exist, and they are a primary/secondary pair:
        //
        //   CPD_TacticalSpec_Padawan          Tactical.PRIMARY
        //     Force Push, Shockwave, Padawan Training, Noble Defense
        //   CPD_TacticalSpec_PadawanExtended  Tactical.SECONDARY
        //     Lightsaber Throw, Force Pull, An Elegant Weapon
        //
        // Both are gated on Tel-Rea's name and nothing else, so the lift is the
        // one this mod already makes for the primary.
        //
        // This row used to offer CPD_TacticalSpec_Padawan in the SECONDARY
        // slot, where its own AllowedSlots says Primary -- so it could never be
        // valid there, which is exactly the "empty specialization" recorded
        // earlier and blamed on the part not existing. It exists; it is just
        // the one asset in the family that does not use the `_Secondary` suffix
        // the other eleven use.
        {STR("padawan-secondary.ini"), true},
        // Off by default, and it is the reason this switch exists at all.
        // Offering Tel-Rea's saber in the Weapon slot puts it in the weapon
        // class list during *character creation*, where the created pawn cannot
        // satisfy it: the card refuses to take the selection, and the recruit
        // reaches the tutorial holding nothing at all, unable to fire the shot
        // the tutorial requires. That is a softlock, and it is precisely the
        // failure this project exists not to reproduce.
        //
        // It works on an existing operator -- confirmed in a mission -- but even
        // there it is wrong: the blade is permanently ignited and held in the
        // wrong grip, because the animations that hold a saber properly are
        // keyed to a hero's identity, which this mod deliberately never grants.
        // A weapon that is safe only on one path and looks wrong on it does not
        // belong on by default.
        {STR("padawan-saber.ini"), false},
        // On by default, unlike the saber, and the difference is not nerve. The
        // saber was switched off because it reached character creation and was
        // offered to a pawn that could not take it; the requirement check that
        // cures that did not exist yet. It does now, and these rows were written
        // behind it from the first line rather than retro-fitted to it. They
        // also carry no ability, no attribute set and no gameplay effect -- an
        // outfit part is a mesh, a foley and a stance -- so the worst a wrong
        // answer can cost here is a tile that does not appear.
        {STR("wardrobe.ini"), true},
        // Cly's own armour, separated from the generic Mandalorian kits because
        // it is a named character's personal set rather than a family of
        // armour, and a player may reasonably want the one without the other.
        {STR("wardrobe-cly.ini"), true},
        // Tel-Rea's robes. Offered after all, having first been left out on the
        // reasoning that her pieces ship a single `SK_HSTF_*` mesh -- short,
        // thin, feminine -- so there would be nothing to draw on anyone else.
        // That reasoning does not survive contact with the evidence: upstream
        // reports at runtime that Cly's feminine-only helmet meshes resolve on
        // a masculine Hawks, so a mesh option's body tags are a best-match hint
        // and not a gate. The same conclusion falls out of the stock data, where
        // every outfit in the game is tagged `Height.Average`/`Weight.Average`
        // and the game plainly dresses short and heavy operators in them.
        //
        // So they will draw. Whether they *fit* is a separate question and the
        // player's to judge, which is the same answer the two saber grips got.
        {STR("wardrobe-jedi.ini"), true},
        // Everybody else's clothes: fifty-four parts across ten characters,
        // generated from the game rather than typed. See
        // scripts/generate-wardrobe-table.py for what is excluded and why --
        // backpacks, faces, and any part that replaces the wearer's body rather
        // than dressing it.
        {STR("wardrobe-heroes.ini"), true},
        // Anakin's, separately, because his is the only outfit split across both
        // gates -- his torso names him, his arms, boots and legs ask only to be
        // worn by an authored character -- and because a player may want the
        // Jedi robes without the rest of the cast's wardrobe.
        {STR("wardrobe-anakin.ini"), true},
        // The rest of the game's clothes: 245 parts whose only gate is
        // `Accepts.AuthoredOnly`. Nothing in the game grants that tag -- a sweep
        // of all 3,362 customization parts found zero -- so it reads as
        // "assigned by authoring, never chosen by a player". Whether it could be
        // lifted at all was an open question until Anakin's arms, boots and legs
        // were offered behind it and the game accepted them.
        {STR("wardrobe-authored.ini"), true},
        // The reason two operators see different wardrobes. 75 helmets require
        // `Accepts.Helm.Human`, and only Human, Clone, Mirialan and Umbaran
        // carry it -- a Rodian, Togruta or Zabrak is offered a much smaller set.
        // This is the one gate here that is about *fit* rather than permission:
        // a helmet shaped for a human skull has to sit on montrals, lekku or
        // horns. Lifting it is what makes every operator see the same list, and
        // what it costs is a look. That is the player's call, so it is a switch.
        {STR("wardrobe-helmets.ini"), true},
        // Every outfit family, offered to every character regardless of what
        // their class grants -- which is what makes the *authored* heroes able
        // to wear what Hawks wears. The capability comes from the character's
        // class, and the heroes' classes are far poorer than an operator's:
        //
        //     Hero_Humanoid (Hawks, every recruit)   14 families
        //     Hero_Umbaran  (Luco)                   11
        //     Hero_Mando    (Cly)                     1 -- only Mdo
        //     Hero_Padawan  (Tel-Rea)                 0 -- none at all
        //
        // So this is the reverse of the bug that made the wardrobe unwearable on
        // Hawks: there, an operator lacked a capability a hero had; here, a hero
        // lacks fourteen an operator has. It costs Hawks nothing, because the
        // catalogue hook will not append a definition the game already listed
        // and adding a tag the character already carries changes no answer.
        {STR("wardrobe-families.ini"), true},
        // Off by default, and the only switch here that is off. These 116 parts
        // are gated on `Accepts.Unlocks.Story_*`, which is campaign progress: the
        // game grants them as the story is played. Turning this on hands over
        // clothes the save has not earned yet. That is a legitimate thing to want
        // and it is not identity, so it is offered -- but it changes the game's
        // pacing rather than lifting a lock, which is why it is opt-in.
        {STR("wardrobe-story.ini"), false},
        // The colours, and a third seam. 335 of the game's 455 outfit colours
        // and most of its weapon paints carry `Accepts.AuthoredOnly`, so the
        // palette a player picks from is smaller than the one that shipped --
        // the same lock the wardrobe had, on the same tag.
        //
        // They are NOT reachable from the catalogue table: colours never pass
        // through `GetCustomizationPartDefinitions` at all, which is why they
        // carry no slot tag. They come from
        // `UCustomizationPartsTagSubsystem::FilterAssetDataByTags`, and that is
        // its own hook in expanded_colours.cpp.
        //
        // The palette offered is Sternab's curated one, and the curation earns
        // its place: of the 335 locked outfit colours, 82 are within 0.02 of a
        // colour the player already has and 126 more within 0.05. Offering all
        // of them would mean a picker of near-identical swatches.
        {STR("colours.ini"), true},
        // The rest of the locked palette: 95 more colours, every one at least
        // 0.05 in linear RGB from anything the game already offers, so none of
        // them is a swatch a player cannot tell from one they have. Generated by
        // scripts/generate-colour-table.py rather than picked.
        //
        // The six the generator excludes by name are upstream's exclusions and
        // are right: Tester_A/B/G/R are developer test ramps, and SlugratRed and
        // SlugratRed_Lite are a character's colour rather than a palette entry.
        //
        // Needs colours.ini -- this widens that palette, it does not replace it.
        {STR("colours-extra.ini"), true},
        // The Armory, and it is three switches because it is three weapons a
        // player would ask for separately. Every row is generated -- see
        // scripts/generate-armoury-table.py, whose header argues each inclusion
        // and each exclusion out of the game's cooked assets.
        //
        // Two of the four things the roadmap called the Armory turned out to be
        // shipped already: Clone Commando armour (CPD_H_Outfit_Clo010_*) and
        // Rex's armour (CPD_H_Outfit_CaptainRexA_*) are outfits, and the
        // wardrobe swept every outfit in the game. All fourteen parts are
        // already rows in wardrobe_table.inc. So the Armory is weapons.
        //
        // Trick's clone-commando rifle: a complete, fully authored weapon with
        // three fire modes (Longarm, Repeater, Launcher), its own ability set,
        // its own attributes, its own animations, its own UI icon and its own
        // weapon-customization model. It is locked twice -- by Trick's name, and
        // by `Accepts.Unlocks.DC17M`, which is required by that one part and
        // granted by NONE of the game's 3,363 customization parts. That is the
        // same shape as `Accepts.AuthoredOnly`: a gate nothing satisfies.
        //
        // Its gear kit, CPD_GK_DC-17M, is deliberately NOT a row. Its only
        // requirement is the category tag this class grants, so a character who
        // has equipped the class carries it and the game lists the kit itself.
        // If the DC-17M appears as a weapon class and its Armory list is empty,
        // that prediction is what was wrong.
        {STR("armoury-dc17m.ini"), true},
        // Rex's pistol, which is two parts: the weapon class that carries his
        // shot, overwatch, DualFire and GunDown abilities and his own
        // attributes, and the gear kit that equips his own DC-17 model.
        //
        // It is its own switch for a presentation reason, not a gameplay one.
        // The class shares the stock pistol class's DisplayName and the kit
        // shares the stock DC-17's, so both read alike on screen -- a player who
        // would rather not have two identically-named cards leaves this off.
        //
        // A correction to what this project recorded about it. INVESTIGATION §6
        // says Rex's pistol becomes its own Armory list because a new
        // specialization category is added to it. That is an edit ZZCArmoryAddon
        // makes, not a property of the game. In the game's own data the class
        // grants the generic
        // `Specializations.Weapon.Blaster.Pistol`, so it joins the pistol list
        // and nothing is regrouped -- and it is gated on `AuthoredOnly` alone,
        // not on Rex's name. Open Kit therefore invents no tag and writes no
        // identity to reach it.
        {STR("armoury-rex-pistol.ini"), true},
        // The rest of what the Armory locks and this module can reach: Cly's
        // pistol and Hawks's pistol as gear kits, and three weapon models --
        // Hawks's pistol, the S-5 and the E-5.
        //
        // Four of those five carry a display name and no icon. Unversioned
        // serialization omits a property at its default, so that is an empty
        // brush rather than a part that failed to parse: the tile draws its name
        // and no picture. A presentation cost on a working weapon, which is the
        // player's call, which is why it is a switch.
        {STR("armoury-hero-weapons.ini"), true},
        // The saber hilts, and this switch exists because a rule was wrong.
        //
        // The generator excluded both of these for having no `DisplayName`,
        // on a rule read off the stock *blaster* kits, every one of which is
        // named. No lightsaber gear kit in the game is named -- including
        // `CPD_GK_Lightsaber_TelRea`, which is the hilt a Padawan is already
        // holding, because it is her class's DefaultPart. A nameless tile is
        // the norm in that list, not a defect.
        //
        // What it offers: the Second Sister's hilt
        // (`GK_Lightsaber_SecondSister_Trilla` ->
        // `BP_LightSaber_Enemy_Imperial_Trilla`), and Tel-Rea's own back again.
        // The second is load-bearing rather than a courtesy: her hilt is in no
        // list, so offering the Inquisitor's without it would let a player
        // switch away from the stock saber with no way to switch back.
        //
        // Both need `Melee.2H`, which only Tel-Rea's saber class grants, so
        // nothing here can appear unless `padawan-saber.ini` is also on. That
        // is why it is on by default despite the saber itself being off.
        {STR("armoury-sabers.ini"), true},
    }};

    // At most four requirements are ever lifted for one part. It was two when
    // the wardrobe was Mandalorian armour, and three once every outfit family
    // was scoped rather than assumed granted; four is headroom, and the
    // generator prints the widest lift it emitted so this can be checked.
    constexpr std::size_t kMaxScopedTags = 4;

    // The table. Slot tag in, part to append out.
    struct CatalogueRow
    {
        const RC::Unreal::TCHAR* slot_tag;
        const RC::Unreal::TCHAR* part_id;
        Option option;
        // The requirements this mod lifts for this part, and nothing else.
        // A null entry is unused; every row has at least one.
        //
        // What is deliberately *not* here matters as much as what is. The
        // wardrobe parts also require `Part.Character.Rig.Humanoid`, and that
        // stays the game's call: an Astromech or a BX droid is refused
        // Mandalorian armour by the game's own check, exactly as authored, and
        // this mod never learns it happened.
        std::array<const RC::Unreal::TCHAR*, kMaxScopedTags> scoped_tags;
    };

    // These kits are primary specializations, and that is the game's decision
    // rather than ours. Every one of the eight stock specializations has a
    // `_Secondary` counterpart part -- Assault_Secondary, Soldier_Secondary and
    // so on -- which is what the picker equips when you choose it for the second
    // slot. Padawan and Warrior are the only two with no `_Secondary` asset
    // anywhere in the game, so offering them there produces a specialization
    // with no abilities at all. 0.2.0 did exactly that; this does not.
    constexpr auto kCatalogue = std::to_array<CatalogueRow>({
        {STR("br.Customization.Slot.Character.Specializations.Tactical.Primary"),
         STR("CPD_TacticalSpec_Padawan"), kOptionPadawan, {kTelReaName}},
        {STR("br.Customization.Slot.Character.Specializations.Talent"),
         STR("CPD_TalentSpec_TheLostPadawan"), kOptionPadawan, {kTelReaName}},
        // Two sabers, one switch, because the game gives no way to choose for
        // the player. Tel-Rea's is two-handed and Anakin's is one-handed, and
        // which of them animates correctly depends on the body wearing it:
        // a masculine operator holds the two-handed grip badly.
        //
        // Picking automatically is not possible: there is no gender or sex
        // anywhere in the customization tag space. `Rig` distinguishes only
        // Humanoid, Astromech and BX, and `Species` is species. So both grips
        // are offered and the player keeps whichever looks right, which is the
        // better answer anyway -- it is a choice, not a guess.
        {STR("br.Customization.Slot.Character.Specializations.Weapon"),
         STR("CPD_WeaponSpec_Melee_2H_TelRea"), kOptionPadawanSaber, {kTelReaName}},
        {STR("br.Customization.Slot.Character.Specializations.Weapon"),
         STR("CPD_WeaponSpec_Melee_1H_Anakin"), kOptionPadawanSaber, {kAnakinName}},

        {STR("br.Customization.Slot.Character.Specializations.Tactical.Primary"),
         STR("CPD_TacticalSpec_Warrior"), kOptionWarrior, {kClyName}},
        {STR("br.Customization.Slot.Character.Specializations.Talent"),
         STR("CPD_TalentSpec_TheMandalorian"), kOptionWarrior, {kClyName}},

        // The Padawan secondary, and it needs no container. This row was
        // recorded as needing the pairing container because the game ships no
        // Padawan secondary and the container maps Padawan to PadawanExtended
        // to fake one. PadawanExtended is not a fake: it declares
        // `Slot.Character.Specializations.Tactical.Secondary` itself, carries
        // Lightsaber Throw, Force Pull and An Elegant Weapon, and asks only for
        // Tel-Rea's name. Naming it here is all that was needed.
        //
        // What was actually wrong is that this row named CPD_TacticalSpec_Padawan,
        // whose AllowedSlots is Primary -- a part that cannot be valid in the
        // slot it was being offered for. Warrior still has no equivalent
        // anywhere in the game, so it is not offered here at any setting.
        {STR("br.Customization.Slot.Character.Specializations.Tactical.Secondary"),
         STR("CPD_TacticalSpec_PadawanExtended"), kOptionPadawanSecondary, {kTelReaName}},

    // The wardrobe: 477 rows, generated from the game's own cooked assets by
    // scripts/generate-wardrobe-table.py. It is not hand-maintained and should
    // not be edited -- regenerate it. The script's header is the argument for
    // every row that is in it and every part that is not.
#include "wardrobe_table.inc"

    // The Armory: 8 rows across three slots, generated from the same cooked
    // assets by scripts/generate-armoury-table.py. Not hand-maintained --
    // regenerate it. The script's header is the argument for every row in it
    // and for the 178 parts that are not.
#include "armoury_table.inc"
    });

    // Once a bitmask, and it stopped scaling twice -- at 32 rows when the
    // wardrobe landed, and again at 64 when the hero wardrobe did. A flag per
    // row costs one byte each and has no ceiling to walk into a third time.
    //
    // The array size is deduced rather than written, for the same reason: it was
    // wrong twice while it was a literal.
    static_assert(kCatalogue.size() > 400, "the generated wardrobe table is missing");

    struct GameplayTag
    {
        FName tag_name{};
    };

    struct GameplayTagContainer
    {
        TArray<GameplayTag> gameplay_tags{};
        TArray<GameplayTag> parent_tags{};
    };

    static_assert(sizeof(GameplayTag) == 8, "requires an eight-byte FGameplayTag ABI");
    static_assert(sizeof(GameplayTagContainer) == 32, "requires a 32-byte FGameplayTagContainer ABI");

    using GetCustomizationPartDefinitionsFn = void(__fastcall*)(const GameplayTagContainer*,
                                                               TArray<const UObject*>*);
    using GetPartDefinitionFromPartIdFn = const UObject*(__fastcall*)(const FPrimaryAssetId*);
    using DoesPartIdMeetRequirementsFn = bool(__fastcall*)(const FPrimaryAssetId*, const GameplayTagContainer*);
    using TagContainerCopyCtorFn = GameplayTagContainer*(__fastcall*)(GameplayTagContainer*,
                                                                     const GameplayTagContainer*);
    using TagContainerDtorFn = void(__fastcall*)(GameplayTagContainer*);
    using TagContainerAddTagFn = void(__fastcall*)(GameplayTagContainer*, const GameplayTag*);

    std::uintptr_t g_base{};
    std::uintptr_t g_image_size{};
    std::array<bool, kOptionCount> g_option_enabled{};
    std::array<bool, kCatalogue.size()> g_row_enabled{};
    std::array<FName, kCatalogue.size()> g_slot_names{};
    std::array<FPrimaryAssetId, kCatalogue.size()> g_part_ids{};
    std::array<std::array<GameplayTag, kMaxScopedTags>, kCatalogue.size()> g_scoped_tags{};
    std::array<std::size_t, kCatalogue.size()> g_scoped_tag_counts{};
    std::array<std::atomic<bool>, kCatalogue.size()> g_declined_seen{};
    std::array<std::atomic<bool>, kCatalogue.size()> g_declined_pending{};
    // The pick, as opposed to the offer. The catalogue seam says what was put
    // in front of the player; this says what happened when they clicked it, and
    // until now nothing did. A save showed an operator carrying Rex's weapon
    // class and the *stock* pistol kit -- which is either a tile picked by
    // mistake or a selection the game refused, and no line in the log told the
    // two apart. Recorded once per row, with the answer, like the others.
    std::array<std::atomic<bool>, kCatalogue.size()> g_picked_seen{};
    std::array<std::atomic<bool>, kCatalogue.size()> g_picked_pending{};
    std::array<bool, kCatalogue.size()> g_picked_answer{};
    std::array<std::atomic<std::uint32_t>, kCatalogue.size()> g_pick_count{};
    // A differential probe, not a feature. Both saber hilts require
    // Melee.2H alongside AuthoredOnly; this module scopes only AuthoredOnly, so
    // a refusal means the character is missing the category -- but "means" is an
    // inference and this makes it a measurement. Recorded per ask, by FName
    // comparison only: FName::ToString must never be called from inside the seam.
    constexpr const RC::Unreal::TCHAR* kMelee2HTag =
        STR("br.Customization.Part.Character.Specializations.Weapon.Melee.2H");
    FName g_melee_2h_name{};
    // Every operator in the player's company carries this; it is the tag the
    // container builder's notes call "the tag set every operator in the roster
    // carries". Whether a pawn part-way through character creation carries it is
    // the open question, and the whole of the tutorial fix depends on it: if the
    // creation pawn lacks it, the kits can be gated on being in the company
    // rather than on detecting a tutorial, which the module has no reliable way
    // to do and which would break on the next patch.
    constexpr const RC::Unreal::TCHAR* kCompanyFactionTag =
        STR("br.Customization.Part.Character.Info.Faction.ZeroCompany");
    FName g_company_faction_name{};
    std::array<std::uint8_t, kCatalogue.size()> g_pick_had_category{};
    std::array<std::uint16_t, kCatalogue.size()> g_pick_tag_count{};
    // The differential. The same question is put to the game a second time with
    // the two-handed category added to the copy as well, and only the FIRST
    // answer is ever returned -- this one is recorded and thrown away.
    //
    // It settles a disagreement no amount of re-reading could. Reported from the
    // game: Tel-Rea and Trick can select the Inquisitor hilt and Hawks cannot,
    // with the same weapon class equipped. Either Hawks is missing the category
    // (and this reads true) or AuthoredOnly is not the tag-shaped gate this
    // module has modelled it as (and this reads false, with the character
    // carrying the category). Nothing in the game's data grants AuthoredOnly --
    // not one of 3,363 customization parts, and not one of 793 character
    // definitions -- so the second answer is the interesting one.
    std::array<std::uint8_t, kCatalogue.size()> g_pick_answer_with_category{};
    // The second differential, and the one the evidence now points at.
    //
    // The same call returns TRUE at the catalogue seam for an outfit whose only
    // lift is AuthoredOnly, and FALSE at the requirement seam for a saber hilt
    // with the same tag scoped. Same function, same technique, opposite answers
    // -- so the difference is in the container, not the tag.
    //
    // INVESTIGATION §8 measured that the catalogue seam's container carries the
    // SLOT TAG as its first entry, ahead of the character's own tags. The pick
    // path may not. This asks the same question with the row's slot tag added to
    // the copy; discarded, never returned.
    std::array<std::uint8_t, kCatalogue.size()> g_pick_answer_with_slot{};
    std::array<std::uint16_t, kCatalogue.size()> g_offer_tag_count{};
    std::array<std::uint8_t, kCatalogue.size()> g_offer_in_company{};
    // DoesPartIdMeetRequirements runs the game's own fragment checks. If any of
    // them ever reaches back through the catalogue seam, this stops the hook
    // recursing into itself rather than trusting that it never will.
    thread_local bool t_in_requirement_check = false;
    std::array<std::atomic<const UObject*>, kCatalogue.size()> g_definitions{};
    std::array<std::atomic<bool>, kCatalogue.size()> g_applied_seen{};
    std::array<std::atomic<bool>, kCatalogue.size()> g_applied_pending{};
    std::uint64_t g_trampoline{};
    std::unique_ptr<PLH::x64Detour> g_hook{};
    // The second seam. Appending a part to a list only makes the tile appear;
    // the game asks again, unscoped, when the player picks it. See the hook.
    std::uint64_t g_requirements_trampoline{};
    std::unique_ptr<PLH::x64Detour> g_requirements_hook{};
    std::atomic<bool> g_requirements_hook_active{false};
    bool g_helmet_fit_active{false};
    bool g_colours_active{false};

    auto bytes_match(std::uintptr_t rva, const std::array<std::uint8_t, 16>& expected) -> bool
    {
        return rva + expected.size() <= g_image_size &&
               std::memcmp(reinterpret_cast<const void*>(g_base + rva), expected.data(), expected.size()) == 0;
    }

    auto validate_runtime(const char*& reason) -> bool
    {
        const auto module = ::GetModuleHandleW(nullptr);
        if (module == nullptr)
        {
            reason = "main-module-not-found";
            return false;
        }

        const auto base = reinterpret_cast<std::uintptr_t>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        {
            reason = "invalid-dos-header";
            return false;
        }

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->FileHeader.TimeDateStamp != kExpectedPeTimestamp ||
            nt->OptionalHeader.SizeOfImage != kExpectedImageSize)
        {
            reason = "retail-pe-identity-mismatch";
            return false;
        }

        g_base = base;
        g_image_size = static_cast<std::uintptr_t>(nt->OptionalHeader.SizeOfImage);

        if (!bytes_match(kGetCustomizationPartDefinitionsRva, kGetCustomizationPartDefinitionsBytes) ||
            !bytes_match(kGetPartDefinitionFromPartIdRva, kGetPartDefinitionFromPartIdBytes) ||
            !bytes_match(kDoesPartIdMeetRequirementsRva, kDoesPartIdMeetRequirementsBytes) ||
            !bytes_match(kTagContainerCopyCtorRva, kTagContainerCopyCtorBytes) ||
            !bytes_match(kTagContainerDtorRva, kTagContainerDtorBytes) ||
            !bytes_match(kTagContainerAddTagRva, kTagContainerAddTagBytes))
        {
            reason = "pdb-target-byte-mismatch";
            return false;
        }
        return true;
    }

    // The mod's own directory, found from this DLL rather than from UE4SS
    // internals: .../Mods/ZCOMOpenKit/dlls/main.dll -> .../Mods/ZCOMOpenKit
    auto mod_directory(HMODULE self) -> std::filesystem::path
    {
        std::array<wchar_t, MAX_PATH * 4> buffer{};
        const auto length = ::GetModuleFileNameW(self, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size())
        {
            return {};
        }
        return std::filesystem::path(buffer.data()).parent_path().parent_path();
    }

    // A missing options/ directory means the defaults, so someone who unzipped
    // only the DLL still gets a working mod. A present directory is taken
    // literally: absent file means off.
    auto read_options(HMODULE self) -> void
    {
        for (std::size_t index = 0; index < kOptionCount; ++index)
        {
            g_option_enabled[index] = kOptions[index].on_by_default;
        }

        const auto root = mod_directory(self);
        if (root.empty())
        {
            return;
        }
        const auto options = root / L"options";

        std::error_code error{};
        if (!std::filesystem::is_directory(options, error) || error)
        {
            return;
        }

        for (auto& enabled : g_option_enabled)
        {
            enabled = false;
        }

        for (const auto& entry : std::filesystem::directory_iterator(options, error))
        {
            if (error || !entry.is_regular_file(error) || error)
            {
                continue;
            }
            const auto name = entry.path().filename().wstring();
            bool known = false;
            for (std::size_t index = 0; index < kOptionCount; ++index)
            {
                if (name == kOptions[index].file_name)
                {
                    g_option_enabled[index] = true;
                    known = true;
                    break;
                }
            }
            // An older DLL must survive a newer fragment rather than refuse to
            // load, so an unrecognised file is noted and ignored.
            if (!known)
            {
                RC::Output::send<RC::LogLevel::Warning>(
                    STR("[ZCOM_OPEN_KIT] option_ignored file={} reason=unknown-to-this-version\n"), name);
            }
        }
    }

    // Reaches UAssetManager::Get(), which faults if the asset manager is not up.
    // Only ever called from inside the hook, where the game is demonstrably
    // already in its own customization system -- and guarded even so, because a
    // mod must not be able to take the game down. No C++ object with a
    // destructor may live in a frame using __try, and none does here.
    auto guarded_get_part_definition(const FPrimaryAssetId* id) -> const UObject*
    {
        __try
        {
            const auto get_part =
                reinterpret_cast<GetPartDefinitionFromPartIdFn>(g_base + kGetPartDefinitionFromPartIdRva);
            return get_part(id);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    // Ask the game's own requirement check whether this character could take
    // this part, with only the row's named requirements lifted: the hero
    // Info.Name a kit part carries in its AllowedSlots, and for the wardrobe the
    // Accepts.Outfit.Mdo capability as well.
    //
    // That short list is the entire mod, stated as a question instead of an
    // assumption. Every other requirement a part declares -- species, rig, slot
    // capability, roster level, whatever a future part brings -- is still
    // answered by the game, on the real character, and a "no" is taken as a no.
    // The wardrobe made that concrete: its parts also demand Rig.Humanoid, which
    // is not on the list, so a droid is refused Mandalorian armour by the game
    // rather than by anything written here.
    //
    // The tags go on a *copy* of the character's tags which is destroyed before
    // this returns. Nothing is written to the character, and the game is never
    // told anybody is Tel-Rea or Cly; it is asked a hypothetical and the answer
    // is thrown away with the copy.
    //
    // This is what the character-creation softlock needed. A pawn being created
    // fails for a reason that has nothing to do with the hero name, so it fails
    // here too, and the part is simply not offered.
    auto guarded_meets_requirements(const FPrimaryAssetId* id,
                                    const GameplayTagContainer* owned,
                                    const GameplayTag* scoped,
                                    std::size_t scoped_count) -> bool
    {
        __try
        {
            const auto copy_ctor =
                reinterpret_cast<TagContainerCopyCtorFn>(g_base + kTagContainerCopyCtorRva);
            const auto add_tag =
                reinterpret_cast<TagContainerAddTagFn>(g_base + kTagContainerAddTagRva);
            const auto destroy = reinterpret_cast<TagContainerDtorFn>(g_base + kTagContainerDtorRva);
            // The trampoline, not the address: once the requirement seam is
            // hooked, that address is our own detour, and scoping a copy that
            // is already a scoped copy would be a lie told twice.
            const auto meets = reinterpret_cast<DoesPartIdMeetRequirementsFn>(
                g_requirements_trampoline != 0 ? g_requirements_trampoline
                                               : g_base + kDoesPartIdMeetRequirementsRva);

            // Raw storage, trivially destructible: no C++ object with a
            // destructor may live in a frame using __try.
            alignas(GameplayTagContainer) std::byte storage[sizeof(GameplayTagContainer)]{};
            auto* copy = reinterpret_cast<GameplayTagContainer*>(&storage[0]);

            copy_ctor(copy, owned);
            for (std::size_t index = 0; index < scoped_count; ++index)
            {
                add_tag(copy, &scoped[index]);
            }
            const bool allowed = meets(id, copy);
            destroy(copy);
            return allowed;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // A part that cannot be asked about is a part that is not offered.
            return false;
        }
    }

    auto definition_for(std::size_t row) -> const UObject*
    {
        if (const auto* cached = g_definitions[row].load(std::memory_order_acquire); cached != nullptr)
        {
            return cached;
        }

        const UObject* definition = guarded_get_part_definition(&g_part_ids[row]);
        if (definition == nullptr || !UObject::IsReal(const_cast<UObject*>(definition)) ||
            const_cast<UObject*>(definition)->IsUnreachable())
        {
            // Not an error: the asset may not be loaded yet, and the next call
            // through this seam tries again.
            return nullptr;
        }

        g_definitions[row].store(definition, std::memory_order_release);
        return definition;
    }

    // Queue a row for logging the first time it happens, and never again. The
    // seam is hit on every redraw of the picker; one line each is enough.
    auto record_row(std::array<std::atomic<bool>, kCatalogue.size()>& seen,
                    std::array<std::atomic<bool>, kCatalogue.size()>& pending,
                    std::size_t row) -> void
    {
        if (!seen[row].exchange(true, std::memory_order_relaxed))
        {
            pending[row].store(true, std::memory_order_release);
        }
    }

    // Once-only is the right amount of noise for six hundred wardrobe rows and
    // the wrong amount for a row being actively debugged. A part declined the
    // first time the picker opened and offered five minutes later logged the
    // decline and nothing else, so "was it offered after the class changed?"
    // could not be answered from the log -- and that was exactly the question.
    //
    // This logs a row's answer whenever it *changes*, which is silent while
    // nothing is happening and says so the moment something does. Used for the
    // Armory rows only; the wardrobe keeps once-only.
    enum class RowAnswer : std::uint8_t
    {
        kUnknown = 0,
        kOffered,
        kDeclined,
        // The game put the part in the list itself, so the hook skipped it.
        // Worth its own state: a tile can be on screen while this module's row
        // reads "declined", and without this the log looks like a contradiction.
        // That is exactly what happened with the saber hilts -- the class's
        // DefaultPart put them in the list, the hook's early-out skipped them,
        // and the last thing recorded stayed `offered=false` from minutes before.
        kGameListed,
    };

    std::array<std::atomic<RowAnswer>, kCatalogue.size()> g_last_answer{};
    std::array<std::atomic<bool>, kCatalogue.size()> g_answer_changed{};

    auto record_answer_state(std::size_t row, RowAnswer now) -> void
    {
        if (g_last_answer[row].exchange(now, std::memory_order_relaxed) != now)
        {
            g_answer_changed[row].store(true, std::memory_order_release);
        }
    }

    auto record_answer_change(std::size_t row, bool offered) -> void
    {
        record_answer_state(row, offered ? RowAnswer::kOffered : RowAnswer::kDeclined);
    }

    auto is_armoury_row(std::size_t row) -> bool
    {
        const Option option = kCatalogue[row].option;
        return option == kOptionArmouryDC17M || option == kOptionArmouryRexPistol ||
               option == kOptionArmouryHeroWeapons || option == kOptionArmourySabers;
    }

    // Rows worth logging every time their answer changes rather than once ever:
    // the kits and the Armory, seventeen of them. The wardrobe's six hundred
    // keep once-only, which is what once-only is for.
    //
    // Once-only has now cost three test cycles by hiding the answer to a
    // question actively being asked -- a hilt that logged one refusal and went
    // quiet for twenty more, saber rows that logged one decline before a class
    // change and nothing after, and Padawan logging its offer at 00:05:37 and
    // never again however many times the character creator was reopened. A
    // default that suppresses the second observation is wrong for anything under
    // investigation.
    auto is_tracked_row(std::size_t row) -> bool
    {
        const Option option = kCatalogue[row].option;
        return is_armoury_row(row) || option == kOptionPadawan || option == kOptionWarrior ||
               option == kOptionPadawanSecondary || option == kOptionPadawanSaber;
    }

    auto names_equal(const FName& left, const FName& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FName)) == 0;
    }

    auto array_contains(const TArray<const UObject*>& objects, const UObject* target) -> bool
    {
        for (const auto* object : objects)
        {
            if (object == target)
            {
                return true;
            }
        }
        return false;
    }

    auto ids_equal(const FPrimaryAssetId& left, const FPrimaryAssetId& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FPrimaryAssetId)) == 0;
    }

    // The second seam, and the thing that made the wardrobe visible but
    // unwearable.
    //
    // Appending a part to a list only makes the tile appear. When the player
    // actually picks it, the game asks `DoesPartIdMeetRequirements` again --
    // against the real character, with nothing scoped -- and a Mandalorian
    // outfit requires `Accepts.Outfit.Mdo`, which an ordinary operator does not
    // have. Measured, not guessed: the capability is granted by the character's
    // *class* part, and `CPD_Char_Class_Hero_Mando` (Cly's) is the only ordinary
    // one that grants it. `CPD_Char_Class_Hero_Humanoid`, which Hawks and every
    // recruit carry, grants fourteen `Accepts.Outfit.*` capabilities and not
    // that one -- an exact match for the fourteen measured on a live operator.
    //
    // So the tile appeared and the selection was refused, which is the same
    // shape as the saber's character-creation failure, and was predicted
    // before the test found it.
    //
    // This answers the same scoped question the catalogue path already asks, and
    // only ever for the module's own part ids. Every other part in the game --
    // and there are some 2,400 of these calls a session -- goes straight to the
    // original with nothing touched.
    auto hook_does_part_id_meet_requirements(const FPrimaryAssetId* id,
                                             const GameplayTagContainer* owned) -> bool
    {
        const auto original = reinterpret_cast<DoesPartIdMeetRequirementsFn>(g_requirements_trampoline);
        if (original == nullptr)
        {
            return false;
        }
        // Already inside a scoped question of our own: answer it plainly.
        if (id == nullptr || owned == nullptr || t_in_requirement_check)
        {
            return original(id, owned);
        }

        for (std::size_t row = 0; row < kCatalogue.size(); ++row)
        {
            if (!g_row_enabled[row] || !ids_equal(*id, g_part_ids[row]))
            {
                continue;
            }

            t_in_requirement_check = true;
            const bool allowed = guarded_meets_requirements(id, owned,
                                                            g_scoped_tags[row].data(),
                                                            g_scoped_tag_counts[row]);
            t_in_requirement_check = false;
            g_picked_answer[row] = allowed;
            // Non-armoury rows never run the probes below, and the arrays are
            // zero-initialised -- which logged as "false" and read exactly like a
            // measured answer. It briefly did: CPD_TacticalSpec_Padawan appeared
            // to have been asked with an empty container. Say "not-asked".
            g_pick_answer_with_category[row] = 2;
            g_pick_answer_with_slot[row] = 2;
            g_pick_tag_count[row] = 0;
            g_pick_had_category[row] = 0;
            // Armoury rows log EVERY ask, not the first one only. Once-only has
            // now hidden the answer to the question being asked twice in a row:
            // a hilt asked at 22:52:09 logged once, and every click after that
            // went unrecorded, so "did the click even reach the check" could not
            // be answered. Picks are user-initiated and rare; there is no volume
            // problem here, and the wardrobe's six hundred rows keep once-only.
            if (is_armoury_row(row))
            {
                // Does the character actually carry the two-handed category at
                // the moment the game asks? Both containers are checked: a tag
                // can arrive as a parent of something else.
                std::uint8_t found = 0;
                std::uint16_t count = 0;
                if (!g_melee_2h_name.IsNone())
                {
                    for (const auto* list : {&owned->gameplay_tags, &owned->parent_tags})
                    {
                        count = static_cast<std::uint16_t>(count + list->Num());
                        for (int index = 0; index < list->Num(); ++index)
                        {
                            if (names_equal(list->GetData()[index].tag_name, g_melee_2h_name))
                            {
                                found = 1;
                            }
                        }
                    }
                }
                g_pick_had_category[row] = found;
                g_pick_tag_count[row] = count;

                // Diagnostic only: the answer below is discarded, never returned.
                std::uint8_t with_category = 2;  // 2 = not asked
                if (!g_melee_2h_name.IsNone() &&
                    g_scoped_tag_counts[row] < kMaxScopedTags)
                {
                    auto probe = g_scoped_tags[row];
                    probe[g_scoped_tag_counts[row]].tag_name = g_melee_2h_name;
                    with_category = guarded_meets_requirements(
                        id, owned, probe.data(), g_scoped_tag_counts[row] + 1) ? 1 : 0;
                }
                g_pick_answer_with_category[row] = with_category;

                // The slot probe answered and is retired: adding the slot tag
                // turned PASSING parts false, so the slot tag must not be in the
                // character's container at all. It stays as a control -- it
                // proves scoping does change answers, so a false elsewhere is a
                // real refusal and not an inert mechanism.
                //
                // This asks with Tel-Rea's NAME instead, which is the live
                // question. At this seam her weapon spec answers true and its
                // only lift is her name, while the hilts answer false and their
                // lift is AuthoredOnly. If a name flips a hilt to true then the
                // hilts want an identity, and the fix is one more entry in the
                // row's scoped tags -- the technique this module already uses on
                // a discarded copy for every kit and every hero garment.
                std::uint8_t with_name = 2;
                if (g_scoped_tag_counts[row] < kMaxScopedTags)
                {
                    const FName tel_rea(kTelReaName, RC::Unreal::FNAME_Find);
                    if (!tel_rea.IsNone())
                    {
                        auto probe = g_scoped_tags[row];
                        probe[g_scoped_tag_counts[row]].tag_name = tel_rea;
                        with_name = guarded_meets_requirements(
                            id, owned, probe.data(), g_scoped_tag_counts[row] + 1) ? 1 : 0;
                    }
                }
                g_pick_answer_with_slot[row] = with_name;

                g_pick_count[row].fetch_add(1, std::memory_order_relaxed);
                g_picked_pending[row].store(true, std::memory_order_release);
            }
            else
            {
                record_row(g_picked_seen, g_picked_pending, row);
            }
            return allowed;
        }

        return original(id, owned);
    }

    auto hook_get_customization_part_definitions(const GameplayTagContainer* owned_tags,
                                                 TArray<const UObject*>* output) -> void
    {
        const auto original = reinterpret_cast<GetCustomizationPartDefinitionsFn>(g_trampoline);
        if (original == nullptr)
        {
            return;
        }

        original(owned_tags, output);
        if (owned_tags == nullptr || output == nullptr || owned_tags->gameplay_tags.Num() <= 0 ||
            t_in_requirement_check)
        {
            return;
        }

        // The first tag is the slot being filled. Everything after it is the
        // character's own tag set, which this mod reads and never alters.
        const FName slot = owned_tags->gameplay_tags.GetData()[0].tag_name;

        for (std::size_t row = 0; row < kCatalogue.size(); ++row)
        {
            if (!g_row_enabled[row] || !names_equal(slot, g_slot_names[row]))
            {
                continue;
            }

            const UObject* definition = definition_for(row);
            if (definition == nullptr)
            {
                continue;
            }
            if (array_contains(*output, definition))
            {
                // Nothing to do -- but say so, because "the game is already
                // offering this" and "this module offered it" look identical on
                // screen and completely different in the log.
                if (is_tracked_row(row))
                {
                    record_answer_state(row, RowAnswer::kGameListed);
                }
                continue;
            }

            // The catalogue says what this mod would like to offer. This asks
            // the game whether this particular character may have it, and takes
            // no for an answer.
            t_in_requirement_check = true;
            const bool allowed = guarded_meets_requirements(&g_part_ids[row], owned_tags,
                                                            g_scoped_tags[row].data(),
                                                            g_scoped_tag_counts[row]);
            t_in_requirement_check = false;

            if (is_tracked_row(row))
            {
                std::uint16_t count = 0;
                std::uint8_t in_company = 0;
                for (const auto* list : {&owned_tags->gameplay_tags, &owned_tags->parent_tags})
                {
                    count = static_cast<std::uint16_t>(count + list->Num());
                    if (g_company_faction_name.IsNone())
                    {
                        continue;
                    }
                    for (int index = 0; index < list->Num(); ++index)
                    {
                        if (names_equal(list->GetData()[index].tag_name, g_company_faction_name))
                        {
                            in_company = 1;
                        }
                    }
                }
                g_offer_tag_count[row] = count;
                g_offer_in_company[row] = in_company;
            }
            if (is_tracked_row(row))
            {
                record_answer_change(row, allowed);
            }

            if (!allowed)
            {
                record_row(g_declined_seen, g_declined_pending, row);
                continue;
            }

            output->Add(definition);

            // Measured where the offer actually happens. Character creation
            // reached this seam and was offered Padawan and Warrior; whether the
            // pawn is distinguishable from a real operator decides whether the
            // fix can be a requirement rather than a special case.
            {
                std::uint16_t count = 0;
                std::uint8_t in_company = 0;
                for (const auto* list : {&owned_tags->gameplay_tags, &owned_tags->parent_tags})
                {
                    count = static_cast<std::uint16_t>(count + list->Num());
                    if (g_company_faction_name.IsNone())
                    {
                        continue;
                    }
                    for (int index = 0; index < list->Num(); ++index)
                    {
                        if (names_equal(list->GetData()[index].tag_name, g_company_faction_name))
                        {
                            in_company = 1;
                        }
                    }
                }
                g_offer_tag_count[row] = count;
                g_offer_in_company[row] = in_company;
            }

            record_row(g_applied_seen, g_applied_pending, row);
        }
    }

    class ZCOMOpenKitMod final : public RC::CppUserModBase
    {
      public:
        explicit ZCOMOpenKitMod(HMODULE self) : m_self(self)
        {
            ModName = STR("ZCOMOpenKit");
            ModVersion = STR("0.4.0");
            ModDescription = STR("Padawan and Warrior kits and the Mandalorian wardrobe offered to every operator, without granting anyone a hero's identity");
            ModAuthors = STR("Open Kit contributors");
            ModIntendedSDKVersion = STR("5.6");
        }

        ~ZCOMOpenKitMod() override
        {
            if (g_colours_active)
            {
                ZCOMOpenKit::ExpandedColours::shutdown();
                g_colours_active = false;
            }
            if (g_helmet_fit_active)
            {
                ZCOMOpenKit::HelmetRenderFit::shutdown();
                g_helmet_fit_active = false;
            }
            // The requirement hook first: it calls into nothing of ours, while
            // the catalogue hook's scoped question routes through the
            // requirement trampoline.
            if (g_requirements_hook)
            {
                g_requirements_hook->unHook();
                g_requirements_hook.reset();
            }
            g_requirements_trampoline = 0;
            g_requirements_hook_active.store(false, std::memory_order_release);
            if (g_hook)
            {
                g_hook->unHook();
                g_hook.reset();
            }
            g_trampoline = 0;
            RC::Output::send<RC::LogLevel::Verbose>(STR("[ZCOM_OPEN_KIT] unloaded hooks_active=false\n"));
        }

        auto on_unreal_init() -> void override
        {
            const char* reason = "ok";
            if (!validate_runtime(reason))
            {
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZCOM_OPEN_KIT] REFUSED reason={} hooks_active=false\n"), RC::ensure_str(reason));
                return;
            }

            read_options(m_self);

            g_melee_2h_name = FName(kMelee2HTag, RC::Unreal::FNAME_Find);
            g_company_faction_name = FName(kCompanyFactionTag, RC::Unreal::FNAME_Find);

            const FName asset_type_name(STR("CustomizationPartDefinition"), RC::Unreal::FNAME_Add);
            for (std::size_t row = 0; row < kCatalogue.size(); ++row)
            {
                g_part_ids[row] = FPrimaryAssetId{
                    FPrimaryAssetType{asset_type_name},
                    FName(kCatalogue[row].part_id, RC::Unreal::FNAME_Add),
                };
                // FNAME_Find, not Add: a slot tag this build does not know is a
                // sign the game changed under us, and inventing the name would
                // hide that behind a hook that silently never matches.
                g_slot_names[row] = FName(kCatalogue[row].slot_tag, RC::Unreal::FNAME_Find);
                // Same policy for every scoped tag: one this build does not
                // know means the requirement check could not be scoped honestly,
                // so the row goes quiet rather than falling back to appending
                // blind. A row with no scoped tags at all would be appending
                // blind by definition, so that counts as unknown too.
                std::size_t scoped_count = 0;
                bool scoped_known = true;
                for (const auto* tag : kCatalogue[row].scoped_tags)
                {
                    if (tag == nullptr)
                    {
                        continue;
                    }
                    const FName resolved(tag, RC::Unreal::FNAME_Find);
                    if (resolved.IsNone())
                    {
                        scoped_known = false;
                        break;
                    }
                    g_scoped_tags[row][scoped_count++].tag_name = resolved;
                }
                g_scoped_tag_counts[row] = scoped_count;
                scoped_known = scoped_known && scoped_count > 0;

                // One unknown slot disables its own row and nothing else. The
                // alternative -- refusing the mod outright -- would let a single
                // renamed tag take away kits that still work perfectly well.
                const bool wanted = g_option_enabled[kCatalogue[row].option];
                const bool slot_known = !g_slot_names[row].IsNone();
                g_row_enabled[row] = wanted && slot_known && scoped_known;
                if (wanted && !slot_known)
                {
                    RC::Output::send<RC::LogLevel::Warning>(
                        STR("[ZCOM_OPEN_KIT] row_disabled part={} reason=slot-tag-not-found slot={}\n"),
                        kCatalogue[row].part_id,
                        kCatalogue[row].slot_tag);
                }
                else if (wanted && !scoped_known)
                {
                    RC::Output::send<RC::LogLevel::Warning>(
                        STR("[ZCOM_OPEN_KIT] row_disabled part={} reason=scoped-tag-not-found\n"),
                        kCatalogue[row].part_id);
                }
            }

            std::size_t enabled_rows = 0;
            for (const bool enabled : g_row_enabled)
            {
                enabled_rows += enabled ? 1 : 0;
            }

            // Counted, not written down. The Armory is eight rows in a table of
            // six hundred and seventy, so "the switch is on and nothing
            // happened" is invisible in the total -- which is precisely the
            // failure the extra colours shipped with, where a palette of 135
            // reported 40 because the line that read the switch never landed.
            // A separate field says how many Armory rows survived option
            // reading, slot resolution and tag resolution.
            std::size_t armoury_total = 0;
            std::size_t armoury_rows = 0;
            for (std::size_t row = 0; row < kCatalogue.size(); ++row)
            {
                const Option option = kCatalogue[row].option;
                if (option != kOptionArmouryDC17M && option != kOptionArmouryRexPistol &&
                    option != kOptionArmouryHeroWeapons && option != kOptionArmourySabers)
                {
                    continue;
                }
                ++armoury_total;
                armoury_rows += g_row_enabled[row] ? 1 : 0;
            }
            if (enabled_rows == 0)
            {
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZCOM_OPEN_KIT] REFUSED reason=no-enabled-rows hooks_active=false\n"));
                return;
            }

            g_hook = std::make_unique<PLH::x64Detour>(g_base + kGetCustomizationPartDefinitionsRva,
                                                      reinterpret_cast<std::uint64_t>(
                                                          &hook_get_customization_part_definitions),
                                                      &g_trampoline);
            if (!g_hook->hook() || g_trampoline == 0)
            {
                g_hook.reset();
                g_trampoline = 0;
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZCOM_OPEN_KIT] REFUSED reason=catalogue-hook-install-failed hooks_active=false\n"));
                return;
            }

            // Second, and not fatal if it fails. Without it the tiles appear and
            // cannot be picked; with the catalogue hook alone the mod is still
            // worth having for the specializations, which the game does not
            // re-check on selection. So a failure here degrades rather than
            // refuses, and says so.
            g_requirements_hook = std::make_unique<PLH::x64Detour>(
                g_base + kDoesPartIdMeetRequirementsRva,
                reinterpret_cast<std::uint64_t>(&hook_does_part_id_meet_requirements),
                &g_requirements_trampoline);
            if (!g_requirements_hook->hook() || g_requirements_trampoline == 0)
            {
                g_requirements_hook.reset();
                g_requirements_trampoline = 0;
                RC::Output::send<RC::LogLevel::Warning>(
                    STR("[ZCOM_OPEN_KIT] degraded reason=requirement-hook-install-failed ")
                    STR("effect=parts-are-offered-but-may-refuse-selection\n"));
            }
            else
            {
                g_requirements_hook_active.store(true, std::memory_order_release);
            }

            // Third, and also not fatal. Without it the Mandalorian helmets are
            // wearable and a wide face clips through the visor, which is a
            // presentation defect rather than a broken mod. It validates its own
            // seven addresses and declines to install if any of them moved.
            bool any_wardrobe = false;
            for (std::size_t row = 0; row < kCatalogue.size(); ++row)
            {
                any_wardrobe = any_wardrobe || g_row_enabled[row];
            }
            if (any_wardrobe)
            {
                g_helmet_fit_active = ZCOMOpenKit::HelmetRenderFit::initialize();
            }

            // Fourth, and independent of everything above: its own seam, its own
            // two hooks, and no shared state. A failure degrades to no colours.
            if (g_option_enabled[kOptionColours])
            {
                g_colours_active = ZCOMOpenKit::ExpandedColours::initialize(
                    g_option_enabled[kOptionColoursExtra]);
            }

            // One line per switch would be eleven placeholders and counting,
            // and the count moved three times in a day. The enabled switches are
            // named instead, so the line stays readable and says what is on.
            RC::StringType switches{};
            for (std::size_t index = 0; index < kOptionCount; ++index)
            {
                if (!g_option_enabled[index])
                {
                    continue;
                }
                if (!switches.empty())
                {
                    switches += STR(",");
                }
                switches += kOptions[index].file_name;
            }

            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT] READY hooks_active=true build=24874058 ")
                STR("seam=GetCustomizationPartDefinitions-post seam2={} seam3={} helmet_fit={} ")
                STR("rows={}/{} armoury_rows={}/{} switches={} ")
                STR("identity_tags_written=0 character_edits=0 requirements=asked-per-character ")
                STR("lifted=named-capability-or-hero-name-on-a-discarded-copy\n"),
                g_requirements_hook_active.load(std::memory_order_acquire)
                    ? STR("DoesPartIdMeetRequirements-scoped")
                    : STR("NOT-INSTALLED"),
                g_colours_active ? STR("FilterAssetDataByTags") : STR("off"),
                g_helmet_fit_active,
                static_cast<int>(enabled_rows),
                static_cast<int>(kCatalogue.size()),
                static_cast<int>(armoury_rows),
                static_cast<int>(armoury_total),
                switches);
        }

        // Reported from here rather than from the hook: the seam is hot, and
        // FName::ToString is not something to call from inside it.
        auto on_update() -> void override
        {
            // The fit's own bookkeeping: it rescans the component registry when
            // the game says a customization was refreshed, and reports counters.
            // Nothing here calls into the game.
            if (g_helmet_fit_active)
            {
                ZCOMOpenKit::HelmetRenderFit::update();
            }
            if (g_colours_active)
            {
                ZCOMOpenKit::ExpandedColours::update();
            }

            for (std::size_t row = 0; row < kCatalogue.size(); ++row)
            {
                if (!g_applied_pending[row].exchange(false, std::memory_order_acquire))
                {
                    continue;
                }
                RC::Output::send<RC::LogLevel::Verbose>(
                    STR("[ZCOM_OPEN_KIT] offered part={} slot={} character_tags={} in_company={}\n"),
                    kCatalogue[row].part_id,
                    kCatalogue[row].slot_tag,
                    static_cast<int>(g_offer_tag_count[row]),
                    g_offer_in_company[row] != 0);
            }

            for (std::size_t row = 0; row < kCatalogue.size(); ++row)
            {
                if (!g_declined_pending[row].exchange(false, std::memory_order_acquire))
                {
                    continue;
                }
                // Not an error. This is the mod declining to offer a part to a
                // character the game says cannot have it -- which is the whole
                // point of asking.
                RC::Output::send<RC::LogLevel::Verbose>(
                    STR("[ZCOM_OPEN_KIT] declined part={} slot={} reason=character-fails-other-requirements\n"),
                    kCatalogue[row].part_id,
                    kCatalogue[row].slot_tag);
            }

            for (std::size_t row = 0; row < kCatalogue.size(); ++row)
            {
                if (!g_answer_changed[row].exchange(false, std::memory_order_acquire))
                {
                    continue;
                }
                const auto state = g_last_answer[row].load(std::memory_order_relaxed);
                RC::Output::send<RC::LogLevel::Verbose>(
                    STR("[ZCOM_OPEN_KIT] row_state part={} source={} character_tags={} in_company={}\n"),
                    kCatalogue[row].part_id,
                    state == RowAnswer::kOffered      ? STR("open-kit-appended")
                    : state == RowAnswer::kGameListed ? STR("game-already-lists-it")
                                                      : STR("declined"),
                    static_cast<int>(g_offer_tag_count[row]),
                    g_offer_in_company[row] != 0);
            }

            for (std::size_t row = 0; row < kCatalogue.size(); ++row)
            {
                if (!g_picked_pending[row].exchange(false, std::memory_order_acquire))
                {
                    continue;
                }
                // The requirement seam was asked about one of our parts outside
                // the catalogue path, which is what a player selecting a tile
                // looks like. `answer=true` means the game accepted it, so a
                // part that then is not equipped was not chosen; `answer=false`
                // means the pick was refused and the reason is upstream of us.
                RC::Output::send<RC::LogLevel::Verbose>(
                    STR("[ZCOM_OPEN_KIT] pick_asked part={} slot={} answer={} asks={} ")
                    STR("character_has_melee_2h={} character_tags={} answer_if_category_scoped={} answer_if_telrea_name_scoped={}\n"),
                    kCatalogue[row].part_id,
                    kCatalogue[row].slot_tag,
                    g_picked_answer[row],
                    g_pick_count[row].load(std::memory_order_relaxed),
                    g_pick_had_category[row] != 0,
                    static_cast<int>(g_pick_tag_count[row]),
                    g_pick_answer_with_category[row] == 2
                        ? STR("not-asked")
                        : (g_pick_answer_with_category[row] == 1 ? STR("true") : STR("false")),
                    g_pick_answer_with_slot[row] == 2
                        ? STR("not-asked")
                        : (g_pick_answer_with_slot[row] == 1 ? STR("true") : STR("false")));
            }
        }

      private:
        HMODULE m_self{};
    };

    HMODULE g_self{};
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = module;
    }
    return TRUE;
}

#define ZCOM_OPEN_KIT_API __declspec(dllexport)

extern "C"
{
    ZCOM_OPEN_KIT_API RC::CppUserModBase* start_mod()
    {
        return new ZCOMOpenKitMod(g_self);
    }

    ZCOM_OPEN_KIT_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
