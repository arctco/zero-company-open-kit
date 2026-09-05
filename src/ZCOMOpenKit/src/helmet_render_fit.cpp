// Helmet render fit -- makes a Mandalorian helmet sit on a head it was not
// authored for.
//
// DERIVED WORK. This file is Sternab's, from the MIT-licensed Zero Company
// Mandalorian Wardrobe (https://github.com/Sternab/ZeroCompanyMandoWardrobe,
// copyright (c) 2026 Sternab), adapted for Open Kit rather than reimplemented.
// The changes are the namespace, the log prefix, and a data-driven family table
// in place of two hardcoded branches. The mechanism, the ABI mirror, the render
// hooks and the two measured scale constants are theirs. See
// THIRD_PARTY_NOTICES.md.
//
// Why it exists: the Mandalorian helmets pick their mesh by `Body.Type`, while a
// stock helmet picks by `Humanoid.Human.Head.Face.*` -- the shape of the face it
// has to fit. So on an operator whose face and body type disagree, the helmet
// was never matched to the head under it, and the face clips through. Reported
// from the game on 2026-09-04 and the reason this was ported.
//
// What it does NOT do: it writes no scene transform, mutates no face visibility,
// and refuses meshes driven by a deformer or a leader pose. It scales the head
// pivot in the render palette only -- the copy of the skinning matrices handed
// to the renderer for one frame -- so nothing it touches is gameplay state or
// is saved. Copyright (c) 2026 Sternab; adaptation (c) 2026 Open Kit
// contributors. MIT.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <polyhook2/Detour/x64Detour.hpp>

#include "helmet_render_fit.hpp"

namespace
{
    using RC::Unreal::FName;
    using RC::Unreal::FUObjectArray;
    using RC::Unreal::UObject;

    // Exact retail build 24874058 identities and symbols resolved from the
    // shipped PDB.  The constructor byte gate is important even though the
    // constructor is not hooked: it gates the DynamicDataMirror layout below.
    constexpr std::uint32_t kExpectedPeTimestamp = 0xE10ABE56;
    constexpr std::uint32_t kExpectedImageSize = 0x0E354000;
    constexpr std::uintptr_t kBroadcastRefreshedRva = 0x63C36A0;
    constexpr std::uintptr_t kGetSkeletalMeshAssetRva = 0x4688A00;
    constexpr std::uintptr_t kGetNumBonesRva = 0x469D920;
    constexpr std::uintptr_t kGetBoneNameRva = 0x469DAD0;
    constexpr std::uintptr_t kUpdateRefToLocalMatricesRva = 0x4EA2060;
    constexpr std::uintptr_t kUpdatePreviousRefToLocalMatricesRva = 0x4EA2270;
    constexpr std::uintptr_t kDynamicDataConstructorRva = 0x4EB8340;

    constexpr std::int32_t kMaximumBoneCount = 768;
    constexpr std::size_t kMaximumComponentInstances = 8192;
    constexpr std::size_t kMaximumTargetComponents = 128;
    constexpr std::uint8_t kMaximumScanAttempts = 4;
    constexpr std::uint32_t kSummaryIntervalFrames = 120;
    constexpr double kFitScaleZ = 1.00;

    // The table. One row per helmet family that needs fitting: the fragment of
    // the skeletal mesh's name that identifies it, and how much to scale it
    // about the head pivot.
    //
    // Both scales are Sternab's, measured by looking at the game; they are not
    // derivable from the data and were not re-derived here. Depth (Y) and width
    // (X) move together because the misfit is a head that is wider and deeper
    // than the one the helmet was authored around; height (Z) is left alone
    // because the helmet already sits correctly on the crown.
    //
    // Open Kit offers four helmet families upstream never did -- Cly, ClyB and
    // Tel-Rea's two. They are deliberately absent: a scale for them would be a
    // number nobody has measured, and fitting a helmet by an invented constant
    // is worse than not fitting it. Adding one is a row here once the game says
    // which of them clip and by how much.
    struct FitFamily
    {
        const RC::Unreal::TCHAR* mesh_name_fragment;
        double scale_x;
        double scale_y;
    };

    constexpr std::array<FitFamily, 2> kFitFamilies{{
        {STR("Man001A_HELM"), 1.07, 1.07},
        {STR("Man002A_HELM"), 1.06, 1.06},
    }};
    // The read-only canary observed 9,605 exact current-palette calls and 32
    // exact previous-palette calls. Exact-retail disassembly then confirmed
    // that LTCG removed the PDB's unused seventh argument: the six arguments
    // declared below are the complete live ABI for build 24874058.
    constexpr bool kEnableMatrixMutation = true;

