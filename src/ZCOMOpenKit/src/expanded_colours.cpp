// Expanded colours -- the locked half of the colour palette, offered in the
// pickers the game already draws.
//
// DERIVED WORK. This file is Sternab's, from the MIT-licensed Zero Company
// Expanded Colours (https://github.com/Sternab/ZeroCompanyExpandedColours,
// copyright (c) 2026 Sternab), adapted for Open Kit rather than reimplemented.
// The changes are the namespace, the log prefix, and a lifecycle Open Kit can
// call instead of a mod class of its own. The two colour tables, their picker
// labels, the seam and the mechanism are theirs. See THIRD_PARTY_NOTICES.md.
//
// This is a THIRD seam, and the project had not seen it. Colours are not served
// by UCustomizationStatics::GetCustomizationPartDefinitions at all; they come
// from UCustomizationPartsTagSubsystem::FilterAssetDataByTags, which returns
// FPrimaryAssetIds filtered by tag. That is why a colour part carries no slot
// tag and why Open Kit's slot-keyed catalogue table could never have placed one
// -- an approach this project had written down as the plan an hour earlier.
//
// The hook identifies which picker it is looking at by what the game already
// put in the list: a list already holding CPD_H_Outfit_Color_* is the outfit
// palette, and one holding CPD_Ast_Color_* with the weapon paint slot tag is
// the weapon palette. Nothing else is touched.
//
// Both addresses were re-resolved from this build's own PDB:
//   0x63D1C20  UCustomizationPartsTagSubsystem::FilterAssetDataByTags
//   0x6FB9AE0  UBitReactorCustomizationPartViewModel::InitializeViewModel
//
// Copyright (c) 2026 Sternab; adaptation (c) 2026 Open Kit contributors. MIT.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/FPrimaryAssetId.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/UObject.hpp>
#include <polyhook2/Detour/x64Detour.hpp>

namespace ZCOMOpenKit::ExpandedColours
{
namespace
{
    using RC::Unreal::CastField;
    using RC::Unreal::FName;
    using RC::Unreal::FPrimaryAssetId;
    using RC::Unreal::FPrimaryAssetType;
    using RC::Unreal::FStructProperty;
    using RC::Unreal::FText;
    using RC::Unreal::FTextProperty;
    using RC::Unreal::TArray;
    using RC::Unreal::UObject;

    static_assert(sizeof(FName) == 8, "Expanded Colours requires an eight-byte FName ABI");
    static_assert(sizeof(FPrimaryAssetId) == 16, "Expanded Colours requires a 16-byte FPrimaryAssetId ABI");

    constexpr std::uintptr_t kFilterAssetDataByTagsRva = 0x63D1C20;
    constexpr std::uintptr_t kInitializePartViewModelRva = 0x6FB9AE0;
    constexpr std::uint32_t kExpectedPeTimestamp = 0xE10ABE56;
    constexpr std::uint32_t kExpectedImageSize = 0x0E354000;

    constexpr std::array<std::uint8_t, 16> kFilterAssetDataByTagsBytes{
        0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x56, 0x57,
        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
    };

    constexpr std::array<std::uint8_t, 16> kInitializePartViewModelBytes{
        0x40, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D, 0xAC,
        0x24, 0xA0, 0xFE, 0xFF, 0xFF, 0x48, 0x81, 0xEC,
    };

    constexpr const RC::Unreal::TCHAR* kOutfitColorAssetPrefix = STR("CPD_H_Outfit_Color_");
    constexpr const RC::Unreal::TCHAR* kWeaponColorAssetPrefix = STR("CPD_Ast_Color_");
    constexpr const RC::Unreal::TCHAR* kWeaponPaintSlotTag =
        STR("br.Customization.Slot.Weapon.PaintColor");
    struct ColourSpec
    {
        const RC::Unreal::TCHAR* asset_name;
        const RC::Unreal::TCHAR* picker_label;
    };

