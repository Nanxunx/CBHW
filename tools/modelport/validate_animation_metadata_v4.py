#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""V4 validator for crash-relevant Classic v256 animation metadata."""
from __future__ import annotations
import json
import struct
import sys
from pathlib import Path

from animation_metadata_v4 import build_animation_lookup, compare_sequence_semantics, parse_wotlk_sequences
from playable_lookup_v4 import build_playable_bytes


def validate(source_v264: Path, target_v256: Path, animation_data_dbc: Path) -> dict:
    src = source_v264.read_bytes()
    dst = target_v256.read_bytes()
    issues = compare_sequence_semantics(src, dst)
    seqs = parse_wotlk_sequences(src)

    expected_lookup = build_animation_lookup(seqs)
    lookup_count, lookup_off = struct.unpack_from("<II", dst, 36)
    actual_lookup = dst[lookup_off:lookup_off + lookup_count * 2] if lookup_count else b""
    if lookup_count != len(expected_lookup) // 2 or actual_lookup != expected_lookup:
        issues.append("AnimationLookup does not match first-physical-sequence rule")

    expected_playable = build_playable_bytes(
        (s["animation_id"] for s in seqs), animation_data_dbc
    )
    playable_count, playable_off = struct.unpack_from("<II", dst, 44)
    actual_playable = dst[playable_off:playable_off + playable_count * 4]
    if playable_count != 226 or actual_playable != expected_playable:
        issues.append("PlayableAnimationLookup does not match V4 fallback algorithm")

    return {
        "source": str(source_v264),
        "target": str(target_v256),
        "valid": not issues,
        "issues": issues,
        "sequence_count": len(seqs),
        "animation_lookup_count": lookup_count,
        "playable_count": playable_count,
    }


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: validate_animation_metadata_v4.py source335.m2 target112.m2 AnimationData.dbc"
        )
    print(json.dumps(
        validate(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])),
        ensure_ascii=False, indent=2
    ))
