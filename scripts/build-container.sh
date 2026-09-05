#!/usr/bin/env bash
# Build the Core IoStore containers from the installed game.
#
# OPENKIT_TAG_MODE=mapping is the one to use. It edits the picker's
# SpecializationPartMapping and nothing else, so the picker knows which secondary
# each new primary pairs with. No tag is written to any part, character or
# faction. The native module does the listing; this only does the pairing.
#
# The other modes -- neutral, hero, company, remove -- are superseded
# experiments in making a pak unlock the kits on its own. They are kept because
# the changelog reasons about them, and every one of them writes identity or
# edits requirements, which is what this project exists not to do.
#
# Requires: retoc, the .NET 10 SDK, an installed copy of the game, and a .usmap.
# Nothing from the game is committed; everything is rebuilt from the user's own
# installation.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PAKS="${ZCOM_PAKS:-$HOME/.steam/steam/steamapps/common/Star Wars Zero Company/SWZeroCompany/Content/Paks}"
USMAP="${ZCOM_USMAP:-$ROOT/../lab/private/mappings/ZCOM-5.6.1.usmap}"
RETOC="${RETOC:-$HOME/.local/bin/retoc}"
UASSETAPI="${UASSETAPI:-$ROOT/../lab/private/UAssetAPI/UAssetAPI/UAssetAPI.csproj}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PICKER=BP_SpecializationSelectionVM
CARDS=WBP_FocusTree_SpecializationCards
CARD=WBP_FocusTree_SpecializationCard
TALENTS=WBP_FocusTree_SpecializationTalentCards
TALENT_CARD=WBP_FocusTree_SpecializationTalentCard
FACTION=CPD_Faction_ZeroCompany
WEAPON_LANDING=WBP_Menu_Armory_WeaponLanding
MODE="${OPENKIT_TAG_MODE:-neutral}"
PADAWAN_PARTS=(CPD_TacticalSpec_Padawan CPD_TacticalSpec_PadawanExtended
               CPD_TalentSpec_TheLostPadawan CPD_WeaponSpec_Melee_2H_TelRea)
WARRIOR_PARTS=(CPD_TacticalSpec_Warrior CPD_TalentSpec_TheMandalorian)

[ -x "$RETOC" ] || command -v "$RETOC" >/dev/null || { echo "missing: $RETOC" >&2; exit 1; }
command -v dotnet >/dev/null || { echo "missing: dotnet" >&2; exit 1; }
[ -d "$PAKS" ] || { echo "game Paks not found: $PAKS" >&2; exit 1; }
[ -f "$USMAP" ] || { echo "usmap not found: $USMAP" >&2; exit 1; }
[ -f "$UASSETAPI" ] || { echo "UAssetAPI not found: $UASSETAPI" >&2; exit 1; }

echo "==> extracting stock assets"
for asset in "$PICKER" "$FACTION" "${PADAWAN_PARTS[@]}" "${WARRIOR_PARTS[@]}"; do
    "$RETOC" to-legacy --filter "$asset" --no-shaders --version UE5_6 "$PAKS" "$WORK/stock" >/dev/null
done

# retoc derives each package's path from where the file sits under the input
# root, so every asset must keep the directory it was extracted into. Packing a
# flat directory silently produces a container that overrides nothing.
copy_asset() {
    local name="$1" dest="$2" src rel
    src="$(find "$WORK/stock" -name "$name.uasset" | head -1)"
    [ -n "$src" ] || { echo "not found in the shipped containers: $name" >&2; exit 1; }
    rel="${src#"$WORK/stock/"}"
    mkdir -p "$dest/$(dirname "$rel")"
    cp "$src" "${src%.uasset}.uexp" "$dest/$(dirname "$rel")/" 2>/dev/null || cp "$src" "$dest/$(dirname "$rel")/"
}