    // Upstream's curated forty come first, deliberately: the extra palette is
    // enabled by taking more of this same table, so "curated only" is a prefix
    // rather than a second code path.
    constexpr std::size_t kCuratedOutfitColours = 40;
    constexpr auto kOutfitColours = std::to_array<ColourSpec>({
        {STR("CPD_H_Outfit_Color_Amber_02"), STR("Amber 02")},
        {STR("CPD_H_Outfit_Color_Amber_13"), STR("Amber 13")},
        {STR("CPD_H_Outfit_Color_Amber_16"), STR("Amber 16")},
        {STR("CPD_H_Outfit_Color_Aqua_02"), STR("Aqua 02")},
        {STR("CPD_H_Outfit_Color_Aqua_13"), STR("Aqua 13")},
        {STR("CPD_H_Outfit_Color_Aqua_16"), STR("Aqua 16")},
        {STR("CPD_H_Outfit_Color_Blue_02"), STR("Blue 02")},
        {STR("CPD_H_Outfit_Color_Blue_13"), STR("Blue 13")},
        {STR("CPD_H_Outfit_Color_Blue_16"), STR("Blue 16")},
        {STR("CPD_H_Outfit_Color_Green_02"), STR("Green 02")},
        {STR("CPD_H_Outfit_Color_Green_13"), STR("Green 13")},
        {STR("CPD_H_Outfit_Color_Green_16"), STR("Green 16")},
        {STR("CPD_H_Outfit_Color_Indigo_02"), STR("Indigo 02")},
        {STR("CPD_H_Outfit_Color_Indigo_13"), STR("Indigo 13")},
        {STR("CPD_H_Outfit_Color_Indigo_16"), STR("Indigo 16")},
        {STR("CPD_H_Outfit_Color_Lime_02"), STR("Lime 02")},
        {STR("CPD_H_Outfit_Color_Lime_13"), STR("Lime 13")},
        {STR("CPD_H_Outfit_Color_Lime_16"), STR("Lime 16")},
        {STR("CPD_H_Outfit_Color_Orange_02"), STR("Orange 02")},
        {STR("CPD_H_Outfit_Color_Orange_13"), STR("Orange 13")},
        {STR("CPD_H_Outfit_Color_Orange_16"), STR("Orange 16")},
        {STR("CPD_H_Outfit_Color_Purple_02"), STR("Purple 02")},
        {STR("CPD_H_Outfit_Color_Purple_13"), STR("Purple 13")},
        {STR("CPD_H_Outfit_Color_Purple_16"), STR("Purple 16")},
        {STR("CPD_H_Outfit_Color_Red_02"), STR("Red 02")},
        {STR("CPD_H_Outfit_Color_Red_13"), STR("Red 13")},
        {STR("CPD_H_Outfit_Color_Red_16"), STR("Red 16")},
        {STR("CPD_H_Outfit_Color_Vermilion_02"), STR("Vermilion 02")},
        {STR("CPD_H_Outfit_Color_Vermilion_13"), STR("Vermilion 13")},
        {STR("CPD_H_Outfit_Color_Vermilion_16"), STR("Vermilion 16")},
        {STR("CPD_H_Outfit_Color_Violet_2"), STR("Violet 02")},
        {STR("CPD_H_Outfit_Color_Violet_13"), STR("Violet 13")},
        {STR("CPD_H_Outfit_Color_Violet_16"), STR("Violet 16")},
        {STR("CPD_H_Outfit_Color_Yellow_02"), STR("Yellow 02")},
        {STR("CPD_H_Outfit_Color_Yellow_13"), STR("Yellow 13")},
        {STR("CPD_H_Outfit_Color_Yellow_16"), STR("Yellow 16")},
        {STR("CPD_H_Outfit_Color_CoilBlack"), STR("Oil Black")},
        {STR("CPD_H_Outfit_Color_NeutralGrey_02"), STR("Neutral Grey 02")},
        {STR("CPD_H_Outfit_Color_NeutralGrey_06"), STR("Neutral Grey 06")},
        {STR("CPD_H_Outfit_Color_NeutralGrey_10"), STR("Neutral Grey 10")},
    
    // The rest of the locked palette worth having: every colour at least 0.05 in
    // linear RGB from anything the game already offers, generated by
    // scripts/generate-colour-table.py. Offered only when colours-extra.ini is
    // present, which is why they sit after the curated forty.
#include "colour_table.inc"
    });

