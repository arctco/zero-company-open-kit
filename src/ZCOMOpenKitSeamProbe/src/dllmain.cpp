// Open Kit seam probe -- observation only. Installs no behaviour and changes
// nothing the game does; every hook calls the original and reports what it saw.
//
// It exists to answer three questions that reasoning could not settle:
//
//   G1  Which of Zero Company's four customization enumerators produces the
//       specialization cards? Appending to the wrong one is a silent no-op, and
//       that failure has already cost this project three shipped containers.
//   G2  What tag container does that caller pass? A scoped requirement answer
//       has to know which capability tags the character is missing.
//   G3  Are the hero specialization parts enumerated and then refused, or never
//       enumerated at all? The two need opposite fixes.
//
// Structure and safety practice follow Sternab's MIT-licensed Zero Company
// Mandalorian Wardrobe (https://github.com/Sternab/ZeroCompanyMandoWardrobe,
// copyright (c) 2026 Sternab): refuse an unverified build, hook with PolyHook,
// and never log from inside a hook -- record a bit and emit it from on_update.
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
#include <memory>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/FPrimaryAssetId.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <polyhook2/Detour/x64Detour.hpp>

namespace
{
    using RC::Unreal::FName;
    using RC::Unreal::FPrimaryAssetId;
    using RC::Unreal::FPrimaryAssetType;
    using RC::Unreal::TArray;
    using RC::Unreal::UObject;

    static_assert(sizeof(FName) == 8, "requires an eight-byte FName ABI");
    static_assert(sizeof(FPrimaryAssetId) == 16, "requires a 16-byte FPrimaryAssetId ABI");

    // Steam build 24874058. Every address below was resolved from the PDB the
    // game installs beside its executable; see docs/SEAM-MAP.md.
    constexpr std::uint32_t kExpectedPeTimestamp = 0xE10ABE56;
    constexpr std::uint32_t kExpectedImageSize = 0x0E354000;

    enum Seam : std::size_t
    {
        kSeamPartDefinitions = 0,
        kSeamPartIds,
        kSeamAllPartIdsMatchingTags,
        kSeamPresetsForCharacter,
        kSeamRequirements,
        kSeamCount,
    };

    struct SeamSpec
    {
        const RC::Unreal::TCHAR* name;
        std::uintptr_t rva;
        std::array<std::uint8_t, 16> prologue;
    };

    constexpr std::array<SeamSpec, kSeamCount> kSeams{{
        {STR("GetCustomizationPartDefinitions"),
         0x63C54D0,
         {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48}},
        {STR("GetCustomizationPartIds"),
         0x63C50C0,
         {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48}},
        {STR("GetAllCustomizationPartIdsMatchingTags"),
         0x63C51B0,
         {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x48, 0x08, 0x55, 0x48, 0x8D, 0x68, 0xA1, 0x48, 0x81, 0xEC, 0xE0}},
        {STR("GetCustomizationPresetsForCharacter"),
         0x63C7F90,
         {0x48, 0x85, 0xC9, 0x0F, 0x84, 0x53, 0x02, 0x00, 0x00, 0x55, 0x53, 0x57, 0x48, 0x8B, 0xEC, 0x48}},
        {STR("DoesPartIdMeetRequirements"),
         0x63C6400,
         {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18, 0x4C}},
    }};

    // Not hooked, called: turns a part id into its definition so the definition
    // pointers an enumerator returns can be identified.
    constexpr std::uintptr_t kGetPartDefinitionFromPartIdRva = 0x63C5660;
    constexpr std::array<std::uint8_t, 16> kGetPartDefinitionFromPartIdBytes{
        0x4C, 0x8B, 0xDC, 0x53, 0x55, 0x48, 0x81, 0xEC,
        0xB8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x01, 0x33,
    };

    // The parts to watch for. Soldier and the Man001A torso are controls: both
    // are offered by the stock game, so a seam that yields them is the seam that
    // feeds that picker. The rest are the six hero kit parts Open Kit is after.
    struct WatchedPart
    {
        const RC::Unreal::TCHAR* asset_name;
        bool is_control;
    };

