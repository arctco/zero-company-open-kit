using UAssetAPI;
using UAssetAPI.Unversioned;
using UAssetAPI.ExportTypes;
using UAssetAPI.UnrealTypes;
using UAssetAPI.PropertyTypes.Objects;
using UAssetAPI.PropertyTypes.Structs;

/// Edits an extracted asset tree in place for one Open Kit variant.
///
/// Two edits, and both are needed. Either one alone changes nothing a player
/// can see:
///
///   listing      BP_SpecializationSelectionVM's SpecializationPartMapping is
///                the list the focus tree offers. Warrior and Padawan are not
///                on it, so they are never drawn.
///   eligibility  Each hero part's AllowedSlots requires the target to carry
///                that hero's Info.Name tag as well as the slot tag. A recruit
///                carries no hero's name, so even a listed part is refused.
///
/// There are two routes through the eligibility half. Writing a hero's Info.Name
/// onto the company's faction definition satisfies the requirement, because the
/// gate is a lookup on that tag. It also lands the name on every character in
/// the company, and everything else keyed on that identity comes with it: the
/// lightsaber melee animation set, the hero gear kit in place of the standard
/// one, and hero identity held from the very first character, which the tutorial
/// does not survive.
///
/// Removing the requirement from the six parts instead reaches the same place
/// without telling anyone they are someone else. That costs a hook, which is
/// why the module exists.
class Program {
    const string INERT_TAG = "br.Customization.Slot.Character.Class";
    const string HERO_NAME_PREFIX = "br.Customization.Part.Character.Info.Name";
    // Every operator in the player's company carries this through
    // CPD_Faction_ZeroCompany. Requiring it instead of a hero's name keeps the
    // part gated to your own people, grants nobody an identity, and sits outside
    // the Info.Name subtree the lock queries match.
    const string COMPANY_TAG = "br.Customization.Part.Character.Info.Faction.ZeroCompany";

    // The game finds candidate parts *by* the Info.Name tags an operator carries;
    // a part with no Info.Name in its AllowedSlots is never looked up. That is why
    // removing the hero name did nothing, and why an Info.Faction tag did not
    // substitute - wrong branch of the tag tree.
    //
    // So keep the mechanism and drop the identity: give the company one neutral
    // name tag of our own and have the hero parts require that instead. Every
    // operator becomes findable for them; nobody is told they are Tel-Rea or Cly,
    // so nothing keyed on hero identity fires.
    // The lookup matches an exact Info.Name leaf. Two attempts proved it:
    // "...Info.Name.OpenKit", a tag of our own, did nothing because gameplay tags
    // are validated against a registry the game builds from data and a pak cannot
    // add to it; and the registered parent "...Info.Name" did nothing either,
    // because a parent does not satisfy a match on a leaf.
    //
    // So a hero's name it must be, and the only choice left is *whose*. The other
    // data mod grants Tel-Rea's and Cly's, which is exactly what gives every
    // soldier a lightsaber melee animation and a Mandalorian arm cannon: those two
    // carry a class, a specialization, a talent and a weapon each.
    //
    // Kabb Uppercut carries none of that. He has no CPD_Char_Class_Hero_*, no
    // TacticalSpec, no TalentSpec; the only parts naming him are
    // CPD_WeaponSpec_Blaster_Rifle and _Pistol, both already available to
    // everyone. Borrowing his name makes the six kit parts findable and, as far as
    // the data shows, brings nothing else with it.
    const string NEUTRAL_TAG = "br.Customization.Part.Character.Info.Name.KabbUppercut";
    // Diagnostic only, never shipped. Reproduces the data-only mechanism - the
    // real hero names on the company faction - which is how the lookup was
    // confirmed to match an exact Info.Name leaf.
    static readonly string[] HERO_TAGS = {
        "br.Customization.Part.Character.Info.Name.Tel-ReaVokoss",
        "br.Customization.Part.Character.Info.Name.ClyKullervo",
    };
    const string FACTION_ASSET = "CPD_Faction_ZeroCompany";
    const string SPEC_DIR = "/Game/Game/Customizations/Characters/Common/Specialization/Tactical/";
    const string PICKER = "BP_SpecializationSelectionVM";

