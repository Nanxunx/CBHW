# ADT Implementation Checkpoint — 2026-08-18

Status: **first guarded source-file -> semantic model -> Turtle/Vanilla ADT CLI is implemented; newest code still requires external build/real-client fixture verification.**

## Implemented source-to-target chain

```text
build12340 .wdt (optional)
  -> WotlkWdtReader
  -> source MPHD big-alpha hint

build12340 LiquidType.dbc
  -> strict WDBC reader
  -> ID -> SoundBank -> Water/Ocean/Magma/Slime resolver

build12340 .adt
  -> WotlkAdtReader
  -> WotlkTerrainNormalizer
  -> WotlkMcshNormalizer
  -> Mh2oReader
  -> WotlkAdtNormalizer
  -> NormalizedAdt
  -> TerrainWriter / LegacyLiquid / McnkWriter / PlacementWriter
  -> AdtWriter
  -> AdtValidator
  -> target .adt
```

Executable:

```text
turtle335_convert_adt <source.adt> <LiquidType.dbc> <output.adt>
    [--wdt <source.wdt>]
    [--allow-lossy]
```

Policy:

- blockers always abort publication;
- losses abort unless `--allow-lossy` is explicitly supplied;
- output is validated before an atomic new-file write;
- source/output overwrite is refused.

## MCSH now preserved

Noggit build12340 source behavior is explicit:

- MCSH payload = 64 rows * uint64 = 512 bytes;
- bit N of a row is shadow column N;
- when MCNK bit15 is clear, column63 is copied from62 and row63 from62 before use;
- an all-zero shadow map is omitted on write.

`WotlkMcshNormalizer` now materializes those source edge semantics and emits a complete target HSCM chunk. The NormalizedAdt target path sets MCNK bit15 deliberately, so Turtle consumes the preserved full edge rather than synthesizing it again.

Classification: **Noggit source-confirmed + Turtle bit15 binary-confirmed.**

## MCNR 13-byte boundary confirmed

Noggit writer emits:

```text
MCNR chunk header
MCNR payload = 145 * 3 = 435 bytes
13 extra blizzlike bytes outside MCNR chunk
next MCLY chunk
```

Therefore the WotLK terrain normalizer's strict 435-byte MCNR payload requirement is correct. The target McnkWriter likewise appends the 13 legacy bytes after the raw RNCM chunk.

Classification: **Noggit source-confirmed.**

## MCCV production policy tightened

Turtle real-client MCNK pointer-fixup around `0x6AF970` processes the source/target offsets for:

```text
MCVT
MCNR
MCLY
MCRF
MCAL
MCSH
MCSE
MCLQ
```

It does **not** establish an equivalent runtime pointer from the MCNK `offsMCCV` field at header payload `+0x74`.

Penqle/tortoise-wow contains `offsMCCV` only in structural headers; no independent target-client behavior was found from that source.

Therefore:

- WotLK MCCV remains a `Loss` in `WotlkAdtNormalizer`;
- `NormalizedAdtCell` no longer exposes `targetMccv` as a verified production target feature;
- raw low-level MCCV support must not be interpreted as client compatibility.

Classification: **Turtle client binary evidence against assuming direct support.**

## LiquidType.dbc no longer requires manual mapping

TrinityCore 3.3.5 `ReadLiquidTypeTableDBC()` reads:

```text
field 0 -> LiquidType ID
field 3 -> SoundBank
```

Trinity build12340 enum:

```text
SoundBank 0 -> Water
SoundBank 1 -> Ocean
SoundBank 2 -> Magma
SoundBank 3 -> Slime
```

`LiquidTypeDbc` implements a strict WDBC parser for exactly this dependency and supplies a resolver directly to MH2O normalization.

## Source WDT reader added

`WotlkWdtReader` parses:

```text
MVER 18
MPHD 8 * uint32
MAIN 64 * 64 * 8 bytes
```

Additional root chunks are safely skipped. Source MPHD bit `0x4` is exposed as the big-alpha hint; it is not propagated into the target terrain WDT. Global-WMO WDTs are currently outside the terrain-only conversion profile and are rejected.

## Raw-file regression added

`test_raw_wotlk_pipeline.cpp` creates a complete ADT byte stream, inserts a real raw `O2HM` top-level chunk, patches the MHDR MH2O pointer, shifts all 256 MCIN absolute offsets, then executes:

```text
ParseWotlkAdt
 -> NormalizeWotlkAdt
 -> SerializeNormalizedAdt
 -> ValidateVanillaAdt
 -> ParseWotlkAdt(target)
```

This closes the earlier test gap where MH2O had only been injected at the already-parsed object layer.

## Current intentional losses / blockers

Losses:

- WotLK MCCV
- WotLK MCSE until a verified cross-version emitter semantic pass is added
- MFBO until target policy is proven
- nonzero source `+0x3E`
- WotLK disable-doodads/detail bytes
- unverified final MCNK dwords
- unverified source MCNK flag bits

Blockers:

- high-resolution holes
- invalid MCRF references
- unknown/missing LiquidType mapping for MH2O
- source liquid only in legacy MCLQ without semantic source importer
- unsupported/mixed MCAL encodings
- source WDT global-WMO mode in the terrain-only pipeline

## Verification status

Do **not** describe the newest source Reader/Normalizer/CLI changes as CI-passed yet.

The connected GitHub App currently exposes workflow read/re-run tools but no action to dispatch a new run. App-origin commits are not producing a visible new check-run through the available connector, and the active container cannot download the private repository archive through the GitHub connector endpoint. Latest code therefore has committed tests and static review, but requires external compilation / CI / real-client fixture verification.

## Next highest-value step

Do not add more speculative subchunk conversions before a real build12340 fixture.

P0:

1. take one real WoW 3.3.5a ADT + its WDT + LiquidType.dbc;
2. run the new structural parser and inventory every Loss/Blocker actually present;
3. convert the simplest real tile;
4. reopen with the converter validator and Noggit;
5. load under Turtle WoW 1.18.1 local-files test client;
6. compare terrain seams, texture alpha, shadow edges, holes, placements and liquids.

Only after that fixture passes should the ADT branch add MCSE/MFBO handling or move the main implementation effort to M2 v264 -> v256.
