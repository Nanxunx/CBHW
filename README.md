# Turtle335Converter

Convert WoW 3.3.5a assets to Vanilla 1.12.x / Turtle WoW 1.18.1 compatible resources.

## Current implementation status

The ADT branch now has a first guarded source-to-target pipeline:

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
- MCSH 64x64 shadow edge normalization
- WotLK WDT source parsing for MAIN presence and the MPHD big-alpha hint
- strict build12340 `LiquidType.dbc` WDBC parsing for liquid category resolution

The converter deliberately reports unsupported source semantics as `LOSS` or `BLOCKER` rather than silently copying bytes across versions.

### Inspect a source ADT without writing anything

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

**Verification note:** the newest source Reader/Normalizer/CLI changes have committed regression tests, but the current GitHub App session cannot dispatch a fresh workflow run. Do not treat the newest main revision as CI-verified until it is compiled externally / by a new Actions run and exercised against a real build12340 ADT fixture.

## Research checkpoint

The reverse-engineering and conversion design is documented under `docs/research/`:

- [Research Checkpoint](docs/research/RESEARCH_CHECKPOINT.md) — project-wide conclusions, evidence levels, architecture and current status.
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