    static readonly string[] PadawanParts = {
        "CPD_TacticalSpec_Padawan", "CPD_TacticalSpec_PadawanExtended",
        "CPD_TalentSpec_TheLostPadawan", "CPD_WeaponSpec_Melee_2H_TelRea",
    };
    static readonly string[] WarriorParts = {
        "CPD_TacticalSpec_Warrior", "CPD_TalentSpec_TheMandalorian",
    };

    // ---- the specialization row refit -------------------------------------
    //
    // The row is a horizontal BitReactorListView of fixed-size cards drawn
    // inside a frame the game authors at 1042 wide
    // (WBP_FocusTree_Specilization_Backing). The list's slot inside that frame
    // is inset 44 left and 32 right, so the row has 1042 - 76 = 966 to work in.
    //
    // Stock is eight cards, 90 wide, 32 between: 8*90 + 7*32 = 944, fitting
    // with 22 to spare. That 944-in-966 is also the proof that the spacing
    // falls *between* entries rather than around each one -- 8*(90+32) = 976
    // would not fit, and stock does.
    //
    // Core adds two, and 10*90 + 9*32 = 1188 does not fit. The row outgrows the
    // frame, and because the assign screen stacks its rows in a right-aligned
    // VerticalBox the excess appears on the left, outside the panel -- which is
    // exactly how the defect was reported.
    //
    // Widening the frame was rejected on measurement, not taste. The ability
    // panel to the left is a fixed 673 wide, and in
    // WBP_FocusTree_AssignSpecialization_New both columns live in one Overlay
    // where they already sit 23 apart at a 1920 design width. A frame wide
    // enough for ten cards at stock spacing (1188 + 76 = 1264) would overlap
    // that panel by some 245 and cover it.
    //
    // So the row is refitted to the space it has. Ten cards at 80 wide with 16
    // between is 10*80 + 9*16 = 944 -- the same width stock uses for eight, and
    // the same 22 of slack. Whatever the list does with the remainder, and
    // whatever it reserves for a scrollbar, it does it exactly as it already
    // does for the stock row.
    //
    // The talent row below it is the same shape and the same problem, and the
    // first pass missed it by asking the wrong question -- it measured what the
    // row *holds* (ten, 10*74 + 9*20 = 920, inside the same 966) and never asked
    // what the module makes it. The module adds two talents, so it is twelve:
    // 12*74 + 11*20 = 1108, which is 142 over.
    //
    // That one is worse than it looks, because nothing above these rows caps
    // their width -- the frame art is nine-slice and follows the widest of them.
    // So the talent row was quietly stretching every panel on the screen to
    // about 1175 and dragging their left edges under the fixed 673 ability panel
    // beside them, which is what put the permanent "SPECIALIZATION ABILITIES"
    // card on top of the talents and clipped "TALENT ABILITY" to "LENT ABILITY".
    // The empty space to the right of the specialization cards was the same
    // stretch seen from the other end.
    //
    // Twelve at 66 with 12 between is 924, against the 920 stock uses for ten --
    // the same principle as the row above, and the same 11% off the card. Width
    // and height move together: a talent card is square at 74x74 and the passive
    // ones draw a circle in it, so shrinking one axis would draw ellipses.
    const string CARDS = "WBP_FocusTree_SpecializationCards";
    const string CARD = "WBP_FocusTree_SpecializationCard";
    const string TALENTS = "WBP_FocusTree_SpecializationTalentCards";
    const string TALENT_CARD = "WBP_FocusTree_SpecializationTalentCard";
    const float ROW_SPACE = 966f;
    static readonly float CardWidth = EnvFloat("OPENKIT_CARD_WIDTH", 80f);
    static readonly float CardSpacing = EnvFloat("OPENKIT_CARD_SPACING", 16f);
    static readonly float TalentWidth = EnvFloat("OPENKIT_TALENT_WIDTH", 66f);
    static readonly float TalentSpacing = EnvFloat("OPENKIT_TALENT_SPACING", 12f);

    static float EnvFloat(string name, float fallback) {
        var raw = Environment.GetEnvironmentVariable(name);
        return string.IsNullOrEmpty(raw) ? fallback : float.Parse(raw);
    }

