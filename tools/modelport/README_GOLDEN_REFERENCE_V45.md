# ModelPort Golden Reference V4.5

V4.5 is the first checkpoint where the M2 work starts moving from research-only Python into the repository's production C++ core.

## Production baseline now

- MD20 v264 strict reader / feature probe
- Sequence.Index = preserve source
- AnimationLookup: maxID+1, missing FFFF, duplicate ID prefers SubID0
- embedded View rule from external `.skin`
- generic legacy Range/Times/Keys
- quaternion short -1 -> exact +1.0

## Playable V4.5

Do not use the old V4 hard-coded 170->16 family.

Primary graph:

```text
build12340 AnimationData.dbc raw field index 5
+ model-aware recursive fallback
+ Golden overrides:
121->14, 146->0, 172->16, 174->16, 181->19, 191->159
```

Selected V4.4 Golden: 29 models / 6554 records exact. Run `Run_ModelPort_Refine_V45.ps1` to re-check only the previous 498 V4 failures.

## Ribbon

`ribbon_v1.py` is Golden-ready offline semantic:

```text
13 models / 448 emitters exact
176B WotLK -> 220B Classic
```

## Particle

`particle_v1.py` is Golden-ready offline semantic with one deliberate blocker:

```text
24 models / 930 emitters exact
476B WotLK -> 504B Classic
```

Non-zero `nUnknownReference/ofsUnknownReference` is blocked until a successful Golden target exercising it is available. WotLK-only dropped fields are reported as explicit losses.

## C++ M2 core

The first actual M2 C++ implementation is under:

```text
include/turtle335/m2/
src/m2/
```

Use:

```text
turtle335_probe_m2 <source.m2>
```

The next implementation stage is the complete v264 + external skin -> v256 target builder using these proven blocks.