# Pack $tree into $out/$stem and prove the result actually overrides. Mounting
# the container against the full shipped set and re-extracting is the only check
# that catches an asset packed at the wrong path: such a container reads back
# perfectly on its own and overrides nothing.
pack_and_verify() {
    local tree="$1" out="$2" stem="$3"
    rm -rf "$out"; mkdir -p "$out"
    "$RETOC" to-zen --version UE5_6 "$tree" "$out/$stem.utoc"
    "$RETOC" verify "$out/$stem.utoc"

    local check="$WORK/check-$stem-$(basename "$out")"
    rm -rf "$check"; mkdir -p "$check/mix"
    ln -sf "$PAKS"/*.utoc "$PAKS"/*.ucas "$PAKS"/*.pak "$check/mix/" 2>/dev/null || true
    cp "$out/$stem".* "$check/mix/"
    local asset want got
    for asset in $(cd "$tree" && find . -name '*.uasset' -printf '%f\n' | sed 's/\.uasset$//'); do
        "$RETOC" to-legacy --filter "$asset" --no-shaders --version UE5_6 \
            "$check/mix" "$check/out" >/dev/null 2>&1
        want="$(cd "$tree" && find . -name "$asset.uasset" | sed 's|^\./||')"
        got="$(cd "$check/out" && find . -name "$asset.uasset" | sed 's|^\./||')"
        [ "$want" = "$got" ] || { echo "    PATH MISMATCH $asset: want $want got ${got:-<nothing>}" >&2; exit 1; }
        # Compare the .uexp, not the .uasset: retoc regenerates package summary
        # offsets on extraction, so headers legitimately differ while the export
        # data - which is everything we edited - round-trips byte-identical.
        cmp -s "${tree}/${want%.uasset}.uexp" "${check}/out/${got%.uasset}.uexp" \
            || { echo "    OVERRIDE NOT WINNING for $asset" >&2; exit 1; }
    done
    echo "    override verified for $(cd "$tree" && find . -name '*.uasset' | wc -l) asset(s)"
}

for variant in both padawan warrior both-swap padawan-swap warrior-swap; do
    echo "==> $variant"
    kits="${variant%-swap}"
    tree="$WORK/$variant"
    rm -rf "$tree"; mkdir -p "$tree"

    copy_asset "$PICKER" "$tree"
    # The faction definition is only carried when it is actually modified;
    # shipping an untouched override would claim the asset for no reason.
    case "$MODE" in neutral|hero) copy_asset "$FACTION" "$tree" ;; esac
    # "mapping" carries the picker alone -- no part is edited, so shipping one
    # would claim an asset for nothing and collide with other mods for nothing.
    if [ "$MODE" != mapping ]; then
        if [ "$kits" = both ] || [ "$kits" = padawan ]; then
            for p in "${PADAWAN_PARTS[@]}"; do copy_asset "$p" "$tree"; done
        fi
        if [ "$kits" = both ] || [ "$kits" = warrior ]; then
            for p in "${WARRIOR_PARTS[@]}"; do copy_asset "$p" "$tree"; done
        fi
    fi

    dotnet run --project "$ROOT/scripts/container-builder/OpenKitContainer.csproj" \
        -c Release -v q -p:UAssetAPI="$UASSETAPI" -- "$tree" "$USMAP" "$variant"

    pack_and_verify "$tree" "$ROOT/container/$variant" "pakchunk99-ZCOMOpenKit_P"
done

# The row refit. Its own container, and not gated on OPENKIT_TAG_MODE, because
# it is not a tag experiment and not optional the way the picker is: the
# overflow is caused by the module, so anyone running the module needs this.
# Distinct chunk number and stem so it mounts alongside the picker container
# rather than colliding with it.
echo "==> ui-fit (specialization row)"
tree="$WORK/ui-fit"
rm -rf "$tree"; mkdir -p "$tree"
for asset in "$CARDS" "$CARD" "$TALENTS" "$TALENT_CARD"; do
    "$RETOC" to-legacy --filter "$asset" --no-shaders --version UE5_6 "$PAKS" "$WORK/stock" >/dev/null
    copy_asset "$asset" "$tree"
done
dotnet run --project "$ROOT/scripts/container-builder/OpenKitContainer.csproj" \
    -c Release -v q -p:UAssetAPI="$UASSETAPI" -- "$tree" "$USMAP" ui-fit
pack_and_verify "$tree" "$ROOT/container/ui-fit" "pakchunk98-ZCOMOpenKitUI_P"

# The Armory's lightsaber lock. Its own container, its own chunk, and NOT
# bundled with anything else -- if this widget fails to construct the Armory
# screen fails completely rather than degrading, which is the same failure mode
# recorded for the assign screen. One asset, one property, easy to drop.
#
# OPENKIT_SABER_ARMORY defaults to "all", and the reason is measured. Lifting
# CanChangeWeaponQuery alone works -- Change Weapon un-greys and lists all eight
# weapon specializations including both sabers -- but that list is *weapon
# classes*, not gear kits. The hilts live behind Customize or Modify Weapon,
# which stay gated unless all three are lifted.
#
# "change" lifts only CanChangeWeaponQuery, which is the smaller and safer edit
# if the other two ever cause trouble: no CPD_WP_Type_* part exists for any melee
# weapon, so those screens may open with little or nothing in them.
echo "==> saber-armoury (the Armory's lightsaber lock)"
tree="$WORK/saber-armoury"
rm -rf "$tree"; mkdir -p "$tree"
"$RETOC" to-legacy --filter "$WEAPON_LANDING" --no-shaders --version UE5_6 "$PAKS" "$WORK/stock" >/dev/null
copy_asset "$WEAPON_LANDING" "$tree"
dotnet run --project "$ROOT/scripts/container-builder/OpenKitContainer.csproj" \
    -c Release -v q -p:UAssetAPI="$UASSETAPI" -- "$tree" "$USMAP" saber-armoury
pack_and_verify "$tree" "$ROOT/container/saber-armoury" "pakchunk97-ZCOMOpenKitSaberArmoury_P"

echo "==> done"
