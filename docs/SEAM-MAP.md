# Seam map

Zero Company ships its own PDB — `SWZeroCompany/Binaries/Win64/SWZeroCompany.pdb`,
267 MB, installed by Steam next to the executable. It carries 1,286,644 public
symbols with full C++ signatures, which means the customization system can be
read as an API rather than guessed at from behaviour.

Everything below was produced from that file and the shipped executable. No
other mod's binary was involved. Reproduce any line with:

```bash
./scripts/resolve-symbols.sh 'UCustomizationStatics'
```

RVAs are module-relative; add the load base (`0x140000000` by default) for a
runtime address.

## Confidence in the addresses

Section `0001` is `.text` and starts at RVA `0x1000`, so a public symbol's
recorded offset plus `0x1000` is its RVA. That mapping is checked, not assumed:
all eight addresses the MIT wardrobe pins by hand land exactly on their
documented targets, and the script resolves each address back through the PDB so
a bad offset shows up as the wrong name instead of a plausible lie.

The installed build is also byte-identical in PE identity to the build the MIT
baseline pins — `TimeDateStamp 0xE10ABE56`, `SizeOfImage 0x0E354000`, Steam build
`24874058` — so its RVAs are valid against this install unmodified.

## 1. The customization catalogue

`UCustomizationStatics` is the whole surface. Four functions can enumerate.
Which one the specialization row actually uses was open when this map was first
written, and was settled by measurement: `GetCustomizationPartDefinitions` takes
every call and the other three are never called at all. See
[INVESTIGATION.md](INVESTIGATION.md) §8.

| RVA | Function |
|---|---|
| `0x63C54D0` | `GetCustomizationPartDefinitions(const FGameplayTagContainer&, TArray<const UCustomizationPartDefinition*>&)` |
| `0x63C50C0` | `GetCustomizationPartIds(const FGameplayTagContainer&, TArray<FPrimaryAssetId>&)` |
| `0x63C51B0` | `GetAllCustomizationPartIdsMatchingTags(const FGameplayTagContainer&, TArray<FPrimaryAssetId>&)` |
| `0x63C7F90` | `GetCustomizationPresetsForCharacter(const AActor*, TArray<const UCustomizationPartDefinition*>&)` |

The gate, and the lookup that turns an id into a definition:

| RVA | Function |
|---|---|
| `0x63C6400` | `DoesPartIdMeetRequirements(const FPrimaryAssetId&, const FGameplayTagContainer&)` |
| `0x63C5660` | `GetCustomizationPartDefinitionFromPartId(const FPrimaryAssetId&)` |
| `0x63C5BF0` | `GetCustomizationPartTagsForPartId(const FPrimaryAssetId&, FGameplayTagContainer&)` |
| `0x63C5950` | `GetAllowedSlotsForPartAssetData(const FAssetData&, FGameplayTagContainer&)` |
| `0x63C6090` | `GetCustomizationDisallowedPartTagsForPartAssetData(const FAssetData&, FGameplayTagContainer&)` |

Applying a choice:

| RVA | Function |
|---|---|
| `0x63C70E0` | `EquipCustomizationParts(UCustomizationInstance*, const TMap<FGameplayTag, FPrimaryAssetId>&, TMap<FGameplayTag, FPrimaryAssetId>&)` |
| `0x63C76C0` | `EquipCustomizationDefinition(UCustomizationInstance*, UCustomizationDefinition*, const FGameplayTagContainer&, TMap<FGameplayTag, FPrimaryAssetId>&)` |
| `0x63C8470` | `ApplyCustomizationPresetToCharacter(const AActor*, const UCustomizationPartDefinition*)` |
| `0x63C8840` | `PreviewCustomizationPresetToCharacter(const AActor*, const UCustomizationPartDefinition*)` |

Note the shape of the equip map: slot tag → part id. Equipping is keyed by slot,
which is why `AllowedSlots` on a part is a requirement and not a category.

## 2. What a part definition can carry

The fragment classes name every capability a `CPD_*` asset can grant. These are
the things "unlocking a kit" actually consists of:

```text
UCustomizationFragmentCharacterClass       the class a part confers
UCustomizationFragmentAbilities            granted abilities
UCustomizationFragmentAbilitySet           an ability set
UBrunoCustomizationFragmentLeveledAbilities  abilities that scale with level
UCustomizationFragmentGearKits             weapons and equipment
UCustomizationFragmentGameplayEffects      applied effects
UCustomizationFragmentGameplayTags         tags the part contributes
UCustomizationFragmentAttributeSets        attributes
UCustomizationFragmentEquipPartToSlot      an authored dependent part
UCustomizationFragmentHelmetVO             the helmet voice preset
UCustomizationApplyRequirementFragment     a requirement on applying it
```

Instance-side mirrors exist for most (`UCustomizationFragmentInstance*`), plus
`UCustomizationStatics::GetAllFragmentInstancesByClass` at `0x63C6A60` to find
them on a live character.

## 3. Specialization availability

Independent of the customization catalogue, there is a system that decides which
specializations a given operator may take:

| RVA | Function |
|---|---|
| `0x72114E0` | `UBrunoCrossTrainingConfiguration::GetCrossTrainingOptions(const AActor*, const AActor*) const` |
| `0x7211720` | `UBrunoCrossTrainingConfiguration::GetSpecializationTags(const AActor*, const AActor*, TArray<FGameplayTag>&) const` |
| `0x7211C80` | `UBrunoCrossTrainingConfiguration::GetFilteredOptions(const AActor*, const AActor*, const TArray<FCrossTrainingOption>&, TArray<const FCrossTrainingOption*>&) const` |
| `0x7212010` | `UBrunoCrossTrainingConfiguration::SelectOption(const UObject*, TArray<const FCrossTrainingOption*>&, TArray<TSubclassOf<UBitReactorGameplayEffect>>&) const` |
| `0x7265490` | `UBrunoStrategyStatics::GetAvailableCrossTrainings(const AActor*, const AActor*)` |
| `0x7265650` | `UBrunoStrategyStatics::ApplyCrossTrainingSelection(AActor*, AActor*, TSubclassOf<UBitReactorGameplayEffect>)` |
| `0x720BDE0` | `ABrunoBondsCentral::GetAvailableCrossTrainings(FGuid, FGuid) const` |
| `0x720BC80` | `ABrunoBondsCentral::ModifyAvailableCrossTrainings(FGuid, FGuid, int)` |

`UBrunoCrossTrainingConfiguration` is a `UPrimaryDataAsset` implementing
`IGameplayTagAssetInterface`, with a `SpecializationCrossTrainingOptions` map
property — so its contents are authored data, readable with `retoc`, not
compiled-in.

Two operators are passed to every one of these, which reads as *this operator
cross-training with that one* rather than as a global availability query. Whether
this system is on the path that lists specialization cards is unverified.

## 4. View models

The UI side, for when the display rather than the data is the problem:

```text
UBitReactorCharacterCustomizationViewModel
UBitReactorCustomizationPartListViewModel     the list of tiles
UBitReactorCustomizationPartViewModel         one tile
UBitReactorCustomizationSlotViewModel         one slot's equipped part
UBrunoLiteCharacterViewModel                  has a Specializations array of
                                              FBrunoLiteCharacterViewModel_Specialization
                                              (field-notified, each with EquippedAsset)
UBrunoCharacterLoadoutViewModel               has WeaponSpecializationTag,
                                              SetWeaponSpecializationType
```

`UBitReactorCustomizationSlotViewModel` was already confirmed at runtime to hold
an equipped part and no candidate list, which is what pushed Core to the native
catalogue in the first place.

## 4b. The tag subsystem -- a third seam

`UCustomizationStatics` is not the only thing that serves parts. Colours come
from a separate subsystem, and nothing in this project's earlier probing saw it
because the probe only hooked the four `UCustomizationStatics` enumerators.

```text
0x63D1C20  UCustomizationPartsTagSubsystem::FilterAssetDataByTags(
               const FGameplayTagContainer&, TArray<FPrimaryAssetId>&) const
0x6FB9AE0  UBitReactorCustomizationPartViewModel::InitializeViewModel()
```

`FilterAssetDataByTags` returns **asset ids filtered by tag**, not part
definitions filtered by slot. That is why a colour part declares no
`Slot.Character.*` tag at all, and why the slot-keyed catalogue table cannot
place one. A hook here identifies which picker it is filling by what the game
already put in the list.

`InitializeViewModel` is where a picker entry's label is decided, which is how
variants that share a display name (`Amber_02`, `Amber_13`) can be told apart.

**The correction this forces:** "there is one enumerator, not four" was measured
and is true *within `UCustomizationStatics`*. It is not true of the game. Before
concluding a part is unreachable, check whether it comes through this subsystem
instead.

## 5. Animation selection

The system that decides which animation set a character uses:

```text
UStanceAnimationSet
UStanceAnimationSetArchetype
FStanceAnimationSetTagMatch   with a MatchingSpecializationTags property
ABitReactorGameCharacter::CacheStanceAnimationSetFromWeapon
                          GetStanceAnimationSetFromArchetype
                          HasWeaponStanceAnimationSet
```

`FStanceAnimationSetTagMatch::MatchingSpecializationTags` is the mechanism behind
the well-known "every soldier swings a lightsaber" failure in the data-only mods:
animation sets are matched against the specialization tags a character carries,
so granting hero identity tags to the whole company matches the hero's set. It is
also where a *correct* animation fix would go.

## 6. Persistence

```text
UCharacterCustomizationSaveGameStatics::SaveCustomizationFragmentInstance(UCustomizationFragmentInstanceBase*)
UCharacterCustomizationSaveGameStatics::LoadCustomizationInstanceWrapper(FBitReactorTypedObjectWrapper&, UObject*)
UCharacterCustomizationInstanceWrapper, UCharacterCustomizationSlotInstanceWrapper,
UCharacterCustomizationVariantInstanceWrapper, UCharacterCustomizationAdditiveInstanceWrapper
UFCustomizationSlotMigration
```

A save stores customization as serialized fragment instances. That bears directly
on what happens to a modded-in part when the mod is removed, and on whether an
existing operator can be changed at all — see G4 and G5 in the gap list.

## 7. Native gameplay tags

Some customization tags are declared in the executable as `FNativeGameplayTag`
under the `CustomizationTags` namespace, for example:

```text
CustomizationTags::Customization_Slot_Character_Specializations_Tactical_Primary
CustomizationTags::Customization_Slot_Character_Specializations_Weapon
```

Most of the tree is data-registered instead, which is why a tag a pak invents is
cleared on load — recorded in the changelog, and consistent with this.
