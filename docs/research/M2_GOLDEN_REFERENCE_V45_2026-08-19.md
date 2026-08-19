# M2 Golden Reference V4.5 — 2026-08-19

## Goal

Continue turning `NansenCore/Turtle335Converter` from reverse-engineering notes into a real 3.3.5a build12340 -> Vanilla/Turtle 1.18.1 conversion tool. Output remains standard `MD20 v256`; historical LKBC/Coffee code is algorithm evidence only and no Orange/private wrapper is allowed.

## V4.4 focused scan result

```text
source_m2_total              18083
paired_m2                    11596
deep_total                    5036
deep_errors                       0
sequence_mismatch                29
timeline_mismatch                27
animation_lookup_mismatch         0
playable_v4_mismatch            498
true_ribbon_models              268
particle_models                5570
texanim_models                  909
external_anim_models            395
alias_models                    934
subanimation_models             822
duplicate_animid_models         822
```

`AnimationLookup` is now a production baseline; the old hard-coded Playable V4 graph was overfit.

## AnimationLookup — production baseline

All 5036 high-risk deep pairs passed:

```text
count = max(AnimationID) + 1
missing = 0xFFFF
same AnimationID:
    prefer first SubAnimationID == 0
    otherwise first physical sequence
```

`Sequence.Index` is a different field and must preserve the source value. It must not be used as the AnimationLookup physical position.

## PlayableAnimationLookup V4.5

Successful legacy targets are primarily reproduced by **build12340 `AnimationData.dbc` raw field index 5**, followed recursively until the current model actually contains an AnimationID.

Example:

```text
170 -> 19 -> 18 -> 17 -> 16
```

A model containing 19 resolves 170 to 19; a model lacking 19/18 but containing 17 resolves to 17; a model containing only 16 resolves to 16.

Six Golden corrections are required over raw build12340 field 5:

```text
121 -> 14
146 -> 0
172 -> 16
174 -> 16
181 -> 19
191 -> 159
```

`146 -> 0` also agrees with the historical LKBC converter's explicit anti-loop override.

With `field5 + six overrides + model-aware recursion`, all **29 packaged V4.4 selected models / 6554 Playable records match their successful 1.12 targets exactly**.

Status: **V4.5 provisional** until the focused local refinement rechecks all 498 old V4 failures.

## Ribbon writer — GOLDEN_READY offline semantic

True RibbonEmitter Golden models are available and the writer was compared against all ribbon-bearing selected pairs:

```text
13 models
448 emitters
448/448 semantic PASS
```

Structure:

```text
WotLK ribbon record 176 B
 -> Classic ribbon record 220 B
```

Preserved/converted:

- base unknown/bone/position
- textureRefs and blendRefs uint16 payloads
- color Vec3 track 20 -> 28
- opacity uint16 track 20 -> 28
- heightAbove float track 20 -> 28
- heightBelow float track 20 -> 28
- texSlot uint16 track 20 -> 28
- enabled uint8 track 20 -> 28
- scalar tail

All six tracks use the validated generic legacy Range/Times/Keys flattening rules. WotLK trailing ribbon `unknown1` is dropped by the successful legacy target.

Status: **GOLDEN_READY_OFFLINE_SEMANTIC**. Full-M2 integration plus one real-client Ribbon regression remains required.

## Particle writer — GOLDEN_READY offline semantic with one blocker

A real particle semantic writer is implemented against:

```text
24 selected models
930 emitters
```

Structure:

```text
WotLK particle record 476 B
 -> Classic particle record 504 B
```

### Base mapping

Across 930/930:

- emitter count preserved
- target flags = source flags & 0xFFFF
- position/bone/texture/blend/particle-type/head-tail/tile layout follows Golden bytes
- target byte `+0x29 = 0`
- target uint16 `+0x2A = source byte +0x29`
- model/child filename payloads preserved and relocated

### Ten main float tracks

All ten source 20-byte WotLK tracks convert to 28-byte Classic tracks using the generic Range/Times/Keys algorithm. Semantic comparison passed 930/930 emitters.

### Fake gradient -> Classic fixed fields

Color exactly three keys:

```text
midPoint = color.time[1] / 32767.0f
legacy bytes = B,G,R,A for each of the three keys
RGB = truncated float -> byte
A = source opacity int16 >> 7, clamped to byte
```

If color key count is not three, successful target writes zero midpoint/colors.

Size exactly three keys:

```text
legacy size[0..2] = source Vec2.X of first three size keys
```

Otherwise all three legacy sizes are zero.

Tile mapping is exact across 930 emitters:

```text
[h0, h1, 1, h2, h3, 1, t0, t1, t2, t3]
```

### Enabled track

Enabled uses legacy flattening with default key **1**. If source outer track is empty, successful target synthesizes:

```text
Ranges = 0
Times  = [0]
Keys   = [1]
```

### Blocker / explicit losses

No selected Golden emitter has non-zero `nUnknownReference/ofsUnknownReference`; the writer refuses such an emitter until a Golden case exists.

Successful legacy targets visibly discard WotLK-only semantics. The converter records explicit losses rather than hiding them:

- source ParticleColorIndex
- WotLK unknown1
- WotLK unknown2 if non-zero
- WotLK scale-vary/unknownfields
- WotLK unknown3
- WotLK unknown4

Selected Golden loss occurrence counts:

```text
ParticleColorIndex   5 emitters
unknown1            47
scale-vary          58
unknown3            92
unknown4            92
```

Status: **GOLDEN_READY_OFFLINE_SEMANTIC_WITH_BLOCKER**. Full-M2 integration and real-client test remain.

## Real C++ M2 core started

The repository previously had executable C++ conversion code for ADT/DBC but no `src/m2` implementation. V4.5 adds the first production C++ M2 core:

```text
include/turtle335/m2/WotlkM2Reader.h
src/m2/WotlkM2Reader.cpp
include/turtle335/m2/AnimationMetadata.h
src/m2/AnimationMetadata.cpp
tools/probe_wotlk_m2.cpp
tests/test_m2_core.cpp
```

Implemented C++ behavior:

- strict MD20 v264 header parser
- sequence/ribbon/particle record-array bounds validation
- feature classification
- source alias/SubAnimation/duplicate-ID detection
- Classic +3333 sequence window builder
- 68-byte Classic sequence record builder preserving source Index/tail metadata
- production AnimationLookup generator
- build12340 AnimationData WDBC parser
- Playable V4.5 model-aware generator
- `turtle335_probe_m2` command-line probe

The new C++ core/test compiles locally with `-std=c++17 -Wall -Wextra -Wpedantic -Werror` and the M2 core unit test passes. The probe was also run against a real selected Ribbon Golden source and correctly reported 98 ribbons / 10 particles and the Ribbon writer gate.

## Next gates

1. Run the targeted V4.5 local refinement: only the previous 498 Playable failures are rechecked; do not rescan 18k M2s.
2. Package all 29 Sequence/Timeline exceptions for exact Golden diff.
3. Integrate Python-proven Ribbon and Particle algorithms into the C++ M2 builder.
4. Implement the complete v264+skin -> v256 embedded-View writer around the proven components.
5. Validate one Ribbon and one Particle model in the real Turtle 1.18.1 client, then unlock batch conversion by feature class.
