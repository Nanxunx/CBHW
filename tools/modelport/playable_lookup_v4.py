#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Classic/Turtle MD20 v256 PlayableAnimationLookup generator (Golden V4).

Evidence:
- successful 1.12 Golden M2 targets supplied by this project
- historical LKBC fallback graph used only as a cross-check/base

Important correction over the earlier V4 draft:
Do NOT derive this table directly from build12340 AnimationData.dbc field 5.
That reproduced many cases but not the successful FelReaver/Sunwell targets.
The graph below is the historical 226-era fallback graph plus extension edges
observed directly in successful 1.12 Golden targets.
"""
from __future__ import annotations
import struct
from typing import Dict, Iterable, List, Sequence, Tuple

PLAYABLE_COUNT = 226

LEGACY_FALLBACK: Dict[int, int] = {
    0:147, 6:1, 8:25, 9:8, 10:9, 13:4, 17:16, 18:17, 19:18, 20:8,
    21:8, 22:8, 23:8, 24:8, 26:25, 27:25, 28:25, 30:8, 32:16, 33:32,
    36:8, 42:4, 45:42, 51:52, 52:31, 53:54, 54:33, 55:16, 57:17,
    58:18, 59:87, 71:100, 85:17, 86:19, 87:88, 88:16, 95:16,
    97:96, 98:96, 100:99, 101:99, 107:16, 115:114, 116:114,
    117:87, 118:57, 119:4, 123:128, 124:52, 125:31, 129:128,
    131:1, 132:131, 135:42, 136:62, 137:14, 138:63, 141:115,
    143:5, 146:0, 147:146, 148:146, 149:148, 150:148, 151:150,
    152:150, 187:5, 188:50, 189:50, 196:1, 197:69, 199:61,
    203:115, 208:60, 209:84, 210:113, 211:69, 212:16,
    223:119, 224:127,
}

# Required to reproduce all 13 second-batch successful targets exactly.
GOLDEN_FALLBACK_EXTENSIONS: Dict[int, int] = {
    170:16,
    171:16,
    172:16,
    173:16,
    174:16,
    175:30,
    176:16,
    178:16,
    179:16,
    181:16,
    191:159,
}

FALLBACK: Dict[int, int] = dict(LEGACY_FALLBACK)
FALLBACK.update(GOLDEN_FALLBACK_EXTENSIONS)

PLAY_THEN_STOP = {6, 97, 100, 115, 123, 132, 188}
PLAY_BACKWARDS = {13, 45, 101, 189}


def resolve_fallback(requested_id: int, present_animation_ids: set[int], fallbacks: Dict[int, int] | None = None) -> int:
    graph = FALLBACK if fallbacks is None else fallbacks
    current = int(requested_id)
    seen: set[int] = set()
    while current not in present_animation_ids:
        if current in seen:
            return 0
        seen.add(current)
        if current < 0 or current >= PLAYABLE_COUNT:
            return 0
        current = int(graph.get(current, 0))
    return current


def build_playable_records(
    present_animation_ids: Iterable[int],
    fallbacks: Dict[int, int] | None = None,
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


def build_playable_bytes(present_animation_ids: Iterable[int], animation_data_dbc=None) -> bytes:
    """Build the 226-record table.

    `animation_data_dbc` is accepted only for backward API compatibility with
    the earlier V4 helper. Golden V4 no longer derives the graph from it.
    """
    return serialize_playable(build_playable_records(present_animation_ids))
