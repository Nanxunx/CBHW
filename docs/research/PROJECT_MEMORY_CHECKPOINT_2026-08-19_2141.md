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

No new obvious C++17 signature mismatch was found in this static pass.

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

## Real local build / CTest result — PASS

On 2026-08-20 03:31 +08 the user ran the hardened local evidence runner from the real Windows machine on branch `fix/v46-whole-m2-build` at commit `57649fe0eb5ed10edee39020140616489d9ecc4a`.

Actual selected environment:

- Visual Studio 2022 Build Tools: `<VS_BUILD_TOOLS_ROOT>`
- Windows SDK: `10.0.26100.0`
- MSVC: `19.44.35228.0`
- compiler: `D:/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`
- CMake: `3.29.9`
- Debug / x64

Configure completed successfully. `turtle335_core`, `turtle335_probe_m2.exe`, `turtle335_convert_m2.exe`, and every registered test executable linked successfully.

The compiler emitted existing `C4244` narrowing warnings in ADT/Particle code, but no build error.

Complete CTest result:

```text
100% tests passed, 0 tests failed out of 30
Total Test time (real) = 3.93 sec
```

The local runner reported:

```text
V4.6 validation status: PASS
PASS: real VS2022 build + complete CTest finished. Next gate is selected Golden regression.
```

Evidence paths:

```text
<REPOSITORY_ROOT>\validation-v46\V46_LocalValidation_20260820_033149\VALIDATION_SUMMARY.txt
<REPOSITORY_ROOT>\validation-v46\V46_LocalValidation_20260820_033149.zip
<REPOSITORY_ROOT>\build-v46\Debug\turtle335_convert_m2.exe
<REPOSITORY_ROOT>\build-v46\Debug\turtle335_probe_m2.exe
```

**The build/CTest hard gate is therefore CLOSED and PASS.**

## Golden corpus convention

The local Golden corpus convention remains:

- source 3.3.5a: `<BUILD12340_SOURCE_ROOT>`
- successful 1.12 target: `<CLASSIC_GOLDEN_ROOT>`
- V4.4 targeted output: `<V44_GOLDEN_ROOT>`

V4.4 selected-family policy remains targeted rather than broad-scan:

- `01_RuleMismatch` — up to 8
- `02_TrueRibbon` — up to 6
- `03_ParticleOneAnimation` — up to 5
- `04_ParticleComplex` — up to 5
- `05_AliasSubAnimation` — up to 5
- plus a capped ordinary-animation regression sample

A new V4.6 collection runner now exists:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\modelport\Run_V46_SelectedGolden.ps1
```

It reuses the V4.4 targeted selector only when its metadata is missing, adds small static/ordinary-animation baselines, runs the actual `turtle335_convert_m2.exe`, and packages source M2 + skin/anim sidecars, generated canonical v256, historical successful 1.12 target, logs, hashes, and metadata for semantic comparison.

## Current hard gate

The current hard gate is now **selected real-model whole-output Golden regression**, not compilation.

The next executable step is:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\modelport\Run_V46_SelectedGolden.ps1
```

Expected families:

1. static baseline
2. ordinary animated baseline
3. true Ribbon
4. Particle one-animation
5. Particle complex
6. alias/subanimation/high-risk animation

Light-bearing models remain intentionally reference-gated and should be reported/skipped rather than force-enabled.

## Next order

1. Run `Run_V46_SelectedGolden.ps1` and inspect/package the real generated outputs.
2. Compare generated canonical v256 against the successful historical 1.12 corpus semantically; do not require historical one-key encodings to be byte-identical where both forms are known-valid.
3. Treat historical V4 Playable bytes as informational only; V4.6 production uses Build12340 `AnimationData.dbc` fallback graph plus canonical overrides.
4. Collect only a tiny targeted `Light > 0` paired Golden if available; no full-library Light scan.
5. Perform minimal Turtle WoW 1.18.1 Ribbon + Particle/Creature in-game regression.
6. Lock M2 production only after those gates pass.
7. Then continue WMO -> ADT/WDT/WDL -> DBC -> MPQ integration.

## Do not regress to old work

- Do not restart ordinary Sword/Mace proof-of-concept testing.
- Do not perform another broad M2 full-library deep scan.
- Do not treat the current GitHub Actions zero-step failures as C++/CTest evidence.
- Do not enable Light-bearing production output by default before paired Golden evidence.
- Do not merge PR #7 into `main` until the Golden and in-game gates pass.
