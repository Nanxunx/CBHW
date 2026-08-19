# Turtle335Converter project checkpoint — 2026-08-19 21:41 +08

## Scope

Continue the V4.6 canonical whole-M2 assembler from PR #7 without merging `main` prematurely.

Repository state at the start of this checkpoint:

- base: `main` @ `39b1c439e0c8a4fd271633f639b77af1a22ba96e`
- branch: `fix/v46-whole-m2-build`
- PR: #7 `feat(m2): validate and expose V4.6 whole-M2 pipeline`
- previous head: `c4ff562915d1bddcd14401982b9136b51e5ba4f1`

## What was verified in this pass

### 1. GitHub Actions failure is pre-step, not a CMake/CTest result

Current `core-tests` Run #282 (`32258160090`) failed for both matrix jobs before any workflow step existed.

Attempt 1:

- Ubuntu job: failure, `steps = null`
- Windows job: failure, `steps = null`
- job-step API returned an empty step list
- job-log API returned `BlobNotFound`

The failed jobs were rerun once. Attempt 2 briefly entered `queued`, then both matrix jobs again completed with failure and no steps.

Therefore this run never reached:

1. `actions/checkout@v4`
2. CMake Configure
3. Build
4. CTest

Do **not** interpret the red Actions badge as a compiler or test failure.

Historical control: PR #6 on 2026-08-18 used the same `core-tests` workflow and the same `ubuntu-latest` / `windows-latest` matrix. Run #104 completed both platforms successfully through setup, checkout, Configure, Build and Test.

GitHub public status was operational during this pass. The strongest remaining hypothesis is an account/repository-level Actions usage, billing, budget or policy gate. The current GitHub connector does not expose billing/usage state, so this remains a hypothesis rather than a proven billing diagnosis.

### 2. CMake / CLI interface static audit

`CMakeLists.txt` still requires C++17 and registers:

- `turtle335_core`
- `turtle335_probe_m2`
- `turtle335_convert_m2`

`tools/convert_wotlk_m2.cpp` matches the current `ClassicM2Writer` / `WotlkM2Reader` API:

- input source M2
- Build 12340 `AnimationData.dbc`
- output M2
- optional reference-gated Light override
- load `<Stem>00.skin`, `<Stem>01.skin`, ... from `nViews`
- load external `<Stem><AnimID:04>-<SubID:02>.anim` for sequences that require sidecars
- call `ConvertWotlkM2ToClassic()`

The previously identified texture-relocation compile blocker remains fixed by the pointer `PutU32(std::uint8_t*, ...)` overload in `ClassicM2Writer.cpp`.

No new obvious C++17 signature mismatch was found in this static pass. This does **not** replace a real compile.

### 3. Strict validator header offsets were cross-checked against the writer

The expanded `ClassicM2Validator` offsets match `BuildClassicM2Header()` exactly:

| Block | Header offset | Stride |
|---|---:|---:|
| Color | `0x54` | 56 |
| Texture | `0x5c` | 16 |
| Transparency | `0x64` | 28 |
| TextureAnimation | `0x74` | 84 |
| TextureReplace | `0x7c` | 2 |
| RenderFlags | `0x84` | 4 |
| BoneLookup | `0x8c` | 2 |
| TextureLookup | `0x94` | 2 |
| TextureUnitLookup | `0x9c` | 2 |
| TransparencyLookup | `0xa4` | 2 |
| TextureAnimationLookup | `0xac` | 2 |
| BoundingTriangles | `0xec` | 2 |
| BoundingVertices | `0xf4` | 12 |
| BoundingNormals | `0xfc` | 12 |
| Attachment | `0x104` | 48 |
| AttachmentLookup | `0x10c` | 2 |
| Event | `0x114` | 44 |
| Light | `0x11c` | 212 |
| Camera | `0x124` | 124 |
| CameraLookup | `0x12c` | 2 |
| Ribbon | `0x134` | 220 |
| Particle | `0x13c` | 504 |

The nested Texture filename `(count, offset)` validation is also aligned with the 16-byte Texture record layout.

Result: no validator header-offset regression found in this pass.

## File-library / Golden availability check

No ready-to-consume V4.6 local Build/CTest log was found in the project file library.

No actual binary/output artifact was found for:

- `ModelPort_GoldenReference_Targeted_V44_ALL.zip`
- `V44_SelectedSamples.csv`
- `V44_Summary.json`
- Build 12340 `AnimationData.dbc`

Only the scanners/packers and earlier metadata/reference documents are currently available there.

The local Golden corpus convention remains:

- source 3.3.5a: `E:\335_FinalExtract_V5`
- successful 1.12 target: `E:\335to112_Converted_FinalExtract_V1`
- V4.4 targeted output: `E:\ModelPort_GoldenUpload_Targeted_V44`

V4.4 selected-family policy remains targeted rather than broad-scan:

- `01_RuleMismatch` — up to 8
- `02_TrueRibbon` — up to 6
- `03_ParticleOneAnimation` — up to 5
- `04_ParticleComplex` — up to 5
- `05_AliasSubAnimation` — up to 5
- plus a capped ordinary-animation regression sample

## Current hard gate

We still do **not** have a real CMake/CTest PASS for the V4.6 whole writer.

The next executable gate is the existing local runner:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\modelport\Run_V46_LocalBuildCTest.ps1 -Clean
```

That runner performs:

1. Visual Studio 2022 x64 Configure
2. Debug build
3. complete CTest with output-on-failure

Only after a real PASS should the project proceed to selected whole-output Golden regression.

## Next order after a real build/test result

1. Fix any real compile/CTest error if present.
2. If PASS, run `turtle335_convert_m2` over selected static / animated / Ribbon / Particle Golden families.
3. Compare generated canonical v256 output against the successful 1.12 corpus semantically; do not require historical one-key encodings to be byte-identical where both forms are known-valid.
4. Collect only a tiny targeted `Light > 0` paired Golden if available; no full-library Light scan.
5. Perform minimal Turtle WoW 1.18.1 Ribbon + Particle/Creature in-game regression.
6. Lock M2 production only after those gates pass.
7. Then continue WMO -> ADT/WDT/WDL -> DBC -> MPQ integration.

## Do not regress to old work

- Do not restart ordinary Sword/Mace proof-of-concept testing.
- Do not perform another broad M2 full-library deep scan.
- Do not claim Actions Run #282 is a C++ or CTest failure.
- Do not enable Light-bearing production output by default before paired Golden evidence.
- Do not merge PR #7 into `main` until the real validation gates above pass.