    static UAsset A = null!;
    static Usmap Mappings = null!;
    static string Mode = Environment.GetEnvironmentVariable("OPENKIT_TAG_MODE") ?? "neutral";

    static FPackageIndex AddSpec(string name) {
        for (int i = 0; i < A.Imports.Count; i++)
            if (A.Imports[i].ObjectName.ToString() == name) return FPackageIndex.FromImport(i);
        A.Imports.Add(new Import("/Script/CoreUObject", "Package", new FPackageIndex(0), SPEC_DIR + name, false, A));
        var pkg = FPackageIndex.FromImport(A.Imports.Count - 1);
        A.Imports.Add(new Import(SPEC_DIR + "BP_CPD_TacticalSpec_Base", "BP_CPD_TacticalSpec_Base_C", pkg, name, false, A));
        return FPackageIndex.FromImport(A.Imports.Count - 1);
    }

    /// Adds the hero entries to the picker's list, and for the -swap variants
    /// retargets the two lock queries so an equipped hero kit can be changed
    /// again. The queries' tag is replaced rather than their token stream
    /// rewritten, so the stream stays a valid ANY-of-one-tag query and nothing
    /// depends on having guessed FGameplayTagQuery's encoding.
    static void EditPicker(string path, string variant) {
        A = new UAsset(path, EngineVersion.VER_UE5_6, Mappings);
        var cdo = A.Exports.OfType<NormalExport>().First(e => e.ObjectName.ToString().StartsWith("Default__"));
        var map = (MapPropertyData)cdo.Data.First(p => p.Name.ToString() == "SpecializationPartMapping");
        int before = map.Value.Count;
        var template = map.Value.Keys.First();

        void Pair(string primary, string? secondary) {
            var k = new ObjectPropertyData(template.Name) { Value = AddSpec(primary) };
            var v = new ObjectPropertyData(template.Name) {
                Value = secondary is null ? new FPackageIndex(0) : AddSpec(secondary) };
            map.Value.Add(k, v);
        }

        var kits = variant.Replace("-swap", "");
        if (kits is "both" or "padawan") Pair("CPD_TacticalSpec_Padawan", "CPD_TacticalSpec_PadawanExtended");
        if (kits is "both" or "warrior") Pair("CPD_TacticalSpec_Warrior", null);

        if (variant.EndsWith("-swap")) {
            foreach (var name in new[] { "SpecLockedQuery", "TalentLockedQuery" }) {
                var q = (StructPropertyData)cdo.Data.First(x => x.Name.ToString() == name);
                var dict = (ArrayPropertyData)q.Value.First(x => x.Name.ToString() == "TagDictionary");
                foreach (var entry in dict.Value) {
                    var tag = (StructPropertyData)entry;
                    var tagName = (NamePropertyData)tag.Value.First(x => x.Name.ToString() == "TagName");
                    tagName.Value = new FName(A, INERT_TAG);
                }
            }
        }

        Console.WriteLine($"  picker: SpecializationPartMapping {before} -> {map.Value.Count}"
            + (variant.EndsWith("-swap") ? ", lock queries retargeted" : ""));
        A.Write(path);
    }

    /// Removes the hero-name requirement from a part's AllowedSlots, leaving the
    /// slot tag that legitimately belongs there. Refuses a part that does not
    /// carry one rather than silently shipping an unchanged override.
    /// Appends the neutral name tag to the company's faction definition, which is
    /// the tag set every operator in the player's roster carries.
    static void EditFaction(string path) {
        A = new UAsset(path, EngineVersion.VER_UE5_6, Mappings);
        var wanted = Mode == "hero" ? HERO_TAGS : new[] { NEUTRAL_TAG };
        int added = 0;
        foreach (var ex in A.Exports.OfType<NormalExport>())
            foreach (var prop in ex.Data) {
                if (prop is not StructPropertyData st || st.Name.ToString() != "GameplayTags") continue;
                foreach (var inner in st.Value) {
                    if (inner is not GameplayTagContainerPropertyData tags) continue;
                    var list = (tags.Value ?? Array.Empty<FName>()).ToList();
                    foreach (var tag in wanted) {
                        if (list.Any(x => x.ToString() == tag)) continue;
                        list.Add(new FName(A, tag));
                        added++;
                    }
                    tags.Value = list.ToArray();
                }
            }
        if (added == 0) throw new InvalidOperationException(
            $"{FACTION_ASSET}: no GameplayTags container found - refusing to ship a no-op override");
        Console.WriteLine($"  {FACTION_ASSET}: + {string.Join(", ", wanted)}");
        A.Write(path);
    }