    constexpr std::array<WatchedPart, 8> kWatchedParts{{
        {STR("CPD_TacticalSpec_Soldier"), true},
        {STR("CPD_H_Outfit_Man001A_TORS"), true},
        {STR("CPD_TacticalSpec_Padawan"), false},
        {STR("CPD_TacticalSpec_PadawanExtended"), false},
        {STR("CPD_TacticalSpec_Warrior"), false},
        {STR("CPD_TalentSpec_TheLostPadawan"), false},
        {STR("CPD_TalentSpec_TheMandalorian"), false},
        {STR("CPD_WeaponSpec_Melee_2H_TelRea"), false},
    }};

    static_assert(kWatchedParts.size() <= 32, "watch masks are 32 bits wide");

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

    using EnumerateDefinitionsFn = void(__fastcall*)(const GameplayTagContainer*, TArray<const UObject*>*);
    using EnumerateIdsFn = void(__fastcall*)(const GameplayTagContainer*, TArray<FPrimaryAssetId>*);
    using PresetsForCharacterFn = void(__fastcall*)(const UObject*, TArray<const UObject*>*);
    using DoesPartIdMeetRequirementsFn = bool(__fastcall*)(const FPrimaryAssetId*, const GameplayTagContainer*);
    using GetPartDefinitionFromPartIdFn = const UObject*(__fastcall*)(const FPrimaryAssetId*);

    // A tag container copied out of a hook and stringified later, on the game
    // thread, where calling FName::ToString is safe.
    constexpr std::size_t kMaxCapturedTags = 96;
    constexpr std::size_t kMaxCapturedSlots = 12;

    // The first tag in the container is the slot being asked about; the rest is
    // the character's own tag set. Capturing per slot rather than per seam is
    // what turns this from one sample into the mapping the module needs.
    struct SlotCapture
    {
        std::atomic<bool> claimed{false};
        std::atomic<bool> ready{false};
        std::atomic<bool> reported{false};
        FName slot{};
        std::uint32_t owned_count{};
        std::uint32_t parent_count{};
        std::uint32_t stored{};
        std::uint64_t results{};
        std::uint32_t watched_mask{};
        std::array<FName, kMaxCapturedTags> tags{};
    };

    struct SeamState
    {
        std::atomic<std::uint64_t> calls{0};
        std::atomic<std::uint32_t> seen_mask{0};      // watched parts found in the output
        std::atomic<std::uint32_t> allowed_mask{0};   // requirements gate said yes
        std::atomic<std::uint64_t> output_max{0};     // largest result seen
        std::atomic<std::uint32_t> slots_used{0};
        std::array<SlotCapture, kMaxCapturedSlots> slots{};
    };

    std::uintptr_t g_base{};
    std::uintptr_t g_image_size{};
    std::array<FPrimaryAssetId, kWatchedParts.size()> g_watched_ids{};
    std::array<std::atomic<const UObject*>, kWatchedParts.size()> g_watched_definitions{};
    std::array<SeamState, kSeamCount> g_seams{};
    std::array<std::uint64_t, kSeamCount> g_trampolines{};
    std::array<std::unique_ptr<PLH::x64Detour>, kSeamCount> g_hooks{};

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