    constexpr std::array<ColourSpec, 90> kWeaponColours{{
        {STR("CPD_Ast_Color_100thBattalionBlack"), STR("100th Battalion Black")},
        {STR("CPD_Ast_Color_Amber_01"), STR("Amber 01")},
        {STR("CPD_Ast_Color_Amber_02"), STR("Amber 02")},
        {STR("CPD_Ast_Color_Amber_05"), STR("Amber 05")},
        {STR("CPD_Ast_Color_Amber_07"), STR("Amber 07")},
        {STR("CPD_Ast_Color_Aqua_01"), STR("Aqua 01")},
        {STR("CPD_Ast_Color_Aqua_03"), STR("Aqua 03")},
        {STR("CPD_Ast_Color_Aqua_05"), STR("Aqua 05")},
        {STR("CPD_Ast_Color_Aqua_07"), STR("Aqua 07")},
        {STR("CPD_Ast_Color_Black"), STR("Nutmeg Black")},
        {STR("CPD_Ast_Color_Blue_01"), STR("Blue 01")},
        {STR("CPD_Ast_Color_Blue_03"), STR("Blue 03")},
        {STR("CPD_Ast_Color_Blue_05"), STR("Blue 05")},
        {STR("CPD_Ast_Color_Blue_07"), STR("Blue 07")},
        {STR("CPD_Ast_Color_Brown_01"), STR("Brown 01")},
        {STR("CPD_Ast_Color_Brown_02"), STR("Brown 02")},
        {STR("CPD_Ast_Color_Brown_03"), STR("Brown 03")},
        {STR("CPD_Ast_Color_Brown_04"), STR("Brown 04")},
        {STR("CPD_Ast_Color_Brown_05"), STR("Brown 05")},
        {STR("CPD_Ast_Color_Brown_06"), STR("Brown 06")},
        {STR("CPD_Ast_Color_Brown_07"), STR("Brown 07")},
        {STR("CPD_Ast_Color_Brown_08"), STR("Brown 08")},
        {STR("CPD_Ast_Color_Brown_09"), STR("Brown 09")},
        {STR("CPD_Ast_Color_Brown_10"), STR("Brown 10")},
        {STR("CPD_Ast_Color_Clone_White1"), STR("Clone White")},
        {STR("CPD_Ast_Color_CoilBlack"), STR("Oil Black")},
        {STR("CPD_Ast_Color_CoolGrey_03"), STR("Cool Grey 03")},
        {STR("CPD_Ast_Color_CoolGrey_04"), STR("Cool Grey 04")},
        {STR("CPD_Ast_Color_CoolGrey_05"), STR("Cool Grey 05")},
        {STR("CPD_Ast_Color_CoolGrey_07"), STR("Cool Grey 07")},
        {STR("CPD_Ast_Color_CoolGrey_08"), STR("Cool Grey 08")},
        {STR("CPD_Ast_Color_CoolGrey_11"), STR("Cool Grey 11")},
        {STR("CPD_Ast_Color_Green_01"), STR("Green 01")},
        {STR("CPD_Ast_Color_Green_03"), STR("Green 03")},
        {STR("CPD_Ast_Color_Green_05"), STR("Green 05")},
        {STR("CPD_Ast_Color_Green_07"), STR("Green 07")},
        {STR("CPD_Ast_Color_Indigo_01"), STR("Indigo 01")},
        {STR("CPD_Ast_Color_Indigo_02"), STR("Indigo 02")},
        {STR("CPD_Ast_Color_Indigo_03"), STR("Indigo 03")},
        {STR("CPD_Ast_Color_Indigo_04"), STR("Indigo 04")},
        {STR("CPD_Ast_Color_Indigo_05"), STR("Indigo 05")},
        {STR("CPD_Ast_Color_Indigo_06"), STR("Indigo 06")},
        {STR("CPD_Ast_Color_Indigo_07"), STR("Indigo 07")},
        {STR("CPD_Ast_Color_Indigo_08"), STR("Indigo 08")},
        {STR("CPD_Ast_Color_Indigo_09"), STR("Indigo 09")},
        {STR("CPD_Ast_Color_Indigo_10"), STR("Indigo 10")},
        {STR("CPD_Ast_Color_Lime_01"), STR("Lime 01")},
        {STR("CPD_Ast_Color_Lime_02"), STR("Lime 02")},
        {STR("CPD_Ast_Color_Lime_05"), STR("Lime 05")},
        {STR("CPD_Ast_Color_Lime_07"), STR("Lime 07")},
        {STR("CPD_Ast_Color_Orange_01"), STR("Orange 01")},
        {STR("CPD_Ast_Color_Orange_02"), STR("Orange 02")},
        {STR("CPD_Ast_Color_Orange_05"), STR("Orange 05")},
        {STR("CPD_Ast_Color_Orange_07"), STR("Orange 07")},
        {STR("CPD_Ast_Color_Pink_01"), STR("Pink 01")},
        {STR("CPD_Ast_Color_Pink_03"), STR("Pink 03")},
        {STR("CPD_Ast_Color_Pink_05"), STR("Pink 05")},
        {STR("CPD_Ast_Color_Pink_07"), STR("Pink 07")},
        {STR("CPD_Ast_Color_Purple_01"), STR("Purple 01")},
        {STR("CPD_Ast_Color_Purple_03"), STR("Purple 03")},
        {STR("CPD_Ast_Color_Purple_05"), STR("Purple 05")},
        {STR("CPD_Ast_Color_Purple_07"), STR("Purple 07")},
        {STR("CPD_Ast_Color_Red_01"), STR("Red 01")},
        {STR("CPD_Ast_Color_Red_02"), STR("Red 02")},
        {STR("CPD_Ast_Color_Red_05"), STR("Red 05")},
        {STR("CPD_Ast_Color_Red_07"), STR("Red 07")},
        {STR("CPD_Ast_Color_Vermilion_01"), STR("Vermilion 01")},
        {STR("CPD_Ast_Color_Vermilion_02"), STR("Vermilion 02")},
        {STR("CPD_Ast_Color_Vermilion_05"), STR("Vermilion 05")},
        {STR("CPD_Ast_Color_Vermilion_07"), STR("Vermilion 07")},
        {STR("CPD_Ast_Color_Violet_01"), STR("Violet 01")},
        {STR("CPD_Ast_Color_Violet_03"), STR("Violet 03")},
        {STR("CPD_Ast_Color_Violet_05"), STR("Violet 05")},
        {STR("CPD_Ast_Color_Violet_07"), STR("Violet 07")},
        {STR("CPD_Ast_Color_WarmGrey_01"), STR("Warm Grey 01")},
        {STR("CPD_Ast_Color_WarmGrey_02"), STR("Warm Grey 02")},
        {STR("CPD_Ast_Color_WarmGrey_03"), STR("Warm Grey 03")},
        {STR("CPD_Ast_Color_WarmGrey_04"), STR("Warm Grey 04")},
        {STR("CPD_Ast_Color_WarmGrey_05"), STR("Warm Grey 05")},
        {STR("CPD_Ast_Color_WarmGrey_06"), STR("Warm Grey 06")},
        {STR("CPD_Ast_Color_WarmGrey_07"), STR("Warm Grey 07")},
        {STR("CPD_Ast_Color_WarmGrey_08"), STR("Warm Grey 08")},
        {STR("CPD_Ast_Color_WarmGrey_09"), STR("Warm Grey 09")},
        {STR("CPD_Ast_Color_WarmGrey_10"), STR("Warm Grey 10")},
        {STR("CPD_Ast_Color_WarmGrey_11"), STR("Warm Grey 11")},
        {STR("CPD_Ast_Color_WarmGrey_12"), STR("Warm Grey 12")},
        {STR("CPD_Ast_Color_Yellow_01"), STR("Yellow 01")},
        {STR("CPD_Ast_Color_Yellow_02"), STR("Yellow 02")},
        {STR("CPD_Ast_Color_Yellow_05"), STR("Yellow 05")},
        {STR("CPD_Ast_Color_Yellow_07"), STR("Yellow 07")},
    }};