    static void EditPart(string path, string name, bool replaceWithCompany) {
        A = new UAsset(path, EngineVersion.VER_UE5_6, Mappings);
        int removed = 0;
        foreach (var ex in A.Exports.OfType<NormalExport>())
            foreach (var prop in ex.Data) {
                if (prop is not StructPropertyData st || st.Name.ToString() != "AllowedSlots") continue;
                foreach (var inner in st.Value) {
                    if (inner is not GameplayTagContainerPropertyData tags || tags.Value is null) continue;
                    var kept = tags.Value.Where(t => !t.ToString().StartsWith(HERO_NAME_PREFIX)).ToList();
                    int dropped = tags.Value.Length - kept.Count;
                    removed += dropped;
                    if (dropped > 0) {
                        if (Mode == "neutral") kept.Add(new FName(A, NEUTRAL_TAG));
                        else if (Mode == "company") kept.Add(new FName(A, COMPANY_TAG));
                    }
                    tags.Value = kept.ToArray();
                }
            }
        if (removed == 0) throw new InvalidOperationException(
            $"{name}: no {HERO_NAME_PREFIX}.* in AllowedSlots - the game changed, refusing to ship a no-op override");
        Console.WriteLine($"  {name}: {removed} hero-name requirement(s) "
            + (Mode == "remove" ? "removed from" : $"-> {(Mode == "neutral" ? NEUTRAL_TAG : COMPANY_TAG)} in") + " AllowedSlots");
        A.Write(path);
    }


    /// Sets one float on one named export. Refuses a no-op: a missing export, a
    /// missing property or a value that is already the target all mean the game
    /// changed under us, and shipping the override anyway would claim an asset
    /// and collide with unrelated mods for nothing.
    static void SetFloat(string path, string export, string prop, float value) {
        A = new UAsset(path, EngineVersion.VER_UE5_6, Mappings);
        var asset = Path.GetFileNameWithoutExtension(path);
        var ex = A.Exports.OfType<NormalExport>().FirstOrDefault(e => e.ObjectName.ToString() == export)
            ?? throw new InvalidOperationException($"{asset}: no export named {export}");
        if (ex.Data.FirstOrDefault(x => x.Name.ToString() == prop) is not FloatPropertyData p)
            throw new InvalidOperationException($"{asset}: {export} has no float property {prop}");
        if (p.Value == value)
            throw new InvalidOperationException(
                $"{asset}: {export}.{prop} is already {value} - refusing to ship a no-op override");
        Console.WriteLine($"  {asset}: {export}.{prop} {p.Value} -> {value}");
        p.Value = value;
        A.Write(path);
    }

    const string WEAPON_LANDING = "WBP_Menu_Armory_WeaponLanding";

    // Which of the landing screen's three lightsaber locks to lift.
    //
    // "all" by default, measured rather than chosen. Lifting CanChangeWeaponQuery
    // alone does un-grey Change Weapon -- confirmed in game -- but that screen
    // lists weapon SPECIALIZATIONS, all eight of them, not gear kits. The saber
    // hilts are gear kits and sit behind Customize or Modify Weapon, so lifting
    // one query got the button back and still offered no hilt.
    //
    // "change" lifts only the first, and is the fallback if the other two turn
    // out to open empty screens: no CPD_WP_Type_* part exists for any melee
    // weapon, so there may be nothing authored behind them.
    static readonly string SaberArmoury =
        Environment.GetEnvironmentVariable("OPENKIT_SABER_ARMORY") ?? "all";

