#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ModelPort Animation Metadata Validator V3.
Checks crash-relevant Classic v256 animation metadata discovered from the
successful 335 -> 1.12 Golden Reference.
"""
import json
import struct
import sys
from pathlib import Path


def parse_header(d: bytes):
    if len(d) < 324 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 256:
        raise ValueError("not standard MD20 v256")
    return {
        "animations": struct.unpack_from("<II", d, 28),
        "anim_lookup": struct.unpack_from("<II", d, 36),
        "playable": struct.unpack_from("<II", d, 44),
    }


def validate(path):
    d = Path(path).read_bytes()
    h = parse_header(d)
    issues = []
    ac, ao = h["animations"]

    seqs = []
    for i in range(ac):
        o = ao + i * 68
        if o + 68 > len(d):
            issues.append(f"sequence_{i}_out_of_range")
            continue
        anim_id, sub_id = struct.unpack_from("<HH", d, o)
        index = struct.unpack_from("<H", d, o + 66)[0]
        seqs.append((anim_id, sub_id, index))
        if index != i:
            issues.append(f"sequence_{i}_index_is_{index}_expected_{i}")

    if seqs:
        expected_lookup_count = max(x[0] for x in seqs) + 1
        lc, lo = h["anim_lookup"]
        if lc < expected_lookup_count:
            issues.append(
                f"anim_lookup_too_short_{lc}_expected_at_least_{expected_lookup_count}"
            )
        elif lo + lc * 2 > len(d):
            issues.append("anim_lookup_out_of_range")
        else:
            lookup = struct.unpack_from("<" + "H" * lc, d, lo)
            for i, (anim_id, sub_id, index) in enumerate(seqs):
                if anim_id >= lc:
                    continue
                if lookup[anim_id] != i:
                    issues.append(
                        f"anim_lookup[{anim_id}]={lookup[anim_id]}_expected_{i}"
                    )

    pc, po = h["playable"]
    if pc != 226:
        issues.append(f"playable_count_{pc}_expected_226")
    elif po + pc * 4 > len(d):
        issues.append("playable_out_of_range")
    else:
        playable = [struct.unpack_from("<HH", d, po + i * 4) for i in range(pc)]
        for anim_id, sub_id, index in seqs:
            if anim_id < 226 and anim_id != 0 and sub_id == 0:
                if playable[anim_id] != (anim_id, 0):
                    issues.append(
                        f"playable[{anim_id}]={playable[anim_id]}_expected_({anim_id},0)"
                    )

    return {
        "path": str(path),
        "valid": not issues,
        "issues": issues,
        "sequences": [
            {"animation_id": a, "sub_id": s, "index": i}
            for a, s, i in seqs
        ],
        "animation_lookup": h["anim_lookup"],
        "playable": h["playable"],
    }


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: validate_animation_metadata_v3.py model.m2")
    print(json.dumps(validate(sys.argv[1]), ensure_ascii=False, indent=2))