    struct GameplayTag
    {
        FName tag_name{};
    };

    struct GameplayTagContainer
    {
        TArray<GameplayTag> gameplay_tags{};
        TArray<GameplayTag> parent_tags{};
    };

    struct RuntimeIdentity
    {
        std::uintptr_t base{};
        std::uintptr_t image_size{};
    };

    using FilterAssetDataByTagsFunction = void(__fastcall*)(
        void*, const GameplayTagContainer*, TArray<FPrimaryAssetId>*);
    using InitializePartViewModelFunction = void(__fastcall*)(UObject*);

    RuntimeIdentity g_runtime{};
    std::array<FPrimaryAssetId, kOutfitColours.size()> g_outfit_colour_ids{};
    // How much of kOutfitColours is live: the curated forty, or all of it.
    std::size_t g_outfit_colour_count{kCuratedOutfitColours};
    std::array<FPrimaryAssetId, kWeaponColours.size()> g_weapon_colour_ids{};
    FName g_weapon_paint_slot_tag{};
    std::uint64_t g_filter_trampoline{};
    std::uint64_t g_view_model_trampoline{};
    std::unique_ptr<PLH::x64Detour> g_filter_hook{};
    std::unique_ptr<PLH::x64Detour> g_view_model_hook{};
    std::atomic<bool> g_seen_outfit_catalogue{};
    std::atomic<bool> g_seen_weapon_catalogue{};
    std::atomic<std::uint32_t> g_pending_outfit_additions{};
    std::atomic<std::uint32_t> g_pending_weapon_additions{};
    std::atomic<std::uint32_t> g_pending_outfit_labels{};
    std::atomic<std::uint32_t> g_pending_weapon_labels{};