    constexpr std::array<std::uint8_t, 16> kBroadcastRefreshedBytes{
        0x40, 0x56, 0x48, 0x83, 0xEC, 0x30, 0x80, 0xB9,
        0xC1, 0x02, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xF1,
    };
    constexpr std::array<std::uint8_t, 16> kGetSkeletalMeshAssetBytes{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
        0x99, 0xD8, 0x05, 0x00, 0x00, 0x48, 0x85, 0xDB,
    };
    constexpr std::array<std::uint8_t, 16> kGetNumBonesBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
        0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x0F,
    };
    constexpr std::array<std::uint8_t, 16> kGetBoneNameBytes{
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C,
        0x24, 0x18, 0x56, 0x57, 0x41, 0x56, 0x48, 0x83,
    };
    constexpr std::array<std::uint8_t, 16> kUpdateRefToLocalMatricesBytes{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48,
        0x89, 0x70, 0x18, 0x57, 0x41, 0x54, 0x41, 0x55,
    };
    constexpr std::array<std::uint8_t, 16> kUpdatePreviousRefToLocalMatricesBytes{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48,
        0x89, 0x70, 0x18, 0x57, 0x41, 0x54, 0x41, 0x55,
    };
    constexpr std::array<std::uint8_t, 16> kDynamicDataConstructorBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
    };

    struct Vector3d
    {
        double x{};
        double y{};
        double z{};
    };

    struct Quaternion4d
    {
        double x{};
        double y{};
        double z{};
        double w{1.0};
    };

    struct alignas(16) Transform3d
    {
        Quaternion4d rotation{};
        alignas(16) Vector3d translation{};
        alignas(16) Vector3d scale{1.0, 1.0, 1.0};
    };

    struct alignas(16) Matrix44f
    {
        float m[4][4]{};
    };

    template <typename T>
    struct ArrayHeader
    {
        T* data{};
        std::int32_t num{};
        std::int32_t max{};
    };

    struct ArrayViewRaw
    {
        const void* data{};
        std::int32_t num{};
        std::int32_t padding{};
    };

    struct SharedPointerMirror
    {
        const void* object{};
        const void* controller{};
    };

    // FSkinnedMeshSceneProxy::FSkinnedMeshSceneProxyDynamicData, build
    // 24874058.  Only the explicitly named fields are read by the hooks.
    struct alignas(16) DynamicDataMirror
    {
        FName debug_name{};                         // 0x000
        const void* cloth_simulation_data{};       // 0x008
        const void* mesh_deformer_instances{};     // 0x010
        SharedPointerMirror ref_pose_override{};   // 0x018
        ArrayViewRaw external_morph_weights{};      // 0x028
        ArrayViewRaw component_space_transforms{}; // 0x038
        ArrayViewRaw previous_transforms{};        // 0x048
        ArrayViewRaw bone_visibility_states{};     // 0x058
        ArrayViewRaw previous_visibility_states{}; // 0x068
        ArrayViewRaw leader_bone_map{};            // 0x078
        ArrayViewRaw skin_cache_usage{};            // 0x088
        std::array<std::byte, 8> padding_098{};
        Transform3d component_world_transform{};    // 0x0A0
        std::uint32_t current_revision{};           // 0x100
        std::uint32_t previous_revision{};          // 0x104
        std::uint32_t bone_transform_revision{};    // 0x108
        std::uint16_t number_of_lods{};             // 0x10C
        std::uint8_t flags{};                       // 0x10E
        std::uint8_t padding_10f{};
    };

    static_assert(sizeof(FName) == 8, "render-fit requires the build-24874058 FName ABI");
    static_assert(sizeof(Vector3d) == 24, "render-fit requires the UE5 FVector3d ABI");
    static_assert(sizeof(Quaternion4d) == 32, "render-fit requires the UE5 FQuat4d ABI");
    static_assert(sizeof(Transform3d) == 96, "render-fit requires the UE5 FTransform3d ABI");
    static_assert(sizeof(Matrix44f) == 64, "render-fit requires the UE5 FMatrix44f ABI");
    static_assert(sizeof(ArrayViewRaw) == 16, "render-fit requires the UE5 TArrayView ABI");
    static_assert(sizeof(DynamicDataMirror) == 0x110,
                  "render-fit DynamicDataMirror no longer matches the shipped constructor");
    static_assert(offsetof(DynamicDataMirror, component_space_transforms) == 0x38 &&
                      offsetof(DynamicDataMirror, previous_transforms) == 0x48 &&
                      offsetof(DynamicDataMirror, leader_bone_map) == 0x78 &&
                      offsetof(DynamicDataMirror, component_world_transform) == 0xA0 &&
                      offsetof(DynamicDataMirror, flags) == 0x10E,
                  "render-fit DynamicDataMirror field offsets changed");
    static_assert(std::atomic<std::uintptr_t>::is_always_lock_free &&
                      std::atomic<std::uint64_t>::is_always_lock_free,
                  "render-fit hot paths require lock-free pointer and counter atomics");

    constexpr std::uint8_t kHasLeaderPoseFlag = 1U << 0U;
    constexpr std::uint8_t kHasMeshDeformerFlag = 1U << 1U;

    struct RuntimeIdentity
    {
        std::uintptr_t base{};
        std::uintptr_t image_size{};
    };

    struct RegistrySlot
    {
        std::atomic<std::uintptr_t> asset{};
        std::atomic<std::int32_t> head_bone_index{-1};
        std::atomic<std::int32_t> bone_count{};
    };

    struct RegistryCandidate
    {
        UObject* asset{};
        std::int32_t head_bone_index{-1};
        std::int32_t bone_count{};
        bool seen{};
        bool conflict{};
    };

    struct ScanResult
    {
        std::size_t component_instances{};
        std::size_t invalid_components{};
        std::size_t guarded_native_faults{};
        std::size_t exact_target_components{};
        std::size_t published_assets{};
        std::size_t unresolved_assets{};
        std::size_t conflicting_families{};
        bool component_bound_refused{};
        bool target_bound_refused{};
        // Diagnostic, added by Open Kit. Across 241 scans in game the fit found
        // zero targets, and the counters could not say whether that was because
        // no Mandalorian helmet was being worn or because the scan never sees a
        // helmet component at all. This separates the two: if helmet_meshes is
        // zero the scan is not reaching helmets, and if it is non-zero while
        // exact_target_components stays zero then the name match is wrong.
        std::size_t helmet_meshes{};
    };

    struct HotCounters
    {
        std::atomic<std::uint64_t> current_target_calls{};
        std::atomic<std::uint64_t> previous_target_calls{};
        std::atomic<std::uint64_t> current_validated{};
        std::atomic<std::uint64_t> previous_validated{};
        std::atomic<std::uint64_t> current_components_applied{};
        std::atomic<std::uint64_t> previous_components_applied{};
        std::atomic<std::uint64_t> matrices_applied{};
        std::atomic<std::uint64_t> registry_refusals{};
        std::atomic<std::uint64_t> output_refusals{};
        std::atomic<std::uint64_t> leader_refusals{};
        std::atomic<std::uint64_t> transform_refusals{};
        std::atomic<std::uint64_t> deformer_refusals{};
    };

    struct CounterSnapshot
    {
        std::uint64_t current_target_calls{};
        std::uint64_t previous_target_calls{};
        std::uint64_t current_validated{};
        std::uint64_t previous_validated{};
        std::uint64_t current_components_applied{};
        std::uint64_t previous_components_applied{};
        std::uint64_t matrices_applied{};
        std::uint64_t registry_refusals{};
        std::uint64_t output_refusals{};
        std::uint64_t leader_refusals{};
        std::uint64_t transform_refusals{};
        std::uint64_t deformer_refusals{};
    };

    using BroadcastRefreshedFunction = void(__fastcall*)(UObject*);
    using GetSkeletalMeshAssetFunction = UObject*(__fastcall*)(UObject*);
    using GetNumBonesFunction = std::int32_t(__fastcall*)(const UObject*);
    using GetBoneNameFunction = void(__fastcall*)(const UObject*, FName*, std::int32_t);
    using UpdateRefToLocalMatricesFunction = void(__fastcall*)(
        ArrayHeader<Matrix44f>*,
        const DynamicDataMirror*,
        const UObject*,
        const void*,
        std::int32_t,
        const ArrayHeader<std::uint16_t>*);
    using UpdatePreviousRefToLocalMatricesFunction = void(__fastcall*)(
        ArrayHeader<Matrix44f>*,
        const DynamicDataMirror*,
        const UObject*,
        const void*,
        std::int32_t,
        const ArrayHeader<std::uint16_t>*);

    RuntimeIdentity g_runtime{};
    std::array<RegistrySlot, kFitFamilies.size()> g_registry{};
    HotCounters g_counters{};

    std::uint64_t g_broadcast_refreshed_trampoline{};
    std::uint64_t g_update_ref_to_local_trampoline{};
    std::uint64_t g_update_previous_ref_to_local_trampoline{};
    std::unique_ptr<PLH::x64Detour> g_broadcast_refreshed_hook{};
    std::unique_ptr<PLH::x64Detour> g_update_ref_to_local_hook{};
    std::unique_ptr<PLH::x64Detour> g_update_previous_ref_to_local_hook{};

    std::atomic<bool> g_render_fit_enabled{};
    std::atomic<bool> g_scan_pending{};
    std::atomic<std::uint8_t> g_frames_until_scan{};
    std::atomic<std::uint8_t> g_scan_attempt{};
    std::atomic<std::uint32_t> g_pending_refresh_events{};
    std::uint32_t g_summary_frames{};
    bool g_initialized{};

    auto bytes_match(std::uintptr_t base,
                     std::uintptr_t image_size,
                     std::uintptr_t rva,
                     const std::array<std::uint8_t, 16>& expected) noexcept -> bool
    {
        return rva + expected.size() <= image_size &&
               std::memcmp(reinterpret_cast<const void*>(base + rva),
                           expected.data(),
                           expected.size()) == 0;
    }

    auto validate_runtime(RuntimeIdentity& identity, const char*& reason) noexcept -> bool
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
        if (!bytes_match(base, image_size, kBroadcastRefreshedRva, kBroadcastRefreshedBytes) ||
            !bytes_match(base, image_size, kGetSkeletalMeshAssetRva, kGetSkeletalMeshAssetBytes) ||
            !bytes_match(base, image_size, kGetNumBonesRva, kGetNumBonesBytes) ||
            !bytes_match(base, image_size, kGetBoneNameRva, kGetBoneNameBytes) ||
            !bytes_match(base, image_size, kUpdateRefToLocalMatricesRva,
                         kUpdateRefToLocalMatricesBytes) ||
            !bytes_match(base, image_size, kUpdatePreviousRefToLocalMatricesRva,
                         kUpdatePreviousRefToLocalMatricesBytes) ||
            !bytes_match(base, image_size, kDynamicDataConstructorRva,
                         kDynamicDataConstructorBytes))
        {
            reason = "pdb-target-byte-mismatch";
            return false;
        }

        identity = RuntimeIdentity{base, image_size};
        return true;
    }

    auto contains_fragment(const RC::StringType& value,
                           const RC::Unreal::TCHAR* fragment) -> bool
    {
        return value.find(fragment) != RC::StringType::npos;
    }

    // Diagnostic only: does this component draw *any* helmet, ours or not?
    auto mesh_is_any_helmet(UObject* mesh) -> bool
    {
        return mesh != nullptr && contains_fragment(mesh->GetFullName(), STR("_HELM"));
    }

    auto family_for_mesh(UObject* mesh) -> std::int32_t
    {
        if (mesh == nullptr)
        {
            return -1;
        }
        const auto name = mesh->GetFullName();
        for (std::size_t index = 0; index < kFitFamilies.size(); ++index)
        {
            if (contains_fragment(name, kFitFamilies[index].mesh_name_fragment))
            {
                return static_cast<std::int32_t>(index);
            }
        }
        return -1;
    }

    auto same_name_case_insensitive(const RC::StringType& left,
                                    const RC::Unreal::TCHAR* right) -> bool
    {
        const RC::StringType wanted{right};
        if (left.size() != wanted.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            auto a = left[index];
            auto b = wanted[index];
            if (a >= static_cast<RC::Unreal::TCHAR>('A') &&
                a <= static_cast<RC::Unreal::TCHAR>('Z'))
            {
                a = static_cast<RC::Unreal::TCHAR>(a +
                    static_cast<RC::Unreal::TCHAR>('a' - 'A'));
            }
            if (b >= static_cast<RC::Unreal::TCHAR>('A') &&
                b <= static_cast<RC::Unreal::TCHAR>('Z'))
            {
                b = static_cast<RC::Unreal::TCHAR>(b +
                    static_cast<RC::Unreal::TCHAR>('a' - 'A'));
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    // FindAllOf intentionally includes objects pending teardown. Validate the
    // raw pointer against its current GUObjectArray slot before calling any
    // game method. This is O(1), unlike UObject::IsReal's full-array search.
    auto is_live_uobject_pointer(UObject* object) noexcept -> bool
    {
        if (object == nullptr)
        {
            return false;
        }
        __try
        {
            const auto index = object->GetInternalIndex();
            auto* item = index < 0 ? nullptr : FUObjectArray::IndexToObject(index);
            return item != nullptr && item->GetUObject() == object &&
                   item->IsValid(false) && !item->IsUnreachable();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    auto guarded_get_skeletal_mesh(UObject* component,
                                   UObject*& mesh) noexcept -> bool
    {
        mesh = nullptr;
        __try
        {
            const auto get_mesh = reinterpret_cast<GetSkeletalMeshAssetFunction>(
                g_runtime.base + kGetSkeletalMeshAssetRva);
            mesh = get_mesh(component);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            mesh = nullptr;
            return false;
        }
    }

    auto guarded_get_num_bones(const UObject* component,
                               std::int32_t& bone_count) noexcept -> bool
    {
        bone_count = 0;
        __try
        {
            const auto get_num_bones = reinterpret_cast<GetNumBonesFunction>(
                g_runtime.base + kGetNumBonesRva);
            bone_count = get_num_bones(component);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            bone_count = 0;
            return false;
        }
    }

    auto guarded_get_bone_name(const UObject* component,
                               FName* name,
                               std::int32_t index) noexcept -> bool
    {
        __try
        {
            const auto get_bone_name = reinterpret_cast<GetBoneNameFunction>(
                g_runtime.base + kGetBoneNameRva);
            get_bone_name(component, name, index);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    auto resolve_unique_head_bone(UObject* component,
                                  std::int32_t& head_bone_index,
                                  std::int32_t& bone_count) -> bool
    {
        head_bone_index = -1;
        bone_count = 0;
        if (g_runtime.base == 0 || !is_live_uobject_pointer(component))
        {
            return false;
        }
        // The exact PDB RVAs are USkinnedMeshComponent methods.  The asset
        // pointer is registered separately and must never be passed here.
        if (!guarded_get_num_bones(component, bone_count))
        {
            return false;
        }
        if (bone_count <= 0 || bone_count > kMaximumBoneCount)
        {
            return false;
        }

        std::int32_t matches{};
        for (std::int32_t index = 0; index < bone_count; ++index)
        {
            FName name{};
            if (!guarded_get_bone_name(component, &name, index))
            {
                return false;
            }
            const auto bone_name = name.ToString();
            if (same_name_case_insensitive(bone_name, STR("head")))
            {
                head_bone_index = index;
                ++matches;
            }
        }
        return matches == 1 && head_bone_index >= 0;
    }

    auto clear_registry_slot(RegistrySlot& slot) noexcept -> void
    {
        slot.asset.store(0, std::memory_order_release);
        slot.head_bone_index.store(-1, std::memory_order_relaxed);
        slot.bone_count.store(0, std::memory_order_relaxed);
    }

    auto publish_registry_slot(RegistrySlot& slot,
                               UObject* asset,
                               std::int32_t head_bone_index,
                               std::int32_t bone_count) noexcept -> void
    {
        // Null publication makes a concurrent render lookup fail open while
        // the non-pointer fields are replaced.
        slot.asset.store(0, std::memory_order_release);
        slot.head_bone_index.store(head_bone_index, std::memory_order_relaxed);
        slot.bone_count.store(bone_count, std::memory_order_relaxed);
        slot.asset.store(reinterpret_cast<std::uintptr_t>(asset), std::memory_order_release);
    }

    auto scan_registry() -> ScanResult
    {
        ScanResult result{};
        std::vector<UObject*> components{};
        RC::Unreal::UObjectGlobals::FindAllOf(STR("SkeletalMeshComponent"), components);
        result.component_instances = components.size();
        if (components.size() > kMaximumComponentInstances)
        {
            result.component_bound_refused = true;
            for (auto& slot : g_registry)
            {
                clear_registry_slot(slot);
            }
            return result;
        }

        std::array<RegistryCandidate, kFitFamilies.size()> candidates{};
        for (UObject* component : components)
        {
            if (!is_live_uobject_pointer(component))
            {
                ++result.invalid_components;
                continue;
            }

            UObject* mesh{};
            if (!guarded_get_skeletal_mesh(component, mesh))
            {
                ++result.guarded_native_faults;
                continue;
            }
            if (!is_live_uobject_pointer(mesh))
            {
                continue;
            }
            if (mesh_is_any_helmet(mesh))
            {
                ++result.helmet_meshes;
            }
            const auto family = family_for_mesh(mesh);
            if (family < 0)
            {
                continue;
            }

            ++result.exact_target_components;
            if (result.exact_target_components > kMaximumTargetComponents)
            {
                result.target_bound_refused = true;
                for (auto& slot : g_registry)
                {
                    clear_registry_slot(slot);
                }
                return result;
            }

            std::int32_t head_bone_index{-1};
            std::int32_t bone_count{};
            if (!resolve_unique_head_bone(component, head_bone_index, bone_count))
            {
                ++result.unresolved_assets;
                continue;
            }

            auto& candidate = candidates[static_cast<std::size_t>(family)];
            if (!candidate.seen)
            {
                candidate = RegistryCandidate{mesh, head_bone_index, bone_count, true, false};
            }
            else if (candidate.asset != mesh ||
                     candidate.head_bone_index != head_bone_index ||
                     candidate.bone_count != bone_count)
            {
                candidate.conflict = true;
            }
        }

        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            const auto& candidate = candidates[index];
            if (candidate.conflict)
            {
                ++result.conflicting_families;
                clear_registry_slot(g_registry[index]);
            }
            else if (candidate.seen)
            {
                publish_registry_slot(g_registry[index],
                                      candidate.asset,
                                      candidate.head_bone_index,
                                      candidate.bone_count);
                ++result.published_assets;
            }
            else
            {
                // A complete bounded census with no matching component means
                // retaining the old raw pointer would invite address reuse.
                clear_registry_slot(g_registry[index]);
            }
        }
        return result;
    }

    struct TargetSnapshot
    {
        std::int32_t head_bone_index{-1};
        std::int32_t bone_count{};
        std::int32_t family{-1};
        bool found{};
    };

    auto lookup_target(const UObject* asset) noexcept -> TargetSnapshot
    {
        const auto address = reinterpret_cast<std::uintptr_t>(asset);
        for (std::size_t family = 0; family < g_registry.size(); ++family)
        {
            const auto& slot = g_registry[family];
            if (slot.asset.load(std::memory_order_acquire) == address && address != 0)
            {
                const auto head = slot.head_bone_index.load(std::memory_order_relaxed);
                const auto bones = slot.bone_count.load(std::memory_order_relaxed);
                if (head >= 0 && head < bones && bones > 0 && bones <= kMaximumBoneCount)
                {
                    return TargetSnapshot{
                        head,
                        bones,
                        static_cast<std::int32_t>(family),
                        true,
                    };
                }
                g_counters.registry_refusals.fetch_add(1, std::memory_order_relaxed);
                return {};
            }
        }
        return {};
    }

    auto finite_transform_for_pivot(const Transform3d& transform,
                                    Quaternion4d& normalized_rotation) noexcept -> bool
    {
        const auto& q = transform.rotation;
        const auto& p = transform.translation;
        if (!std::isfinite(q.x) || !std::isfinite(q.y) ||
            !std::isfinite(q.z) || !std::isfinite(q.w) ||
            !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
            std::abs(p.x) > 1000000.0 ||
            std::abs(p.y) > 1000000.0 ||
            std::abs(p.z) > 1000000.0)
        {
            return false;
        }
        const double norm_squared = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (!std::isfinite(norm_squared) || norm_squared < 0.90 || norm_squared > 1.10)
        {
            return false;
        }
        const double inverse_norm = 1.0 / std::sqrt(norm_squared);
        normalized_rotation = Quaternion4d{
            q.x * inverse_norm,
            q.y * inverse_norm,
            q.z * inverse_norm,
            q.w * inverse_norm,
        };
        return true;
    }

    auto make_head_pivot_scale(const Transform3d& head_transform,
                               std::int32_t family,
                               Matrix44f& adjustment) noexcept -> bool
    {
        Quaternion4d q{};
        if (!finite_transform_for_pivot(head_transform, q))
        {
            return false;
        }

        const double x2 = q.x + q.x;
        const double y2 = q.y + q.y;
        const double z2 = q.z + q.z;
        const double xx2 = q.x * x2;
        const double yy2 = q.y * y2;
        const double zz2 = q.z * z2;
        const double yz2 = q.y * z2;
        const double wx2 = q.w * x2;
        const double xy2 = q.x * y2;
        const double wz2 = q.w * z2;
        const double xz2 = q.x * z2;
        const double wy2 = q.w * y2;

        // UE uses row vectors.  This is the row-vector rotation matrix for q.
        const double rotation[3][3]{
            {1.0 - (yy2 + zz2), xy2 + wz2, xz2 - wy2},
            {xy2 - wz2, 1.0 - (xx2 + zz2), yz2 + wx2},
            {xz2 + wy2, yz2 - wx2, 1.0 - (xx2 + yy2)},
        };
        // The registry only ever publishes a family index this table holds, but
        // this runs on the render thread and a bad index would read past the
        // table, so it is checked rather than trusted.
        if (family < 0 || static_cast<std::size_t>(family) >= kFitFamilies.size())
        {
            return false;
        }
        const auto& fit = kFitFamilies[static_cast<std::size_t>(family)];
        const double scale[3]{fit.scale_x, fit.scale_y, kFitScaleZ};

        double linear[3][3]{};
        for (std::size_t row = 0; row < 3; ++row)
        {
            for (std::size_t column = 0; column < 3; ++column)
            {
                for (std::size_t axis = 0; axis < 3; ++axis)
                {
                    linear[row][column] +=
                        rotation[axis][row] * scale[axis] * rotation[axis][column];
                }
                if (!std::isfinite(linear[row][column]))
                {
                    return false;
                }
                adjustment.m[row][column] = static_cast<float>(linear[row][column]);
            }
            adjustment.m[row][3] = 0.0F;
        }

        const double pivot[3]{
            head_transform.translation.x,
            head_transform.translation.y,
            head_transform.translation.z,
        };
        for (std::size_t column = 0; column < 3; ++column)
        {
            double transformed{};
            for (std::size_t row = 0; row < 3; ++row)
            {
                transformed += pivot[row] * linear[row][column];
            }
            const double translation = pivot[column] - transformed;
            if (!std::isfinite(translation) || std::abs(translation) > 1000000.0)
            {
                return false;
            }
            adjustment.m[3][column] = static_cast<float>(translation);
        }
        adjustment.m[3][3] = 1.0F;
        return true;
    }

    auto post_multiply(Matrix44f& matrix, const Matrix44f& adjustment) noexcept -> void
    {
        Matrix44f result{};
        for (std::size_t row = 0; row < 4; ++row)
        {
            for (std::size_t column = 0; column < 4; ++column)
            {
                float value{};
                for (std::size_t axis = 0; axis < 4; ++axis)
                {
                    value += matrix.m[row][axis] * adjustment.m[axis][column];
                }
                result.m[row][column] = value;
            }
        }
        matrix = result;
    }

    enum class PaletteKind : std::uint8_t
    {
        Current,
        Previous,
    };

    auto apply_render_fit(ArrayHeader<Matrix44f>* output,
                          const DynamicDataMirror* dynamic_data,
                          const UObject* asset,
                          PaletteKind kind) noexcept -> void
    {
        if (!g_render_fit_enabled.load(std::memory_order_relaxed))
        {
            return;
        }

        const TargetSnapshot target = lookup_target(asset);
        if (!target.found)
        {
            return;
        }

        auto& target_calls = kind == PaletteKind::Current
            ? g_counters.current_target_calls
            : g_counters.previous_target_calls;
        target_calls.fetch_add(1, std::memory_order_relaxed);

        if (output == nullptr || output->data == nullptr ||
            output->num != target.bone_count || output->max < output->num ||
            output->num <= 0 || output->num > kMaximumBoneCount ||
            dynamic_data == nullptr)
        {
            g_counters.output_refusals.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if ((dynamic_data->flags & kHasMeshDeformerFlag) != 0)
        {
            g_counters.deformer_refusals.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        const ArrayViewRaw& transform_view = kind == PaletteKind::Current
            ? dynamic_data->component_space_transforms
            : dynamic_data->previous_transforms;
        if (transform_view.data == nullptr || transform_view.num <= 0 ||
            transform_view.num > kMaximumBoneCount)
        {
            g_counters.transform_refusals.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        std::int32_t transform_index = target.head_bone_index;
        if ((dynamic_data->flags & kHasLeaderPoseFlag) != 0)
        {
            const auto& map = dynamic_data->leader_bone_map;
            if (map.data == nullptr || map.num != target.bone_count ||
                target.head_bone_index < 0 || target.head_bone_index >= map.num)
            {
                g_counters.leader_refusals.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            const auto* indices = static_cast<const std::int32_t*>(map.data);
            transform_index = indices[target.head_bone_index];
        }
        if (transform_index < 0 || transform_index >= transform_view.num)
        {
            g_counters.leader_refusals.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        const auto* transforms = static_cast<const Transform3d*>(transform_view.data);
        Matrix44f adjustment{};
        if (!make_head_pivot_scale(transforms[transform_index],
                                   target.family,
                                   adjustment))
        {
            g_counters.transform_refusals.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        auto& validated = kind == PaletteKind::Current
            ? g_counters.current_validated
            : g_counters.previous_validated;
        validated.fetch_add(1, std::memory_order_relaxed);

        if constexpr (!kEnableMatrixMutation)
        {
            // Compile-time safety gate retained for any future read-only ABI
            // canary. The active build enables the bounded palette mutation.
            return;
        }

        for (std::int32_t index = 0; index < output->num; ++index)
        {
            post_multiply(output->data[index], adjustment);
        }
        auto& applied = kind == PaletteKind::Current
            ? g_counters.current_components_applied
            : g_counters.previous_components_applied;
        applied.fetch_add(1, std::memory_order_relaxed);
        g_counters.matrices_applied.fetch_add(
            static_cast<std::uint64_t>(output->num), std::memory_order_relaxed);
    }

    auto hook_update_ref_to_local_matrices(
        ArrayHeader<Matrix44f>* output,
        const DynamicDataMirror* dynamic_data,
        const UObject* asset,
        const void* render_data,
        std::int32_t lod_index,
        const ArrayHeader<std::uint16_t>* extra_required_bones) -> void
    {
        const auto original = reinterpret_cast<UpdateRefToLocalMatricesFunction>(
            g_update_ref_to_local_trampoline);
        if (original == nullptr)
        {
            return;
        }
        original(output,
                 dynamic_data,
                 asset,
                 render_data,
                 lod_index,
                 extra_required_bones);
        apply_render_fit(output, dynamic_data, asset, PaletteKind::Current);
    }

    auto hook_update_previous_ref_to_local_matrices(
        ArrayHeader<Matrix44f>* output,
        const DynamicDataMirror* dynamic_data,
        const UObject* asset,
        const void* render_data,
        std::int32_t lod_index,
        const ArrayHeader<std::uint16_t>* extra_required_bones) -> void
    {
        const auto original = reinterpret_cast<UpdatePreviousRefToLocalMatricesFunction>(
            g_update_previous_ref_to_local_trampoline);
        if (original == nullptr)
        {
            return;
        }
        original(output,
                 dynamic_data,
                 asset,
                 render_data,
                 lod_index,
                 extra_required_bones);
        apply_render_fit(output, dynamic_data, asset, PaletteKind::Previous);
    }

    auto schedule_scan() noexcept -> void
    {
        g_scan_attempt.store(0, std::memory_order_release);
        g_frames_until_scan.store(2, std::memory_order_release);
        g_scan_pending.store(true, std::memory_order_release);
        g_pending_refresh_events.fetch_add(1, std::memory_order_relaxed);
    }

    auto hook_broadcast_refreshed(UObject* customization) -> void
    {
        const auto original = reinterpret_cast<BroadcastRefreshedFunction>(
            g_broadcast_refreshed_trampoline);
        if (original == nullptr)
        {
            return;
        }
        original(customization);
        schedule_scan();
    }

    auto clear_registry() noexcept -> void
    {
        for (auto& slot : g_registry)
        {
            clear_registry_slot(slot);
        }
    }

    auto take_counters() noexcept -> CounterSnapshot
    {
        return CounterSnapshot{
            g_counters.current_target_calls.exchange(0, std::memory_order_relaxed),
            g_counters.previous_target_calls.exchange(0, std::memory_order_relaxed),
            g_counters.current_validated.exchange(0, std::memory_order_relaxed),
            g_counters.previous_validated.exchange(0, std::memory_order_relaxed),
            g_counters.current_components_applied.exchange(0, std::memory_order_relaxed),
            g_counters.previous_components_applied.exchange(0, std::memory_order_relaxed),
            g_counters.matrices_applied.exchange(0, std::memory_order_relaxed),
            g_counters.registry_refusals.exchange(0, std::memory_order_relaxed),
            g_counters.output_refusals.exchange(0, std::memory_order_relaxed),
            g_counters.leader_refusals.exchange(0, std::memory_order_relaxed),
            g_counters.transform_refusals.exchange(0, std::memory_order_relaxed),
            g_counters.deformer_refusals.exchange(0, std::memory_order_relaxed),
        };
    }

    auto snapshot_nonzero(const CounterSnapshot& snapshot) noexcept -> bool
    {
        return snapshot.current_target_calls != 0 ||
               snapshot.previous_target_calls != 0 ||
               snapshot.current_validated != 0 ||
               snapshot.previous_validated != 0 ||
               snapshot.current_components_applied != 0 ||
               snapshot.previous_components_applied != 0 ||
               snapshot.matrices_applied != 0 ||
               snapshot.registry_refusals != 0 ||
               snapshot.output_refusals != 0 ||
               snapshot.leader_refusals != 0 ||
               snapshot.transform_refusals != 0 ||
               snapshot.deformer_refusals != 0;
    }

    auto unhook_all() noexcept -> void
    {
        g_render_fit_enabled.store(false, std::memory_order_release);
        if (g_update_ref_to_local_hook)
        {
            g_update_ref_to_local_hook->unHook();
        }
        if (g_update_previous_ref_to_local_hook)
        {
            g_update_previous_ref_to_local_hook->unHook();
        }
        if (g_broadcast_refreshed_hook)
        {
            g_broadcast_refreshed_hook->unHook();
        }
        g_update_ref_to_local_hook.reset();
        g_update_previous_ref_to_local_hook.reset();
        g_broadcast_refreshed_hook.reset();
        g_update_ref_to_local_trampoline = 0;
        g_update_previous_ref_to_local_trampoline = 0;
        g_broadcast_refreshed_trampoline = 0;
    }
}

namespace ZCOMOpenKit::HelmetRenderFit
{
    auto initialize() -> bool
    {
        if (g_initialized)
        {
            return true;
        }

        const char* reason = "ok";
        if (!validate_runtime(g_runtime, reason))
        {
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZCOM_OPEN_KIT] helmet_render_fit_REFUSED reason={} hooks_active=false\n"),
                RC::ensure_str(reason));
            return false;
        }

        clear_registry();
        take_counters();
        g_summary_frames = 0;

        g_broadcast_refreshed_hook = std::make_unique<PLH::x64Detour>(
            g_runtime.base + kBroadcastRefreshedRva,
            reinterpret_cast<std::uint64_t>(&hook_broadcast_refreshed),
            &g_broadcast_refreshed_trampoline);
        if (!g_broadcast_refreshed_hook->hook() || g_broadcast_refreshed_trampoline == 0)
        {
            unhook_all();
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZCOM_OPEN_KIT] helmet_render_fit_REFUSED reason=refresh-hook-install-failed hooks_active=false\n"));
            return false;
        }

        g_update_previous_ref_to_local_hook = std::make_unique<PLH::x64Detour>(
            g_runtime.base + kUpdatePreviousRefToLocalMatricesRva,
            reinterpret_cast<std::uint64_t>(&hook_update_previous_ref_to_local_matrices),
            &g_update_previous_ref_to_local_trampoline);
        if (!g_update_previous_ref_to_local_hook->hook() ||
            g_update_previous_ref_to_local_trampoline == 0)
        {
            unhook_all();
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZCOM_OPEN_KIT] helmet_render_fit_REFUSED reason=previous-matrix-hook-install-failed hooks_active=false\n"));
            return false;
        }

        g_update_ref_to_local_hook = std::make_unique<PLH::x64Detour>(
            g_runtime.base + kUpdateRefToLocalMatricesRva,
            reinterpret_cast<std::uint64_t>(&hook_update_ref_to_local_matrices),
            &g_update_ref_to_local_trampoline);
        if (!g_update_ref_to_local_hook->hook() || g_update_ref_to_local_trampoline == 0)
        {
            unhook_all();
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZCOM_OPEN_KIT] helmet_render_fit_REFUSED reason=current-matrix-hook-install-failed hooks_active=false\n"));
            return false;
        }

        g_initialized = true;
        g_render_fit_enabled.store(true, std::memory_order_release);
        schedule_scan();
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[ZCOM_OPEN_KIT] helmet_render_fit_READY hooks=refresh,current-matrices,previous-matrices ")
            STR("build=24874058 registry=bounded-game-thread families={} ")
            STR("mode=head-pivot-render-palette fit_space=head-pivot-render-palette ")
            STR("scene_transform_writes=false face_visibility_mutation=false ")
            STR("render_hook_allocations=false render_hook_logs=false mesh_deformers=refused ")
            STR("leader_pose=refused\n"),
            static_cast<int>(kFitFamilies.size()));
        return true;
    }

    auto update() -> void
    {
        if (!g_initialized)
        {
            return;
        }

        const auto refreshes = g_pending_refresh_events.exchange(0, std::memory_order_relaxed);
        if (refreshes != 0)
        {
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZCOM_OPEN_KIT] helmet_render_fit_refresh count={} scan_pending=true\n"),
                refreshes);
        }

        if (g_scan_pending.load(std::memory_order_acquire))
        {
            const auto frames = g_frames_until_scan.load(std::memory_order_acquire);
            if (frames != 0)
            {
                g_frames_until_scan.store(static_cast<std::uint8_t>(frames - 1),
                                          std::memory_order_release);
            }
            else
            {
                const ScanResult result = scan_registry();
                RC::Output::send<RC::LogLevel::Verbose>(
                    STR("[ZCOM_OPEN_KIT] helmet_render_fit_registry_scan components={} helmet_meshes={} invalid_components={} guarded_native_faults={} target_components={} published_assets={} unresolved_assets={} conflicts={} component_bound_refused={} target_bound_refused={}\n"),
                    result.component_instances,
                    result.helmet_meshes,
                    result.invalid_components,
                    result.guarded_native_faults,
                    result.exact_target_components,
                    result.published_assets,
                    result.unresolved_assets,
                    result.conflicting_families,
                    result.component_bound_refused,
                    result.target_bound_refused);

                const auto attempt = static_cast<std::uint8_t>(
                    g_scan_attempt.fetch_add(1, std::memory_order_acq_rel) + 1);
                const bool complete = !result.component_bound_refused &&
                                      !result.target_bound_refused &&
                                      result.published_assets == g_registry.size();
                if (complete || attempt >= kMaximumScanAttempts)
                {
                    g_scan_pending.store(false, std::memory_order_release);
                }
                else
                {
                    constexpr std::array<std::uint8_t, 4> delays{8, 30, 120, 120};
                    g_frames_until_scan.store(delays[attempt - 1], std::memory_order_release);
                }
            }
        }

        if (++g_summary_frames >= kSummaryIntervalFrames)
        {
            g_summary_frames = 0;
            const CounterSnapshot snapshot = take_counters();
            if (snapshot_nonzero(snapshot))
            {
                // This is the update/game thread.  Render hooks only touch the
                // lock-free counters above and never log or allocate.
                RC::Output::send<RC::LogLevel::Verbose>(
                    STR("[ZCOM_OPEN_KIT] helmet_render_fit_summary current_target={} previous_target={} current_validated={} previous_validated={} current_applied={} previous_applied={} matrices={} registry_refused={} output_refused={} leader_refused={} transform_refused={} deformer_refused={} mutation_enabled=true\n"),
                    snapshot.current_target_calls,
                    snapshot.previous_target_calls,
                    snapshot.current_validated,
                    snapshot.previous_validated,
                    snapshot.current_components_applied,
                    snapshot.previous_components_applied,
                    snapshot.matrices_applied,
                    snapshot.registry_refusals,
                    snapshot.output_refusals,
                    snapshot.leader_refusals,
                    snapshot.transform_refusals,
                    snapshot.deformer_refusals);
            }
        }
    }

    auto shutdown() -> void
    {
        if (!g_initialized && !g_broadcast_refreshed_hook &&
            !g_update_ref_to_local_hook && !g_update_previous_ref_to_local_hook)
        {
            return;
        }

        unhook_all();
        clear_registry();
        g_scan_pending.store(false, std::memory_order_release);
        g_pending_refresh_events.store(0, std::memory_order_relaxed);
        g_initialized = false;
        const CounterSnapshot snapshot = take_counters();
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[ZCOM_OPEN_KIT] helmet_render_fit_unloaded hooks_active=false current_applied={} previous_applied={} matrices={} refusals={} scene_transform_writes=false\n"),
            snapshot.current_components_applied,
            snapshot.previous_components_applied,
            snapshot.matrices_applied,
            snapshot.registry_refusals + snapshot.output_refusals +
                snapshot.leader_refusals + snapshot.transform_refusals +
                snapshot.deformer_refusals);
    }
}
