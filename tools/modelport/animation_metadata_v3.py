#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Golden-Reference-aligned Classic v256 animation metadata helpers.

No Orange/private format dependency.
Output policy remains standard MD20 v256.

Validated against:
- successful 1.12 Sword
- successful 1.12 Mace
- successful 1.12 Bow_2H_Crossbow_PVP330_D_01
- failed project Crossbow AnimationV1
"""
from __future__ import annotations
import struct
from pathlib import Path
from typing import Sequence

PLAYABLE_COUNT = 226


def build_animation_lookup(animation_ids: Sequence[int]) -> bytes:
    """Classic lookup count = max(AnimationID)+1; missing=0xFFFF."""
    if not animation_ids:
        return b""
    if any(x < 0 for x in animation_ids):
        raise ValueError("AnimationID must be non-negative")
    out = [0xFFFF] * (max(animation_ids) + 1)
    for seq_index, anim_id in enumerate(animation_ids):
        # Golden Crossbow: ID0->0, ID160->1, ID161->2.
        # First sequence wins if duplicate IDs exist. SubAnimation variants need
        # more Golden samples before duplicate handling is generalized.
        if out[anim_id] == 0xFFFF:
            out[anim_id] = seq_index
    return struct.pack("<" + "H" * len(out), *out)


def build_model_aware_playable(
    base_226: bytes,
    sequences: Sequence[tuple[int, int, int]],
) -> bytes:
    """
    Start from target-native 226x4 fallback table, then update actual model
    AnimationIDs observed with SubAnimationID=0.

    Each record is two uint16.
    Golden Crossbow confirms:
      playable[160]=(160,0)
      playable[161]=(161,0)
    """
    if len(base_226) != PLAYABLE_COUNT * 4:
        raise ValueError("base playable must be exactly 226*4 bytes")
    b = bytearray(base_226)
    for anim_id, sub_id, seq_index in sequences:
        if 0 <= anim_id < PLAYABLE_COUNT and sub_id == 0 and anim_id != 0:
            struct.pack_into("<HH", b, anim_id * 4, anim_id, 0)
    return bytes(b)


def parse_classic_sequences(d: bytes, count: int, offset: int):
    out = []
    for i in range(count):
        o = offset + i * 68
        if o + 68 > len(d):
            raise ValueError(f"sequence {i} out of range")
        anim_id, sub_id = struct.unpack_from("<HH", d, o)
        index = struct.unpack_from("<H", d, o + 66)[0]
        out.append((anim_id, sub_id, index))
    return out


def repair_v256_animation_metadata(
    input_m2: Path,
    output_m2: Path,
    base_playable_226: Path,
    fix_quat_minus_one: bool = True,
):
    """
    Repair a standard v256 model produced by the older project serializer:
    - Sequence Index -> real sequence index
    - append generated AnimationLookup
    - append model-aware 226 PlayableAnimationLookup
    - exact-fix rotation float keys equal to old stf(-1) result -> 1.0

    This does not introduce any private wrapper.
    Existing section offsets stay valid because new arrays are appended.
    """
    d = bytearray(input_m2.read_bytes())
    if len(d) < 324 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 256:
        raise ValueError("input must be standard MD20 v256")

    anim_count, anim_off = struct.unpack_from("<II", d, 28)
    bone_count, bone_off = struct.unpack_from("<II", d, 52)

    seqs = []
    for i in range(anim_count):
        o = anim_off + i * 68
        anim_id, sub_id = struct.unpack_from("<HH", d, o)
        struct.pack_into("<H", d, o + 66, i)
        seqs.append((anim_id, sub_id, i))

    lookup = build_animation_lookup([x[0] for x in seqs])
    lookup_off = len(d) if lookup else 0
    d.extend(lookup)
    struct.pack_into("<II", d, 36, len(lookup) // 2, lookup_off)

    base = base_playable_226.read_bytes()
    playable = build_model_aware_playable(base, seqs)
    playable_off = len(d)
    d.extend(playable)
    struct.pack_into("<II", d, 44, PLAYABLE_COUNT, playable_off)

    fixed_components = 0
    if fix_quat_minus_one:
        bad = struct.pack("<f", 32766.0 / 32767.0)
        good = struct.pack("<f", 1.0)
        for bi in range(bone_count):
            bo = bone_off + bi * 108
            if bo + 108 > len(d):
                raise ValueError(f"bone {bi} out of range")
            # Classic bone rotation AnimationBlock starts at +40.
            typ, seq, rn, ro, tn, to, kn, ko = struct.unpack_from(
                "<hhIIIIII", d, bo + 40
            )
            if kn and ko + kn * 16 <= len(d):
                for k in range(kn):
                    qoff = ko + k * 16
                    for c in range(4):
                        p = qoff + c * 4
                        if d[p:p + 4] == bad:
                            d[p:p + 4] = good
                            fixed_components += 1

    output_m2.parent.mkdir(parents=True, exist_ok=True)
    output_m2.write_bytes(d)
    return {
        "input": str(input_m2),
        "output": str(output_m2),
        "animations": anim_count,
        "animation_ids": [x[0] for x in seqs],
        "animation_lookup_count": len(lookup) // 2,
        "playable_count": PLAYABLE_COUNT,
        "quaternion_components_fixed": fixed_components,
        "size": len(d),
    }
