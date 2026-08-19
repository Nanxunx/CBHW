# M2 Golden V4.5 — Physical Writer Correction / Refinement

Date: 2026-08-19

## Scope

This addendum continues the V4.5 work after the focused V4.4 corpus scan. It does not claim WMO/map/full-client completion. The immediate goal remains a production M2 v264 -> v256 converter, followed later by WMO/map/DBC/MPQ integration.

Evidence order remains:

1. successful target binary / real Turtle client;
2. successful 1.12 Golden model pairs;
3. current converter tests;
4. historical tools/documents as cross-check only.

## Playable V4.5

The repository V4.5 generator remains based on build12340 `AnimationData.dbc` raw field index 5 plus the small Golden override set. Do not revert to the older hard-coded V4 170-series graph.

The refinement pass has been upgraded so it reopens only the V4.4 failures and emits:

- exact requested Playable ID;
- V4.5 predicted ID/flags;
- Golden actual ID/flags;
- model present AnimationID set;
- exact Sequence fields that differ;
- expected vs Golden timeline start/end.

No 18k-model rescan is required.

## Particle physical record correction

A direct byte-level recheck was performed against the 10 V4.4 selected Particle Golden pairs (772 emitters). This found that the earlier `particle_v1.py` semantic layout must **not** be used as the final physical writer for the post-gradient tail.

Actual successful v256 physical offsets are:

```text
332 midpoint float
336 BGRA[3] = 12 bytes
348 size[3] float = 12 bytes
360 10 x uint16 cell/tile fields = 20 bytes
380 unk Vec3
392 scales Vec3
404 slowdown float
408 rotation float
412 target unknown4 Vec2 = zeroed
420 Rot1 Vec3
432 Rot2 Vec3
444 Trans X,Y only (source Z dropped)
452 f2[4]
468 nUnknownReference
472 ofsUnknownReference
476 enabled Classic AnimationBlock (28 B)
504 record end
```

The older physical placement `Rot1@416 / Rot2@428 / Trans@440` is superseded by this direct Golden byte evidence.

Additional exact behavior:

- source `unknown3 Vec2 @376` is dropped;
- target `unknown4 Vec2 @412` is zero;
- source `Rot2.z == -0.0` is normalized by the successful target to `+0.0`;
- source `Trans.z` is dropped; only X/Y survive;
- selected Golden set has `nUnknownReference == 0`; non-zero remains a hard blocker.

A new `particle_v2.py` physical block writer is added with these offsets. On the 10 selected V4.4 pairs it reproduced all **772/772** ParticleEmitter fixed fields + filenames + 10 float tracks + enabled track byte/semantic output.

## Particle head/tail cell rule now frozen for selected Golden

Direct test across 772 emitters confirms:

```text
head = first four WotLK head-cell keys, zero padded
 tail = first four WotLK tail-cell keys, zero padded

target 10 shorts =
[h0, h1, 1, h2, h3, 1, t0, t1, t2, t3]
```

This passed **772/772**, including source head key counts 0, 2, 4, 24, 42, and 62.

## Ribbon status

The existing V4.5 Ribbon rule remains Golden-ready offline. A shared binary legacy-track codec is added for future C++ parity and effect-writer integration.

## Next gate

1. Run the upgraded V4.5 targeted refinement over only old Playable/Sequence failures.
2. Freeze any remaining Playable edges and the rare Sequence/Timeline exception classes.
3. Port the proven Ribbon + corrected Particle physical writer into C++.
4. Build the complete file-level `v264 + .skin -> v256 embedded View` writer.
5. Perform one real Turtle 1.18.1 Ribbon model regression and one Particle model regression.
6. Unlock batch conversion by feature class.
