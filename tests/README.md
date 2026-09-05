# Tests

There is no automated test suite yet. What verification exists is built into the
tools, and is worth listing here because each check exists in response to a
specific failure that shipped.

## Enforced by the build

- **Container override, not self-consistency.** `scripts/build-container.sh`
  asserts the packed package path equals the path the asset was extracted from,
  then mounts the container against the full shipped set and confirms the packed asset
  is the one that comes back. The first containers built here packed to a flat
  path, overrode nothing, and still read back perfectly when re-extracted alone.
- **No byte-identical override.** A container that ships an unmodified asset
  still claims it and collides with unrelated mods. The builder refuses.
- **Anchored edits.** Any scripted string replacement asserts its anchor before
  writing. Two silent no-op replacements shipped in one day without this.
- **Query token streams.** The saber-armoury builder asserts the stock ten bytes
  and the exact tag pair before rewriting, refusing rather than shipping an
  override that lifts nothing.
- **Row fit.** The `ui-fit` builder refuses a width and spacing combination that
  still overflows the frame.
- **The two archives agree.** `scripts/build-manual.py` resolves the installer's
  own `ModuleConfig.xml` against the same `dist/fomod` stage rather than
  restating the layout, so the FOMOD and manual archives cannot ship different
  payloads or install to different places. It refuses a resolved layout with no
  module, no `enabled.txt`, no options, or any destination outside
  `SWZeroCompany\`, and re-checks every shipped option name against the built
  DLL.
- **FOMOD portability.** `scripts/check-fomod.py` enforces the element subset all
  three managers implement, catches flags that are set but never read and the
  reverse, and verifies every source path exists in the staged archive. It runs
  in the release build and blocks it.

## Enforced by hand, and worth automating

- **Tag discipline.** No packed asset may add a
  `br.Customization.Part.Character.Info.Name.*` tag to any character, faction or
  roster definition. This is the project's one rule and it should be a test
  rather than a matter of care. The module reports
  `identity_tags_written=0 character_edits=0` at runtime, which is the claim; a
  test should check the containers make it too.
- **Built binary matches the source.** Every part id, switch name, new log field
  and new tag is currently grepped out of the built DLL before install, and the
  installed hash checked against the built one. A stale DLL has cost a test
  cycle.

## In-game checks that no test can replace

The log is the interface. Counts are fields rather than literals so a wrong one
is visible in a single line, and rows log when their answer changes rather than
once ever. Reading `READY`, then `offered` / `declined` / `pick_asked`, is how
every behavioural finding in the changelog was reached.
