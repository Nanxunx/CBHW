#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Classic/Turtle MD20 v256 PlayableAnimationLookup generator.

Validated against 16 successful 3.3.5a -> 1.12 Golden M2 pairs.
The target table is model-aware: 226 records of int16 animation_id + int16 flags.
"""
from __future__ import annotations
import struct
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

PLAYABLE_COUNT = 226

# Successful Golden targets require these legacy routes although the build12340
# DBC fallback field is zero for them.
GOLDEN_FALLBACK_OVERRIDES = {
    172: 16,
    174: 16,
    181: 16,
    191: 159,
}

PLAY_THEN_STOP = {6, 97, 100, 115, 123, 132, 188}
PLAY_BACKWARDS = {13, 45, 101, 189}


def load_build12340_fallbacks(animation_data_dbc: Path) -> Dict[int, int]:
    """Read build12340 AnimationData.dbc: field0=ID, field5=Fallback."""
    d = animation_data_dbc.read_bytes()
    if len(d) < 20 or d[:4] != b"WDBC":
        raise ValueError("AnimationData.dbc is not WDBC")
    record_count, field_count, record_size, string_size = struct.unpack_from("<4I", d, 4)
    if field_count != 8 or record_size != 32:
        raise ValueError(
            f"expected build12340 AnimationData 8x4 layout, got fields={field_count} record_size={record_size}"
        )
    if 20 + record_count * record_size + string_size > len(d):
        raise ValueError("truncated AnimationData.dbc")

    out: Dict[int, int] = {}
    for i in range(record_count):
        rec = struct.unpack_from("<8I", d, 20 + i * record_size)
        anim_id = rec[0]
        if anim_id < PLAYABLE_COUNT:
            out[anim_id] = rec[5]
    out.update(GOLDEN_FALLBACK_OVERRIDES)
    return out


def resolve_fallback(requested_id: int, present_animation_ids: set[int], fallbacks: Dict[int, int]) -> int:
    current = requested_id
    seen: set[int] = set()
    while current not in present_animation_ids:
        if current in seen:
            return 0
        seen.add(current)
        current = fallbacks.get(current, 0)
    return current


def build_playable_records(
    present_animation_ids: Iterable[int],
    fallbacks: Dict[int, int],
) -> List[Tuple[int, int]]:
    present = set(int(x) for x in present_animation_ids)
    records: List[Tuple[int, int]] = []
    for requested in range(PLAYABLE_COUNT):
        real = resolve_fallback(requested, present, fallbacks)
        flags = 0
        if real != requested:
            if requested in PLAY_THEN_STOP:
                flags = 3
            elif requested in PLAY_BACKWARDS:
                flags = 1
        records.append((real, flags))
    return records


def serialize_playable(records: Sequence[Tuple[int, int]]) -> bytes:
    if len(records) != PLAYABLE_COUNT:
        raise ValueError("PlayableAnimationLookup must contain exactly 226 records")
    return b"".join(struct.pack("<hh", int(anim_id), int(flags)) for anim_id, flags in records)


def build_playable_bytes(present_animation_ids: Iterable[int], animation_data_dbc: Path) -> bytes:
    return serialize_playable(
        build_playable_records(
            present_animation_ids,
            load_build12340_fallbacks(animation_data_dbc),
        )
    )
