# Turtle335Converter project checkpoint — 2026-08-20 03:35 +08

## Real local validation gate is now PASS

User executed the repository's V4.6 local evidence runner on the actual Windows development machine:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\modelport\Run_V46_LocalBuildCTest.ps1 -Clean
```

Validated branch before the run:

- branch: `fix/v46-whole-m2-build`
- commit: `57649fe0eb5ed10edee39020140616489d9ecc4a`
- commit subject: `tools(m2): package local V4.6 validation evidence`

Actual toolchain selected by CMake:

- Visual Studio 2022 Build Tools installed at `<VS_BUILD_TOOLS_ROOT>`
- Windows SDK: `10.0.26100.0`
- target OS reported by CMake: Windows 10.0.19045
- MSVC: `19.44.35228.0`
- compiler path: `D:/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`
- CMake: `3.29.9`
- generator: Visual Studio 17 2022 / x64
- configuration: Debug

Configure completed successfully. The full `turtle335_core` library and all registered tools/tests compiled, including:

- `turtle335_probe_m2.exe`
- `turtle335_convert_m2.exe`
- `turtle335_classic_m2_validator_tests.exe`
- `turtle335_classic_m2_writer_tests.exe`
- Ribbon / Particle / Light / Camera / Event / Bone / Auxiliary track tests

Compiler emitted several `C4244` narrowing warnings in existing ADT/Particle code paths, but there were no build errors. These warnings did not prevent linking and are not a V4.6 whole-writer gate failure.

## CTest result

Complete CTest result on the real Windows build:

```text
100% tests passed, 0 tests failed out of 30
Total Test time (real) = 3.93 sec
```

All 30 registered tests passed, including the V4.6-critical M2 tests:

- M2 core
- external anim
- quaternion codec
- bone writer
- auxiliary tracks
- camera writer
- event writer
- light writer
- Classic M2 header
- ribbon writer
- particle writer
- skin/view writer
- strict Classic M2 validator
- canonical Classic M2 whole writer

The local runner reported:

```text
V4.6 validation status: PASS
PASS: real VS2022 build + complete CTest finished. Next gate is selected Golden regression.
```

Local evidence paths from that run:

```text
<REPOSITORY_ROOT>\validation-v46\V46_LocalValidation_20260820_033149\VALIDATION_SUMMARY.txt
<REPOSITORY_ROOT>\validation-v46\V46_LocalValidation_20260820_033149.zip
<REPOSITORY_ROOT>\build-v46\Debug\turtle335_convert_m2.exe
<REPOSITORY_ROOT>\build-v46\Debug\turtle335_probe_m2.exe
```

## Consequence

The previous hard gate is closed. Do not continue treating the GitHub Actions zero-step failures as a substitute for compiler/test evidence. We now have a real local MSVC build and a complete 30/30 CTest PASS.

The project must now move to real-model whole-output Golden regression.

## Exact next order

1. Use the existing V4.4 targeted selector to obtain a small real-model corpus from the established paired roots:
   - WotLK source: `<BUILD12340_SOURCE_ROOT>`
   - successful 1.12 target: `<CLASSIC_GOLDEN_ROOT>`
2. Exercise the current `turtle335_convert_m2.exe` on selected:
   - static baseline
   - ordinary animated baseline
   - true Ribbon
   - Particle one-animation
   - Particle complex
   - alias/subanimation/high-risk animation
3. Compare generated canonical v256 against the successful historical 1.12 corpus semantically. Byte-identical output is not required for known-valid alternative legacy one-key encodings.
4. Treat the historical V4 hard-coded Playable table as informational only; V4.6 production uses Build12340 `AnimationData.dbc` fallback graph plus canonical overrides.
5. Light-bearing models remain reference-gated. Collect only a tiny `Light > 0` paired Golden after the selected non-Light families.
6. After Golden regression passes, perform minimal Turtle WoW 1.18.1 Ribbon + Particle/Creature in-game regression.
7. Only then lock M2 production and continue WMO -> ADT/WDT/WDL -> DBC -> MPQ integration.

## Do not regress

- Do not return to Sword/Mace proof-of-concept testing.
- Do not repeat a broad all-model deep scan.
- Do not merge PR #7 to `main` yet.
- Do not enable Light production output by default before paired Golden evidence.