    // Armory tiles that cannot be told apart on screen, relabelled here because
    // this is where a picker entry's label is decided -- the same seam and the
    // same mechanism the colours use to separate Amber_02 from Amber_13.
    //
    // Two of them share a DisplayName with a stock part and two have no
    // DisplayName at all, and both facts are the game's own; the Armory
    // generator reports each as a caveat on its row. A player being asked to
    // pick "the DC-17 with no icon" is not a workable instruction, and a test
    // that cannot tell which tile was clicked cannot answer what happened.
    //
    // These are UI-only. Nothing here touches the part, the character or the
    // save -- it writes the view model's DisplayName for one tile, exactly as
    // upstream's colour labels do.
    struct ArmouryLabel
    {
        const RC::Unreal::TCHAR* asset_name;
        const RC::Unreal::TCHAR* picker_label;
    };

    constexpr std::array<ArmouryLabel, 7> kArmouryLabels{{
        // Both read "DC-17" in the list; only one of them is the dual-wield kit.
        // Confirmed in game with this one chosen: the operator carries two
        // pistols and DualFire fires both.
        {STR("CPD_GK_Pistol_DC-17_Rex"), STR("DC-17 (Rex)")},
        // The three nameless melee kits, which are the game's own parts and not
        // this module's rows. They became visible only once the Armory's
        // lightsaber lock was lifted, and the MODEL list then drew three blank
        // tiles: no melee gear kit in the game carries a DisplayName, because
        // until now no player could open that screen. Named from the weapon each
        // one actually equips, followed through the gear kit to the mesh:
        //
        //   CoilStriker           -> GK_MeleeWeap_Enemy_Coil_Striker    -> SK_CoilStrikerWeapon
        //   Enemy_Coil_Scourge    -> GK_MeleeWeap_Scourger
        //   Enemy_Generic_Striker -> GK_MeleeWeap_Enemy_Slugrat_Striker -> SK_SlugratHook
        {STR("CPD_GK_MeleeWeapon_CoilStriker"), STR("Coil Striker")},
        {STR("CPD_GK_MeleeWeapon_Enemy_Coil_Scourge"), STR("Scourger")},
        {STR("CPD_GK_MeleeWeapon_Enemy_Generic_Striker"), STR("Slugrat Hook")},
        // Both read "Blaster Pistol" in the weapon class list.
        {STR("CPD_WeaponSpec_Blaster_Pistol_Rex"), STR("Blaster Pistol (Rex)")},
        // Nameless, as every lightsaber gear kit in the game is.
        {STR("CPD_GK_Lightsaber_Enemy_Imperial_Trilla"), STR("Inquisitor Lightsaber")},
        {STR("CPD_GK_Lightsaber_TelRea"), STR("Padawan Lightsaber")},
    }};

    std::array<FPrimaryAssetId, kArmouryLabels.size()> g_armoury_label_ids{};
    std::atomic<std::uint32_t> g_pending_armoury_labels{};
    bool g_hook_active{};

