#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Golden Reference V4 animation metadata helpers.

V4 evidence-driven rules:
- Preserve the WotLK source Sequence.Index/aliasNext value exactly.
- Rebuild AnimationLookup independently from Sequence.Index.
- AnimationLookup count = max(AnimationID)+1, missing=0xFFFF.
- For duplicate AnimationID, prefer SubAnimationID==0; if none exists use the
  first physical occurrence.
- Rebuild the 226-record PlayableAnimationLookup using playable_lookup_v4.
- Preserve the validated 3333-ms Classic sequence timeline construction.
- Exact quaternion compressed short -1 endpoint becomes float +1.0.

No Orange/private wrapper is produced; output remains standard MD20 v256.
"""
from __future__ import annotations
import struct
from pathlib import Path
from typing import List, Sequence

from playable_lookup_v4 import build_playable_bytes


def parse_wotlk_sequences(d: bytes) -> List[dict]:
    if len(d) < 304 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 264:
        raise ValueError("source must be MD20 v264")
    count, offset = struct.unpack_from("<II", d, 28)
    out = []
    for physical_index in range(count):
        o = offset + physical_index * 64
        if o + 64 > len(d):
            raise ValueError(f"source sequence {physical_index} out of range")
        anim_id, sub_id, length = struct.unpack_from("<HHI", d, o)
        index = struct.unpack_from("<H", d, o + 62)[0]
        out.append({
            "physical_index": physical_index,
            "animation_id": anim_id,
            "sub_animation_id": sub_id,
            "length": length,
            "index": index,
            # moveSpeed through Index is layout-compatible after the Classic
            # start/end pair inserts four bytes before it.
            "raw_tail_from_move_speed": d[o + 8:o + 64],
        })
    return out


def parse_classic_sequences(d: bytes) -> List[dict]:
    if len(d) < 324 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 256:
        raise ValueError("target must be MD20 v256")
    count, offset = struct.unpack_from("<II", d, 28)
    out = []
    for physical_index in range(count):
        o = offset + physical_index * 68
        if o + 68 > len(d):
            raise ValueError(f"target sequence {physical_index} out of range")
        anim_id, sub_id, start, end = struct.unpack_from("<HHII", d, o)
        index = struct.unpack_from("<H", d, o + 66)[0]
        out.append({
            "physical_index": physical_index,
            "animation_id": anim_id,
            "sub_animation_id": sub_id,
            "start": start,
            "end": end,
            "length": end - start,
            "index": index,
            "raw_tail_from_move_speed": d[o + 12:o + 68],
        })
    return out


def build_animation_lookup(source_sequences: Sequence[dict]) -> bytes:
    """Build Classic AnimationID -> physical sequence position lookup.

    Golden duplicate rule:
      1) choose the first SubAnimationID==0 entry for an AnimationID;
      2) if no sub=0 exists, choose the first physical occurrence.
    """
    if not source_sequences:
        return b""
    count = max(int(s["animation_id"]) for s in source_sequences) + 1
    lookup = [0xFFFF] * count

    for s in source_sequences:
        anim_id = int(s["animation_id"])
        if int(s["sub_animation_id"]) == 0 and lookup[anim_id] == 0xFFFF:
            lookup[anim_id] = int(s["physical_index"])

    for s in source_sequences:
        anim_id = int(s["animation_id"])
        if lookup[anim_id] == 0xFFFF:
            lookup[anim_id] = int(s["physical_index"])

    return struct.pack("<" + "H" * len(lookup), *lookup)


def compare_sequence_semantics(source: bytes, target: bytes) -> List[str]:
    src = parse_wotlk_sequences(source)
    dst = parse_classic_sequences(target)
    issues: List[str] = []
    if len(src) != len(dst):
        return [f"sequence_count source={len(src)} target={len(dst)}"]

    timeline = 0
    for i, (a, b) in enumerate(zip(src, dst)):
        timeline += 3333
        expected_start = timeline
        expected_end = expected_start + int(a["length"])
        timeline = expected_end

        if a["animation_id"] != b["animation_id"]:
            issues.append(f"seq[{i}] AnimationID")
        if a["sub_animation_id"] != b["sub_animation_id"]:
            issues.append(f"seq[{i}] SubAnimationID")
        if a["length"] != b["length"]:
            issues.append(f"seq[{i}] duration")
        if a["index"] != b["index"]:
            issues.append(f"seq[{i}] Index source={a['index']} target={b['index']}")
        if a["raw_tail_from_move_speed"] != b["raw_tail_from_move_speed"]:
            issues.append(f"seq[{i}] metadata bytes")
        if b["start"] != expected_start or b["end"] != expected_end:
            issues.append(
                f"seq[{i}] timeline target={b['start']}..{b['end']} expected={expected_start}..{expected_end}"
            )
    return issues


def repair_v256_animation_metadata(
    source_v264: Path,
    input_v256: Path,
    output_v256: Path,
    animation_data_dbc: Path | None = None,
    fix_quat_minus_one: bool = True,
) -> dict:
    """Repair older project v256 animation metadata using the original v264.

    The original source is mandatory: once an older target has zeroed/rewritten
    Sequence.Index there is no safe way to reconstruct alias semantics from the
    damaged target alone.

    `animation_data_dbc` is accepted for compatibility but Golden V4 playable
    generation no longer derives its graph from build12340 DBC field 5.
    """
    src = source_v264.read_bytes()
    dst = bytearray(input_v256.read_bytes())
    source_sequences = parse_wotlk_sequences(src)
    target_sequences = parse_classic_sequences(dst)
    if len(source_sequences) != len(target_sequences):
        raise ValueError("source/target sequence count differs")

    timeline = 0
    _, anim_off = struct.unpack_from("<II", dst, 28)
    for i, s in enumerate(source_sequences):
        o = anim_off + i * 68
        timeline += 3333
        start = timeline
        end = start + int(s["length"])
        timeline = end
        struct.pack_into(
            "<HHII", dst, o,
            int(s["animation_id"]), int(s["sub_animation_id"]), start, end
        )
        dst[o + 12:o + 68] = s["raw_tail_from_move_speed"]

    lookup = build_animation_lookup(source_sequences)
    lookup_off = len(dst) if lookup else 0
    dst.extend(lookup)
    struct.pack_into("<II", dst, 36, len(lookup) // 2, lookup_off)

    playable = build_playable_bytes(s["animation_id"] for s in source_sequences)
    playable_off = len(dst)
    dst.extend(playable)
    struct.pack_into("<II", dst, 44, 226, playable_off)

    fixed = 0
    if fix_quat_minus_one:
        bad = struct.pack("<f", 32766.0 / 32767.0)
        good = struct.pack("<f", 1.0)
        bone_count, bone_off = struct.unpack_from("<II", dst, 52)
        for bi in range(bone_count):
            bo = bone_off + bi * 108
            if bo + 108 > len(dst):
                raise ValueError(f"bone {bi} out of range")
            # Classic rotation AnimationBlock at bone +40.
            _, _, _, _, _, _, key_count, key_off = struct.unpack_from(
                "<hhIIIIII", dst, bo + 40
            )
            if key_count and key_off + key_count * 16 <= len(dst):
                for k in range(key_count):
                    q = key_off + k * 16
                    for c in range(4):
                        p = q + c * 4
                        if dst[p:p + 4] == bad:
                            dst[p:p + 4] = good
                            fixed += 1

    output_v256.parent.mkdir(parents=True, exist_ok=True)
    output_v256.write_bytes(dst)
    return {
        "source": str(source_v264),
        "input": str(input_v256),
        "output": str(output_v256),
        "sequences": len(source_sequences),
        "animation_lookup_count": len(lookup) // 2,
        "playable_count": 226,
        "quaternion_components_fixed": fixed,
        "size": len(dst),
    }