    /// Lifts the Armory's hard-coded lightsaber lock.
    ///
    /// WBP_Menu_Armory_WeaponLanding carries three FGameplayTagQuery defaults,
    /// every one of them authored as
    ///
    ///     ALL( ALL( BitReactor.Item.UIType ), NONE( BitReactor.Item.UIType.Lightsaber ) )
    ///
    /// and all four lightsaber gear kits carry BitReactor.Item.UIType.Lightsaber.
    /// So the game deliberately greys out Change Weapon, Customize Weapon and
    /// Modify Weapon the moment a lightsaber is equipped. In the shipped game
    /// nobody sees it: Tel-Rea is the only saber user and her hilt was never
    /// meant to be swapped. Reported the moment the module made sabers reachable.
    ///
    /// It is also why no saber gear kit could ever be offered -- the whole screen
    /// is gated off before the GearKit slot is ever enumerated -- which was
    /// wrongly recorded here as "melee has no gear-kit picker".
    ///
    /// The token stream is decoded rather than guessed, and the stock bytes are
    /// asserted before anything is written:
    ///
    ///     0        stream version
    ///     1        has-root flag
    ///     5        AllExprMatch, 2 sub-expressions
    ///       2 1 0    AllTagsMatch, 1 tag, TagDictionary[0] = BitReactor.Item.UIType
    ///       3 1 1    NoTagsMatch,  1 tag, TagDictionary[1] = ...UIType.Lightsaber
    ///
    /// The replacement drops the NONE clause and keeps the rest:
    ///
    ///     0 1 2 1 0    ALL( BitReactor.Item.UIType )
    ///
    /// which a lightsaber satisfies, because gameplay tag matching is
    /// hierarchical and UIType.Lightsaber is a child of UIType. The dictionary is
    /// left intact; an entry nothing references costs nothing and keeps the edit
    /// to one array.
    static readonly byte[] StockQueryTokens = { 0, 1, 5, 2, 2, 1, 0, 3, 1, 1 };
    static readonly byte[] LiftedQueryTokens = { 0, 1, 2, 1, 0 };
    const string LIFTED_DESCRIPTION = " ALL( BitReactor.Item.UIType )";

    static void EditWeaponLanding(string path) {
        A = new UAsset(path, EngineVersion.VER_UE5_6, Mappings);
        var cdo = A.Exports.OfType<NormalExport>()
            .First(e => e.ObjectName.ToString().StartsWith("Default__"));

        var wanted = SaberArmoury == "all"
            ? new[] { "CanChangeWeaponQuery", "CanCustomizeWeaponQuery", "CanModifyWeaponQuery" }
            : new[] { "CanChangeWeaponQuery" };

        int lifted = 0;
        foreach (var name in wanted) {
            var q = (StructPropertyData)cdo.Data.First(x => x.Name.ToString() == name);
            var stream = (ArrayPropertyData)q.Value.First(x => x.Name.ToString() == "QueryTokenStream");
            var dict = (ArrayPropertyData)q.Value.First(x => x.Name.ToString() == "TagDictionary");

            // Assert the anchor. A silently-changed query would ship a container
            // that overrides the asset and lifts nothing, which is the exact
            // failure this project has shipped twice.
            var actual = stream.Value.Cast<BytePropertyData>().Select(b => b.Value).ToArray();
            if (!actual.SequenceEqual(StockQueryTokens))
                throw new InvalidOperationException(
                    $"{name}: token stream is [{string.Join(",", actual)}], expected "
                    + $"[{string.Join(",", StockQueryTokens)}] - the query changed, refusing to guess");

            var tags = dict.Value.Cast<StructPropertyData>()
                .Select(t => ((NamePropertyData)t.Value.First(x => x.Name.ToString() == "TagName")).Value.ToString())
                .ToArray();
            if (tags.Length != 2 || tags[0] != "BitReactor.Item.UIType"
                || tags[1] != "BitReactor.Item.UIType.Lightsaber")
                throw new InvalidOperationException(
                    $"{name}: TagDictionary is [{string.Join(", ", tags)}], not the expected pair");

            var template = (BytePropertyData)stream.Value[0];
            stream.Value = LiftedQueryTokens
                .Select((b, i) => (PropertyData)new BytePropertyData(new FName(A, i.ToString())) {
                    ByteType = template.ByteType, EnumType = template.EnumType, Value = b })
                .ToArray();

            var auto = (StrPropertyData)q.Value.First(x => x.Name.ToString() == "AutoDescription");
            auto.Value = new FString(LIFTED_DESCRIPTION);
            lifted++;
        }

        if (lifted != wanted.Length)
            throw new InvalidOperationException("saber-armoury: not every query was lifted");
        Console.WriteLine($"  {WEAPON_LANDING}: {lifted} lightsaber lock(s) lifted ({string.Join(", ", wanted)})");
        A.Write(path);
    }