    auto bytes_match(std::uintptr_t base,
                 std::uintptr_t image_size,
                 std::uintptr_t rva,
                 const std::array<std::uint8_t, 16>& expected) -> bool
    {
        return rva + expected.size() <= image_size &&
           std::memcmp(reinterpret_cast<const void*>(base + rva), expected.data(), expected.size()) == 0;
    }

    auto validate_runtime(RuntimeIdentity& identity, const char*& reason) -> bool
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

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        base + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->FileHeader.TimeDateStamp != kExpectedPeTimestamp ||
        nt->OptionalHeader.SizeOfImage != kExpectedImageSize)
        {
        reason = "retail-pe-identity-mismatch";
        return false;
        }

        const auto image_size = static_cast<std::uintptr_t>(nt->OptionalHeader.SizeOfImage);
        if (!bytes_match(base, image_size, kFilterAssetDataByTagsRva, kFilterAssetDataByTagsBytes))
        {
        reason = "filter-target-byte-mismatch";
        return false;
        }
        if (!bytes_match(base, image_size, kInitializePartViewModelRva, kInitializePartViewModelBytes))
        {
        reason = "part-view-model-target-byte-mismatch";
        return false;
        }

        identity = {base, image_size};
        return true;
    }

    auto ids_equal(const FPrimaryAssetId& left, const FPrimaryAssetId& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FPrimaryAssetId)) == 0;
    }

    auto array_contains(const TArray<FPrimaryAssetId>& ids, const FPrimaryAssetId& target) -> bool
    {
        for (const auto& id : ids)
        {
        if (ids_equal(id, target))
        {
            return true;
        }
        }
        return false;
    }

    auto names_equal(const FName& left, const FName& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FName)) == 0;
    }

    auto asset_name_has_prefix(const FPrimaryAssetId& id,
                           const RC::Unreal::TCHAR* prefix) -> bool
    {
        const auto name = id.PrimaryAssetName.ToString();
        const auto prefix_length = std::char_traits<RC::Unreal::TCHAR>::length(prefix);
        return name.size() >= prefix_length &&
           std::char_traits<RC::Unreal::TCHAR>::compare(
               name.data(), prefix, prefix_length) == 0;
    }

    auto catalogue_has_prefix(const TArray<FPrimaryAssetId>& ids,
                          const RC::Unreal::TCHAR* prefix) -> bool
    {
        for (const auto& id : ids)
        {
        if (asset_name_has_prefix(id, prefix))
        {
            return true;
        }
        }
        return false;
    }

    auto tag_array_contains(const TArray<GameplayTag>& tags, const FName& target) -> bool
    {
        for (const auto& tag : tags)
        {
        if (names_equal(tag.tag_name, target))
        {
            return true;
        }
        }
        return false;
    }

    auto tag_container_contains(const GameplayTagContainer* tags, const FName& target) -> bool
    {
        return tags != nullptr &&
           (tag_array_contains(tags->gameplay_tags, target) ||
            tag_array_contains(tags->parent_tags, target));
    }

    auto append_missing(TArray<FPrimaryAssetId>& output,
                        const FPrimaryAssetId* additions,
                        std::size_t count) -> std::size_t
    {
        std::size_t appended{};
        for (std::size_t index = 0; index < count; ++index)
        {
            if (!array_contains(output, additions[index]))
            {
                output.Add(additions[index]);
                ++appended;
            }
        }
        return appended;
    }

    auto record_catalogue_once(std::atomic<bool>& seen,
                           std::atomic<std::uint32_t>& pending,
                           std::size_t appended) -> void
    {
        if (appended > 0 && !seen.exchange(true, std::memory_order_relaxed))
        {
        pending.store(static_cast<std::uint32_t>(appended), std::memory_order_release);
        }
    }

    struct ColourMatch
    {
        const ColourSpec* spec{};
        bool weapon{};
    };

    auto find_colour(const FPrimaryAssetId& id) -> ColourMatch
    {
        for (std::size_t index = 0; index < g_outfit_colour_count; ++index)
        {
        if (ids_equal(id, g_outfit_colour_ids[index]))
        {
            return {&kOutfitColours[index], false};
        }
        }

        for (std::size_t index = 0; index < g_weapon_colour_ids.size(); ++index)
        {
        if (ids_equal(id, g_weapon_colour_ids[index]))
        {
            return {&kWeaponColours[index], true};
        }
        }
        return {};
    }

    auto set_picker_label(UObject* view_model, const RC::Unreal::TCHAR* text) -> bool
    {
        if (view_model == nullptr)
        {
        return false;
        }

        auto* property = CastField<FTextProperty>(
        view_model->GetPropertyByNameInChain(STR("DisplayName")));
        auto* destination = property == nullptr
                            ? nullptr
                            : property->ContainerPtrToValuePtr<void>(view_model);
        if (destination == nullptr)
        {
        return false;
        }

        const FText label(text);
        property->CopyCompleteValue(destination, &label);
        return true;
    }

    auto hook_filter_asset_data_by_tags(void* subsystem,
                                    const GameplayTagContainer* owned_tags,
                                    TArray<FPrimaryAssetId>* output) -> void
    {
        const auto original = reinterpret_cast<FilterAssetDataByTagsFunction>(g_filter_trampoline);
        if (original == nullptr)
        {
        return;
        }

        original(subsystem, owned_tags, output);
        if (output == nullptr)
        {
        return;
        }

        if (catalogue_has_prefix(*output, kOutfitColorAssetPrefix))
        {
        const auto appended =
                append_missing(*output, g_outfit_colour_ids.data(), g_outfit_colour_count);
        record_catalogue_once(
            g_seen_outfit_catalogue, g_pending_outfit_additions, appended);
        }
        else if (catalogue_has_prefix(*output, kWeaponColorAssetPrefix) &&
             tag_container_contains(owned_tags, g_weapon_paint_slot_tag))
        {
        const auto appended = append_missing(*output, g_weapon_colour_ids.data(),
                                                g_weapon_colour_ids.size());
        record_catalogue_once(
            g_seen_weapon_catalogue, g_pending_weapon_additions, appended);
        }
    }

    auto hook_initialize_part_view_model(UObject* view_model) -> void
    {
        const auto original = reinterpret_cast<InitializePartViewModelFunction>(g_view_model_trampoline);
        if (original == nullptr)
        {
        return;
        }

        original(view_model);
        if (view_model == nullptr)
        {
        return;
        }

        auto* asset_id_property = CastField<FStructProperty>(
        view_model->GetPropertyByNameInChain(STR("AssetId")));
        if (asset_id_property == nullptr ||
        asset_id_property->GetElementSize() != sizeof(FPrimaryAssetId))
        {
        return;
        }

        const auto* asset_id = asset_id_property->ContainerPtrToValuePtr<FPrimaryAssetId>(view_model);
        if (asset_id == nullptr)
        {
        return;
        }

        const auto match = find_colour(*asset_id);
        if (match.spec != nullptr && set_picker_label(view_model, match.spec->picker_label))
        {
        auto& pending = match.weapon ? g_pending_weapon_labels : g_pending_outfit_labels;
        pending.fetch_add(1, std::memory_order_release);
        return;
        }

        for (std::size_t index = 0; index < kArmouryLabels.size(); ++index)
        {
        if (!ids_equal(*asset_id, g_armoury_label_ids[index]))
        {
            continue;
        }
        if (set_picker_label(view_model, kArmouryLabels[index].picker_label))
        {
            g_pending_armoury_labels.fetch_add(1, std::memory_order_release);
        }
        return;
        }
    }
} // anonymous namespace

