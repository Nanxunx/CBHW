# Turtle335Converter project checkpoint — 2026-08-20 04:30 +08

## Scope

Continue V4.6 canonical whole-M2 production validation on PR #7. Keep the PR Draft and do not merge to `main` yet.

## Hard evidence now available

### Local build / CTest

The user executed the V4.6 local evidence runner on Windows with VS2022 x64. The build completed and CTest passed 30/30.

This closes the earlier compile/test gate. GitHub hosted Actions remains a separate zero-step infrastructure/account-layer problem and is not being used as source-code evidence.

### Selected whole-M2 Golden run

Input roots:

- Build 12340 source: `E:\335_FinalExtract_V5`
- historical successful 1.12 target: `E:\335to112_Converted_FinalExtract_V1`
- `AnimationData.dbc`: `E:\335_FinalExtract_V5\DBFilesClient\AnimationData.dbc`

Evidence package:

`V46_SelectedGolden_20260820_040423.zip`

Run result at commit `9efef5a24f9b5538983bf81db0afc4c8faeb6c67`:

- selected: 33
- generated: 26
- converter fail: 7
- script errors: 0
- Light gated: 0
- animated baseline selected: 4
- ribbon-bearing selected: 13
- particle-bearing selected: 24

## What the 26 generated models proved

Semantic generated-vs-historical comparison found:

- 26/26 target MD20 v256
- all major header array counts match the historical successful target
- 26/26 Sequence metadata and canonical +3333ms timeline match
- 26/26 AnimationLookup match
- 26/26 Playable226 match for the selected corpus
- 26/26 embedded View semantics match (indices, triangles, properties, Classic32 submeshes, texture units, LOD)
- 26/26 texture definition/name semantics match
- unchanged raw arrays/lookups/bounds match
- Ribbon: 11/11 generated ribbon-bearing models match historical semantics
- Camera: 6/6 match
- TextureAnimation: 7/7 match
- Attachment: 9/9 match
- Bone tracks: 25/26 exact representation; the remaining difference is a known legal one-key ConstantNoRanges vs per-sequence-expanded encoding with the same quaternion value

Do not require whole-file byte identity. Canonical relocation/order and known legal one-key representations make byte identity the wrong oracle.

## Seven converter failures: one root cause

All seven failed with:

`WotLK timestamps payload outside selected data source`

Failed models:

1. `Creature\NETHERDRAKE\NorthrendNetherDrake.M2`
2. `Creature\Kaelthas\Kaelthas.m2`
3. `Creature\AlglontheObserver\AlgalontheObserver.M2`
4. `Creature\Kaelthas_broken\KaelThasBroken.m2`
5. `Creature\KingYmiron\KingYmiron.M2`
6. `Creature\FelElfCasterMale\FelElfCasterMale.m2`
7. `Creature\FelElfWarriorMale\FelElfWarriorMale.m2`

Real Build 12340 source evidence shows external `.anim` payloads can legally start at sidecar-relative offset 0. The old resolver rejected every non-empty offset-0 payload.

A second issue is exposed by the same models: alias sequences (`flags & 0x40`) often have no alias-specific sidecar. Their payload owner is the sequence referenced by `Sequence.Index`. In the six alias-heavy failed models, every missing alias-specific sidecar in the selected corpus resolves to an existing target-sequence sidecar through `Sequence.Index`.

Implemented fixes:

- allow offset 0 for external-sidecar payloads while keeping offset 0 invalid for non-empty main-M2 payloads
- recursively resolve alias payload ownership through `Sequence.Index`
- fail closed on alias cycles/out-of-range targets
- make Event timers use the same alias/sidecar ownership rule
- stop the CLI from reporting alias-specific sidecars as falsely missing

## Additional semantic Golden fixes found in generated output

The converter could generate these models, but paired Golden comparison found incorrect synthesized defaults/rules.

### Color alpha

Empty per-sequence Color alpha groups must synthesize `32767` (`0x7FFF`, opaque), not zero.

Observed real mismatches: 49 Color alpha tracks in the selected generated corpus.

### Transparency

Empty per-sequence Transparency groups must also synthesize `32767`, not zero.

Observed real mismatches: 6 tracks.

### Particle enabled

A fully empty Particle enabled track already used `Times=[0], Keys=[1]`, but empty groups inside a non-empty per-sequence enabled track incorrectly synthesized zero. The Golden rule is enabled=1.

Observed real mismatches in 3 selected particle models.

### Event timers

The old Event flattening rule did not match historical successful targets. Paired source/target simulation matched all 136 selected Event records with this rule:

- outer count 0: empty
- global sequence: one group raw
- one non-global group: preserve raw times and emit one `(0,count)` range when non-empty
- per-sequence empty group: `(cursor,cursor)`
- per-sequence one-time group: duplicate at `window.start + t` and `window.end + t`, range length 2
- per-sequence multi-time group: shift all by `window.start`, range length N
- append final `(0,0)` range for the per-sequence form

This rule replaces the previous Event implementation.

## Code patches applied on PR #7

Starting from `9efef5a`:

- `13ab805` — declare shared alias payload resolver
- `0c3e2f7` — external `.anim` zero-offset + alias resolution
- `02195a9` — external resolver regression tests
- `4121f0f` — opaque Color alpha / Transparency defaults
- `ca4e86b` — alpha default regression tests
- `bc44856` — Particle enabled empty-group default = 1
- `c035f99` — Particle enabled regression test
- `2ff3f47` — Event timer paired-Golden semantics + alias/zero-offset sidecar handling
- `c3547fb` — Event regression tests
- `0d83280` — CLI skips false alias-specific missing-sidecar accounting

Existing CMake already registers all modified test executables; no new CTest target is required.

## Current hard gate

The new C++ changes have not yet been compiled on the user's Windows machine. Do not claim this patch set passes Build/CTest or selected Golden yet.

Required next order:

1. `git pull`
2. rerun `Run_V46_LocalBuildCTest.ps1 -Clean`
3. if 30/30 (or complete current test count) PASS, rerun `Run_V46_SelectedGolden.ps1`
4. analyze the new ZIP semantically
5. only after selected Golden is clean, collect a tiny targeted non-zero Light paired Golden if available
6. minimal unmodified Turtle WoW 1.18.1 in-game Creature/Ribbon/Particle regression
7. production lock and only then merge PR #7

## Do not regress

- do not restart broad M2 scans
- do not return to Sword/Mace proof-of-concept work
- do not treat whole-file byte identity as the Golden criterion
- do not enable Light by default before paired evidence
- do not merge PR #7 yet
