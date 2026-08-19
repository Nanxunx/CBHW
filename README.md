# Turtle335Converter

Convert WoW 3.3.5a assets to Vanilla 1.12.x / Turtle WoW 1.18.1 compatible resources.

## Current implementation status

The ADT branch now has a guarded source-to-target pipeline:

```text
WoW 3.3.5a build12340 ADT
  + LiquidType.dbc
  + optional source WDT
        |
        v
WotLK structural readers
        |
        v
semantic normalization
        |
        v
Vanilla/Turtle ADT writer
        |
        v
self validator
```

Implemented ADT areas currently include:

- MVER / MHDR / MCIN / 256 MCNK root reconstruction
- MTEX, MMDX/MMID, MWMO/MWID, MDDF/MODF and MCRF
- MCVT / MCNR / MCLY terrain normalization
- old 4-bit, big 8-bit and RLE MCAL input normalization
- MH2O -> category-grouped legacy MCLQ
- legacy MCLQ owning-size handling (`QLCM` inner size may be zero; `MCNK.sizeMCLQ` owns the real block size)
- MCSH 64x64 shadow edge normalization
- WotLK WDT source parsing for MAIN presence and the MPHD big-alpha hint
- strict build12340 `LiquidType.dbc` WDBC parsing for liquid category resolution
- source-only single-ADT and batch map probes
- heap-backed 256-cell normalized/writer storage to avoid Windows default-stack failures

The converter deliberately reports unsupported source semantics as `LOSS` or `BLOCKER` rather than silently copying bytes across versions.

### Source-only single ADT probe

```text
turtle335_probe_adt <source.adt> [--wdt <source.wdt>]
```

This does not require `LiquidType.dbc` and does not write target data. It inventories MCAL encoding, MH2O liquid IDs, MCSH/MCCV/MCSE/MFBO, holes, placement-reference risks and other source semantics.

Exit intent:

```text
0 = clean
2 = blocker exists
3 = risk/loss exists without blocker
```

### Batch map probe

```text
turtle335_probe_map <adt-directory> [--recursive] [--wdt <source.wdt>] [--details]
```

The batch probe scans all `.adt` files in a directory, classifies each tile as clean/risk/blocker/parse-failure, aggregates issue codes and MH2O LiquidType IDs, and surfaces clean candidates for the first real conversion fixture. `--details` expands per-tile output.

Exit intent remains:

```text
0 = all scanned tiles clean
2 = at least one blocker or parse failure
3 = no blocker, but at least one risk/loss
```

### Full semantic scan without writing

```text
turtle335_convert_adt \
  <source.adt> \
  <LiquidType.dbc> \
  --wdt <source.wdt> \
  --scan-only
```

Exit intent:

```text
0 = scan is representable without reported loss
2 = blocker exists
3 = representable, but conversion is lossy
```

### Convert

```text
turtle335_convert_adt \
  <source.adt> \
  <LiquidType.dbc> \
  <output.adt> \
  --wdt <source.wdt>
```

Lossy output is refused by default. After reviewing every reported loss:

```text
--allow-lossy
```

can be supplied explicitly.

## Verification status

Current ADT/probe main code is cross-platform CI verified.

Latest batch-probe validation:

```text
workflow: core-tests
run id: 32095952833
run number: 104
validated main code SHA: 771cf518d4a49bf88c07ef752721f918a65eb14d
```

Results:

```text
Ubuntu: Configure PASS / Build PASS / CTest PASS (16/16)
Windows: Configure PASS / Build PASS / CTest PASS (16/16)
```

The remaining P0 gate is not compilation: it is a real unmodified WoW 3.3.5a build12340 ADT/WDT fixture and Turtle client runtime validation.

## V4.6 whole-M2 validation

The V4.6 whole-M2 branch has now completed a real Windows VS2022 x64 Debug build and complete local CTest run:

```text
30/30 tests passed
```

The current M2 gate is selected real-model whole-output Golden regression, not compilation.

After building the branch locally, update the checkout and run from the repository root:

```powershell
git pull
powershell -ExecutionPolicy Bypass -File .\tools\modelport\Run_V46_SelectedGolden.ps1
```

Default paired corpus roots used by the current research workflow are:

```text
E:\335_FinalExtract_V5
E:\335to112_Converted_FinalExtract_V1
```

The runner reuses the V4.4 targeted selector when needed, adds small static and ordinary-animation baselines, runs the actual `turtle335_convert_m2.exe`, and packages source M2/skin/anim sidecars, generated canonical v256 output, historical successful 1.12 targets, logs and hashes for semantic comparison. Native conversion failures are collected per sample instead of aborting the entire evidence run; Light-bearing models remain intentionally reference-gated.

Latest detailed checkpoint: `docs/research/PROJECT_MEMORY_CHECKPOINT_2026-08-20_0335.md`.

## Research checkpoint

The reverse-engineering and conversion design is documented under `docs/research/`:

- [Current Project Memory](docs/research/PROJECT_MEMORY_CURRENT.md) — authoritative baseline; prefer this when older research notes conflict.
- `PROJECT_MEMORY_CHECKPOINT_2026-08-18_1358.md` — newest delta after batch-probe/CI work; takes precedence for the items it updates.
- [Research Checkpoint](docs/research/RESEARCH_CHECKPOINT.md) — project-wide historical conclusions, evidence levels and architecture.
- [M2 Retroport Spec](docs/research/M2_RETROPORT_SPEC.md) — MD20/M2 structure conversion, skins, animations, particles and validation.
- [WMO Retroport Spec](docs/research/WMO_RETROPORT_SPEC.md) — root/group WMO conversion, MOMT, MOPY, second UV/color sets and MLIQ.
- [ADT Retroport Spec](docs/research/ADT_RETROPORT_SPEC.md) — ADT/MCNK conversion, MH2O -> MCLQ, resource tables and placements.
- [ADT Implementation Checkpoint](docs/research/ADT_IMPLEMENTATION_CHECKPOINT_2026-08-18.md) — current executable ADT pipeline, limitations and next fixture-validation step.

## Target pipeline

```text
WoW 3.3.5a assets
        |
        v
Turtle335Converter
        |
        v
Vanilla/Turtle-compatible M2 / WMO / ADT / BLP
        |
        v
Tortoise extractor
        |
        v
maps / vmaps / mmaps
        |
        v
Turtle WoW 1.18.1
```

The target client loader and target Tortoise extractor are treated as the final compatibility authority. Community documentation is used as supporting evidence, not as a substitute for loader-level verification.