        for (const auto& seam : kSeams)
        {
            if (!bytes_match(seam.rva, seam.prologue))
            {
                reason = "pdb-target-byte-mismatch";
                return false;
            }
        }
        if (!bytes_match(kGetPartDefinitionFromPartIdRva, kGetPartDefinitionFromPartIdBytes))
        {
            reason = "pdb-target-byte-mismatch";
            return false;
        }
        return true;
    }

    auto ids_equal(const FPrimaryAssetId& left, const FPrimaryAssetId& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FPrimaryAssetId)) == 0;
    }

    auto watched_index_for_id(const FPrimaryAssetId& id) -> std::size_t
    {
        for (std::size_t index = 0; index < g_watched_ids.size(); ++index)
        {
            if (ids_equal(id, g_watched_ids[index]))
            {
                return index;
            }
        }
        return g_watched_ids.size();
    }

    std::atomic<std::uint32_t> g_resolved_mask{0};
    std::atomic<std::uint32_t> g_resolve_attempted_mask{0};

    // GetCustomizationPartDefinitionFromPartId reaches UAssetManager::Get(),
    // which faults if the asset manager is not up yet. A diagnostic must not be
    // able to take the game down, so the call is guarded. No C++ object with a
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

    // Resolved lazily and cached. Only ever called from inside a hook, where the
    // game is demonstrably already inside its own customization system and the
    // asset manager is therefore alive. Calling it from on_update instead is
    // what crashed 0.2.0: an access violation in UAssetManager::Get() about a
    // second after startup, long before the manager exists.
    auto definition_for(std::size_t index) -> const UObject*
    {
        if (index >= g_watched_definitions.size())
        {
            return nullptr;
        }
        if (const auto* cached = g_watched_definitions[index].load(std::memory_order_acquire); cached != nullptr)
        {
            return cached;
        }

        const std::uint32_t bit = std::uint32_t{1} << static_cast<std::uint32_t>(index);
        g_resolve_attempted_mask.fetch_or(bit, std::memory_order_relaxed);

        const UObject* definition = guarded_get_part_definition(&g_watched_ids[index]);
        if (definition == nullptr || !UObject::IsReal(const_cast<UObject*>(definition)))
        {
            // Not an error: the asset may simply not be loaded yet, and the next
            // call through this seam will try again.
            return nullptr;
        }

        g_watched_definitions[index].store(definition, std::memory_order_release);
        g_resolved_mask.fetch_or(bit, std::memory_order_relaxed);
        return definition;
    }

    auto names_equal(const FName& left, const FName& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FName)) == 0;
    }

    auto capture_slot(SeamState& state,
                      const GameplayTagContainer* tags,
                      std::uint32_t watched_mask,
                      std::uint64_t output_size) -> void
    {
        if (tags == nullptr || tags->gameplay_tags.Num() <= 0)
        {
            return;
        }
        const FName slot = tags->gameplay_tags.GetData()[0].tag_name;

        // Already recorded this slot? Only fold in what is new.
        const auto used = state.slots_used.load(std::memory_order_acquire);
        for (std::uint32_t index = 0; index < used && index < kMaxCapturedSlots; ++index)
        {
            auto& existing = state.slots[index];
            if (!existing.ready.load(std::memory_order_acquire) || !names_equal(existing.slot, slot))
            {
                continue;
            }
            existing.watched_mask |= watched_mask;
            if (output_size > existing.results)
            {
                existing.results = output_size;
            }
            return;
        }

        const auto index = state.slots_used.fetch_add(1, std::memory_order_acq_rel);
        if (index >= kMaxCapturedSlots)
        {
            state.slots_used.store(kMaxCapturedSlots, std::memory_order_release);
            return;
        }

        auto& capture = state.slots[index];
        if (capture.claimed.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }
        capture.slot = slot;
        capture.results = output_size;
        capture.watched_mask = watched_mask;
        capture.owned_count = static_cast<std::uint32_t>(tags->gameplay_tags.Num());
        capture.parent_count = static_cast<std::uint32_t>(tags->parent_tags.Num());
        std::uint32_t stored = 0;
        for (const auto& tag : tags->gameplay_tags)
        {
            if (stored >= kMaxCapturedTags)
            {
                break;
            }
            capture.tags[stored++] = tag.tag_name;
        }
        capture.stored = stored;
        capture.ready.store(true, std::memory_order_release);
    }

    auto note_call(Seam seam, const GameplayTagContainer* tags, std::uint32_t watched_mask, std::uint64_t output_size)
        -> void
    {
        auto& state = g_seams[seam];
        state.calls.fetch_add(1, std::memory_order_relaxed);

        for (auto previous = state.output_max.load(std::memory_order_relaxed);
             output_size > previous &&
             !state.output_max.compare_exchange_weak(previous, output_size, std::memory_order_relaxed);)
        {
        }

        if (watched_mask != 0)
        {
            state.seen_mask.fetch_or(watched_mask, std::memory_order_relaxed);
        }
        capture_slot(state, tags, watched_mask, output_size);
    }

    auto scan_definitions(const TArray<const UObject*>* output) -> std::uint32_t
    {
        if (output == nullptr)
        {
            return 0;
        }
        std::uint32_t mask = 0;
        for (std::size_t index = 0; index < kWatchedParts.size(); ++index)
        {
            const UObject* wanted = definition_for(index);
            if (wanted == nullptr)
            {
                continue;
            }
            for (const auto* entry : *output)
            {
                if (entry == wanted)
                {
                    mask |= std::uint32_t{1} << static_cast<std::uint32_t>(index);
                    break;
                }
            }
        }
        return mask;
    }

    auto scan_ids(const TArray<FPrimaryAssetId>* output) -> std::uint32_t
    {
        if (output == nullptr)
        {
            return 0;
        }
        std::uint32_t mask = 0;
        for (const auto& id : *output)
        {
            const std::size_t index = watched_index_for_id(id);
            if (index < kWatchedParts.size())
            {
                mask |= std::uint32_t{1} << static_cast<std::uint32_t>(index);
            }
        }
        return mask;
    }

    auto hook_part_definitions(const GameplayTagContainer* owned_tags, TArray<const UObject*>* output) -> void
    {
        const auto original = reinterpret_cast<EnumerateDefinitionsFn>(g_trampolines[kSeamPartDefinitions]);
        if (original == nullptr)
        {
            return;
        }
        original(owned_tags, output);
        note_call(kSeamPartDefinitions,
                  owned_tags,
                  scan_definitions(output),
                  output != nullptr ? static_cast<std::uint64_t>(output->Num()) : 0);
    }

    auto hook_part_ids(const GameplayTagContainer* owned_tags, TArray<FPrimaryAssetId>* output) -> void
    {
        const auto original = reinterpret_cast<EnumerateIdsFn>(g_trampolines[kSeamPartIds]);
        if (original == nullptr)
        {
            return;
        }
        original(owned_tags, output);
        note_call(kSeamPartIds,
                  owned_tags,
                  scan_ids(output),
                  output != nullptr ? static_cast<std::uint64_t>(output->Num()) : 0);
    }

    auto hook_all_part_ids_matching_tags(const GameplayTagContainer* owned_tags, TArray<FPrimaryAssetId>* output) -> void
    {
        const auto original = reinterpret_cast<EnumerateIdsFn>(g_trampolines[kSeamAllPartIdsMatchingTags]);
        if (original == nullptr)
        {
            return;
        }
        original(owned_tags, output);
        note_call(kSeamAllPartIdsMatchingTags,
                  owned_tags,
                  scan_ids(output),
                  output != nullptr ? static_cast<std::uint64_t>(output->Num()) : 0);
    }

    auto hook_presets_for_character(const UObject* character, TArray<const UObject*>* output) -> void
    {
        const auto original = reinterpret_cast<PresetsForCharacterFn>(g_trampolines[kSeamPresetsForCharacter]);
        if (original == nullptr)
        {
            return;
        }
        original(character, output);
        // This overload takes an actor, not a tag container, so there is nothing
        // to capture -- only whether it is on the path at all.
        note_call(kSeamPresetsForCharacter,
                  nullptr,
                  scan_definitions(output),
                  output != nullptr ? static_cast<std::uint64_t>(output->Num()) : 0);
    }

    auto hook_does_part_meet_requirements(const FPrimaryAssetId* id, const GameplayTagContainer* owned_tags) -> bool
    {
        const auto original = reinterpret_cast<DoesPartIdMeetRequirementsFn>(g_trampolines[kSeamRequirements]);
        if (original == nullptr || id == nullptr)
        {
            return false;
        }

        const bool result = original(id, owned_tags);
        const std::size_t index = watched_index_for_id(*id);
        const std::uint32_t bit =
            index < kWatchedParts.size() ? std::uint32_t{1} << static_cast<std::uint32_t>(index) : 0;
        if (bit != 0 && result)
        {
            g_seams[kSeamRequirements].allowed_mask.fetch_or(bit, std::memory_order_relaxed);
        }
        note_call(kSeamRequirements, owned_tags, bit, 0);
        return result;
    }

    // Not a constexpr table: reinterpret_cast is not a constant expression, and
    // the hooks have five different signatures.
    auto hook_function_address(std::size_t seam) -> std::uint64_t
    {
        switch (seam)
        {
        case kSeamPartDefinitions:
            return reinterpret_cast<std::uint64_t>(&hook_part_definitions);
        case kSeamPartIds:
            return reinterpret_cast<std::uint64_t>(&hook_part_ids);
        case kSeamAllPartIdsMatchingTags:
            return reinterpret_cast<std::uint64_t>(&hook_all_part_ids_matching_tags);
        case kSeamPresetsForCharacter:
            return reinterpret_cast<std::uint64_t>(&hook_presets_for_character);
        case kSeamRequirements:
            return reinterpret_cast<std::uint64_t>(&hook_does_part_meet_requirements);
        default:
            return 0;
        }
    }

    auto unhook_all() -> void
    {
        for (std::size_t index = kSeamCount; index-- > 0;)
        {
            if (g_hooks[index])
            {
                g_hooks[index]->unHook();
                g_hooks[index].reset();
            }
            g_trampolines[index] = 0;
        }
    }

    class ZCOMOpenKitSeamProbeMod final : public RC::CppUserModBase
    {
      public:
        ZCOMOpenKitSeamProbeMod()
        {
            ModName = STR("ZCOMOpenKitSeamProbe");
            ModVersion = STR("0.2.1");
            ModDescription = STR("Observation-only probe naming which Zero Company customization seam enumerates the specialization cards");
            ModAuthors = STR("Open Kit contributors");
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT_PROBE] loaded observation_only=true mutation_capability=false hooks_pending=true\n"));
        }

        ~ZCOMOpenKitSeamProbeMod() override
        {
            unhook_all();
            RC::Output::send<RC::LogLevel::Verbose>(STR("[ZCOM_OPEN_KIT_PROBE] unloaded hooks_active=false\n"));
        }

        auto on_unreal_init() -> void override
        {
            const char* reason = "ok";
            if (!validate_runtime(reason))
            {
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZCOM_OPEN_KIT_PROBE] REFUSED reason={} hooks_active=false\n"), RC::ensure_str(reason));
                return;
            }

            const FName asset_type_name(STR("CustomizationPartDefinition"), RC::Unreal::FNAME_Add);
            for (std::size_t index = 0; index < kWatchedParts.size(); ++index)
            {
                g_watched_ids[index] = FPrimaryAssetId{
                    FPrimaryAssetType{asset_type_name},
                    FName(kWatchedParts[index].asset_name, RC::Unreal::FNAME_Add),
                };
            }

            for (std::size_t index = 0; index < kSeamCount; ++index)
            {
                g_hooks[index] = std::make_unique<PLH::x64Detour>(g_base + kSeams[index].rva,
                                                                  hook_function_address(index),
                                                                  &g_trampolines[index]);
                if (!g_hooks[index]->hook() || g_trampolines[index] == 0)
                {
                    g_hooks[index].reset();
                    g_trampolines[index] = 0;
                    unhook_all();
                    RC::Output::send<RC::LogLevel::Error>(
                        STR("[ZCOM_OPEN_KIT_PROBE] REFUSED reason=hook-install-failed seam={} hooks_active=false\n"),
                        kSeams[index].name);
                    return;
                }
            }

            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT_PROBE] READY hooks_active=true seams={} watched_parts={} build=24874058 observation_only=true\n"),
                static_cast<int>(kSeamCount),
                static_cast<int>(kWatchedParts.size()));
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT_PROBE] open the specialization assign screen and the wardrobe, then quit and attach UE4SS.log\n"));
        }

        // Everything is reported from here rather than from the hooks: these are
        // hot paths, and FName::ToString is not something to call from one.
        auto on_update() -> void override
        {
            report_resolution();
            for (std::size_t index = 0; index < kSeamCount; ++index)
            {
                report_seam(static_cast<Seam>(index));
            }
        }

      private:
        // A watched part missing from a result means nothing unless its
        // definition resolved in the first place -- so report which ids the
        // probe can actually see. This only reads what the hooks recorded; it
        // calls nothing. Resolving from here is exactly what crashed 0.2.0.
        auto report_resolution() -> void
        {
            const auto resolved = g_resolved_mask.load(std::memory_order_relaxed);
            const auto attempted = g_resolve_attempted_mask.load(std::memory_order_relaxed);
            if (resolved == m_reported_resolved && attempted == m_reported_attempted)
            {
                return;
            }
            m_reported_resolved = resolved;
            m_reported_attempted = attempted;

            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT_PROBE] definitions resolved={} tried_and_failed={} not_tried_yet={}\n"),
                describe_mask(resolved),
                describe_mask(attempted & ~resolved),
                describe_mask(all_watched_mask() & ~attempted));
        }

        static constexpr auto all_watched_mask() -> std::uint32_t
        {
            return (std::uint32_t{1} << kWatchedParts.size()) - 1;
        }

        auto report_seam(Seam seam) -> void
        {
            auto& state = g_seams[seam];
            const auto calls = state.calls.load(std::memory_order_relaxed);
            const auto mask = state.seen_mask.load(std::memory_order_relaxed);
            const auto allowed = state.allowed_mask.load(std::memory_order_relaxed);

            const bool changed = calls != m_reported_calls[seam] || mask != m_reported_masks[seam] ||
                                 allowed != m_reported_allowed[seam];
            // A hot seam would otherwise write a line per frame. Report on a
            // change of substance, or once every few hundred frames so a live
            // reader can see it is still running.
            if (!changed && ++m_quiet_frames[seam] < 600)
            {
                return;
            }
            if (!changed && calls == 0)
            {
                m_quiet_frames[seam] = 0;
                return;
            }
            m_quiet_frames[seam] = 0;

            const bool interesting = mask != m_reported_masks[seam] || allowed != m_reported_allowed[seam];
            m_reported_calls[seam] = calls;
            m_reported_masks[seam] = mask;
            m_reported_allowed[seam] = allowed;

            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT_PROBE] seam={} calls={} max_results={} watched={} allowed={}\n"),
                kSeams[seam].name,
                calls,
                state.output_max.load(std::memory_order_relaxed),
                describe_mask(mask),
                describe_mask(allowed));

            (void)interesting;
            const auto used = state.slots_used.load(std::memory_order_acquire);
            for (std::uint32_t index = 0; index < used && index < kMaxCapturedSlots; ++index)
            {
                report_capture(seam, state.slots[index]);
            }
        }

        static auto describe_mask(std::uint32_t mask) -> RC::StringType
        {
            if (mask == 0)
            {
                return RC::StringType{STR("none")};
            }
            RC::StringType out{};
            for (std::size_t index = 0; index < kWatchedParts.size(); ++index)
            {
                if ((mask & (std::uint32_t{1} << static_cast<std::uint32_t>(index))) == 0)
                {
                    continue;
                }
                if (!out.empty())
                {
                    out += STR("|");
                }
                out += kWatchedParts[index].asset_name;
            }
            return out;
        }

        auto report_capture(Seam seam, SlotCapture& capture) -> void
        {
            if (!capture.ready.load(std::memory_order_acquire) ||
                capture.reported.exchange(true, std::memory_order_acq_rel))
            {
                return;
            }

            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT_PROBE] slot seam={} slot={} results={} watched={} owned={} parents={} listed={}\n"),
                kSeams[seam].name,
                capture.slot.ToString(),
                capture.results,
                describe_mask(capture.watched_mask),
                capture.owned_count,
                capture.parent_count,
                capture.stored);

            // The first tag is the slot itself, already named above.
            for (std::uint32_t index = 1; index < capture.stored; ++index)
            {
                RC::Output::send<RC::LogLevel::Verbose>(STR("[ZCOM_OPEN_KIT_PROBE]   tag {}\n"),
                                                        capture.tags[index].ToString());
            }
        }

        std::array<std::uint64_t, kSeamCount> m_reported_calls{};
        std::array<std::uint32_t, kSeamCount> m_reported_masks{};
        std::array<std::uint32_t, kSeamCount> m_reported_allowed{};
        std::array<std::uint32_t, kSeamCount> m_quiet_frames{};
        std::uint32_t m_reported_resolved{};
        std::uint32_t m_reported_attempted{};
    };
} // namespace

#define ZCOM_OPEN_KIT_SEAM_PROBE_API __declspec(dllexport)

extern "C"
{
    ZCOM_OPEN_KIT_SEAM_PROBE_API RC::CppUserModBase* start_mod()
    {
        return new ZCOMOpenKitSeamProbeMod();
    }

    ZCOM_OPEN_KIT_SEAM_PROBE_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