    static int Main(string[] a) {
        string tree = a[0], variant = a[2];
        Mappings = new Usmap(a[1]);

        string Find(string name) {
            var hit = Directory.GetFiles(tree, name + ".uasset", SearchOption.AllDirectories);
            if (hit.Length != 1) throw new FileNotFoundException($"{name}: expected one match, found {hit.Length}");
            return hit[0];
        }

        // The row refit is independent of every tag mode. It writes no tag and
        // bypasses no requirement -- it moves two floats so ten cards occupy the
        // width the game already gives eight -- and unlike the picker container
        // it is not an opt-in behaviour change: anyone running the module sees
        // the overflow, so it ships on its own.
        if (variant == "ui-fit") {
            // Checked before anything is written, so a bad retune leaves the
            // extracted tree untouched rather than half-edited.
            var used = 10 * CardWidth + 9 * CardSpacing;
            if (used > ROW_SPACE)
                throw new InvalidOperationException(
                    $"10 cards at {CardWidth} + {CardSpacing} = {used}, and the row has {ROW_SPACE}");
            var talentUsed = 12 * TalentWidth + 11 * TalentSpacing;
            if (talentUsed > ROW_SPACE)
                throw new InvalidOperationException(
                    $"12 talents at {TalentWidth} + {TalentSpacing} = {talentUsed}, and the row has {ROW_SPACE}");
            SetFloat(Find(CARDS), "Specializations", "HorizontalEntrySpacing", CardSpacing);
            SetFloat(Find(CARD), "SpecializationWidget", "WidthOverride", CardWidth);
            SetFloat(Find(TALENTS), "Talents", "HorizontalEntrySpacing", TalentSpacing);
            SetFloat(Find(TALENT_CARD), "TalentWidget", "WidthOverride", TalentWidth);
            SetFloat(Find(TALENT_CARD), "TalentWidget", "HeightOverride", TalentWidth);
            Console.WriteLine($"  ui-fit: 10 cards at {CardWidth} + {CardSpacing} = {used} of {ROW_SPACE}");
            Console.WriteLine($"  ui-fit: 12 talents at {TalentWidth} + {TalentSpacing} = {talentUsed} of {ROW_SPACE}");
            return 0;
        }

        if (variant == "saber-armoury") {
            EditWeaponLanding(Find(WEAPON_LANDING));
            return 0;
        }

        EditPicker(Find(PICKER), variant);

        var kits = variant.Replace("-swap", "");
        var parts = new List<string>();
        if (kits is "both" or "padawan") parts.AddRange(PadawanParts);
        if (kits is "both" or "warrior") parts.AddRange(WarriorParts);
        // "mapping" touches the picker and nothing else. It is the companion to
        // the native module: the module makes the kits appear, and this teaches
        // the picker which secondary each new primary pairs with. It writes no
        // tag to any part, character or faction, so it is the only mode that
        // keeps the project's rule -- the others are superseded experiments.
        //
        // "hero" leaves the parts alone: the whole point is that only the
        // character side changes, which is what the data-only route does.
        if (Mode is not "hero" and not "mapping") foreach (var part in parts) EditPart(Find(part), part, Mode == "company");
        if (Mode is "neutral" or "hero") EditFaction(Find(FACTION_ASSET));
        Console.WriteLine($"  tag mode: {Mode}");

        Console.WriteLine(Mode == "mapping"
            ? $"  {variant}: 1 picker, no parts edited"
            : $"  {variant}: 1 picker + {parts.Count} part(s)");
        return 0;
    }
}