// Open Kit drives these; upstream ran them from its own CppUserModBase.
auto initialize(bool include_extra_colours) -> bool
{

        const char* reason = "ok";
        if (!validate_runtime(g_runtime, reason))
        {
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZCOM_OPEN_KIT] colours_declined reason={}\n"), RC::ensure_str(reason));
            return false;
        }

        // How much of kOutfitColours is live. Every id is registered either way --
        // registering one costs an FName and nothing else -- but only the first
        // g_outfit_colour_count of them are ever offered or labelled.
        g_outfit_colour_count =
            include_extra_colours ? kOutfitColours.size() : kCuratedOutfitColours;

        const FName asset_type_name(STR("CustomizationPartDefinition"), RC::Unreal::FNAME_Add);
        for (std::size_t index = 0; index < kOutfitColours.size(); ++index)
        {
            g_outfit_colour_ids[index] = FPrimaryAssetId{
                FPrimaryAssetType{asset_type_name},
                FName(kOutfitColours[index].asset_name, RC::Unreal::FNAME_Add),
            };
        }
        for (std::size_t index = 0; index < kWeaponColours.size(); ++index)
        {
            g_weapon_colour_ids[index] = FPrimaryAssetId{
                FPrimaryAssetType{asset_type_name},
                FName(kWeaponColours[index].asset_name, RC::Unreal::FNAME_Add),
            };
        }
        g_weapon_paint_slot_tag = FName(kWeaponPaintSlotTag, RC::Unreal::FNAME_Add);

        for (std::size_t index = 0; index < kArmouryLabels.size(); ++index)
        {
            g_armoury_label_ids[index] = FPrimaryAssetId{
                FPrimaryAssetType{asset_type_name},
                FName(kArmouryLabels[index].asset_name, RC::Unreal::FNAME_Add),
            };
        }

        g_filter_hook = std::make_unique<PLH::x64Detour>(
            g_runtime.base + kFilterAssetDataByTagsRva,
            reinterpret_cast<std::uint64_t>(&hook_filter_asset_data_by_tags),
            &g_filter_trampoline);
        if (!g_filter_hook->hook() || g_filter_trampoline == 0)
        {
            g_filter_hook.reset();
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZCOM_OPEN_KIT] colours_declined reason=filter-hook-install-failed\n"));
            return false;
        }

        g_view_model_hook = std::make_unique<PLH::x64Detour>(
            g_runtime.base + kInitializePartViewModelRva,
            reinterpret_cast<std::uint64_t>(&hook_initialize_part_view_model),
            &g_view_model_trampoline);
        if (!g_view_model_hook->hook() || g_view_model_trampoline == 0)
        {
            g_view_model_hook.reset();
            g_filter_hook->unHook();
            g_filter_hook.reset();
            g_filter_trampoline = 0;
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZCOM_OPEN_KIT] colours_declined reason=picker-label-hook-install-failed\n"));
            return false;
        }

        g_hook_active = true;
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[ZCOM_OPEN_KIT] colours_READY seam=FilterAssetDataByTags build=24874058 outfit_palette_size={} weapon_palette_size=90 picker_labels=distinct-asset-variant-names ui_only_label_override=true outfit_guard=existing-Outfit-Color-primary-asset weapon_guard=Ast-Color-plus-Weapon-PaintColor-tag material_overrides=false test_colours=false character_specific_colours=false\n"),
        g_outfit_colour_count);
    return true;
}

auto update() -> void
{

        const auto outfit_additions =
            g_pending_outfit_additions.exchange(0, std::memory_order_acquire);
        if (outfit_additions != 0)
        {
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT] catalogue_extended family=outfit additions={} material_override=false\n"),
                outfit_additions);
        }

        const auto weapon_additions =
            g_pending_weapon_additions.exchange(0, std::memory_order_acquire);
        if (weapon_additions != 0)
        {
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT] catalogue_extended family=weapon-paint additions={} material_override=false\n"),
                weapon_additions);
        }

        const auto outfit_labels =
            g_pending_outfit_labels.exchange(0, std::memory_order_acquire);
        if (outfit_labels != 0)
        {
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT] picker_labels_applied family=outfit count={} ui_only=true\n"),
                outfit_labels);
        }

        const auto armoury_labels =
            g_pending_armoury_labels.exchange(0, std::memory_order_acquire);
        if (armoury_labels != 0)
        {
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT] picker_labels_applied family=armoury count={} ui_only=true\n"),
                armoury_labels);
        }

        const auto weapon_labels =
            g_pending_weapon_labels.exchange(0, std::memory_order_acquire);
        if (weapon_labels != 0)
        {
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT] picker_labels_applied family=weapon-paint count={} ui_only=true\n"),
                weapon_labels);
        }
}

auto shutdown() -> void
{
    if (g_view_model_hook)
    {
        g_view_model_hook->unHook();
    }
    if (g_filter_hook)
    {
        g_filter_hook->unHook();
    }
    g_view_model_hook.reset();
    g_filter_hook.reset();
    g_view_model_trampoline = 0;
    g_filter_trampoline = 0;
    g_hook_active = false;
}

} // namespace ZCOMOpenKit::ExpandedColours
