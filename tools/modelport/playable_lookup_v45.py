#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PlayableAnimationLookup V4.5.

Evidence order:
1. successful 1.12/Turtle-compatible Golden M2 targets;
2. build12340 AnimationData.dbc field 5 as the primary fallback graph;
3. a very small Golden override set for source rows whose field-5 value does
   not reproduce the successful legacy target;
4. historical LKBC fallback logic as a cross-check only.

Important:
- 226 records, each <int16 fallbackAnimationID, int16 flags>.
- The graph is MODEL-AWARE at resolution time: follow fallback edges until the
  model actually has that AnimationID.
- Do not use the old V4 hardcoded 170->16 family. build12340 field 5 shows the
  stronger chain 170/171/173/176/178/179 -> 19, which naturally resolves to
  19/18/17/16 depending on what the model contains.
"""
from __future__ import annotations

import struct
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

PLAYABLE_COUNT = 226

# Golden target corrections over build12340 AnimationData.dbc field 5.
# 146->0 is also the intentional historic LKBC anti-loop override.
GOLDEN_OVERRIDES: Dict[int, int] = {
    121: 14,
    146: 0,
    172: 16,
    174: 16,
    181: 19,
    191: 159,
}

PLAY_THEN_STOP = {6, 97, 100, 115, 123, 132, 188}
PLAY_BACKWARDS = {13, 45, 101, 189}


def _parse_wdbc8(path: Path) -> List[Tuple[int, ...]]:
    d = path.read_bytes()
    if len(d) < 20:
        raise ValueError("AnimationData.dbc too small")
    magic, records, fields, record_size, string_size = struct.unpack_from("<4s4I", d, 0)
    if magic != b"WDBC":
        raise ValueError("AnimationData.dbc is not WDBC")
    if fields != 8 or record_size != 32:
        raise ValueError(
            f"expected build12340 AnimationData.dbc 8 fields / 32 bytes, got "
            f"{fields} fields / {record_size} bytes"
        )
    end = 20 + records * record_size
    if end + string_size > len(d):
        raise ValueError("AnimationData.dbc truncated")
    return [
        struct.unpack_from("<8I", d, 20 + i * 32)
        for i in range(records)
    ]


def graph_from_build12340_animation_data(path: Path) -> Dict[int, int]:
    """Build the 0..225 fallback graph from source WotLK AnimationData.dbc.

    Field index 5 is used as raw fallback evidence. We deliberately call it
    "field 5" here because historical schemas attach inconsistent names to the
    later AnimationData columns.
    """
    rows = _parse_wdbc8(path)
    graph: Dict[int, int] = {}
    for row in rows:
        animation_id = int(row[0])
        if 0 <= animation_id < PLAYABLE_COUNT:
            graph[animation_id] = int(row[5])
    # Missing IDs fail safely to animation 0.
    for i in range(PLAYABLE_COUNT):
        graph.setdefault(i, 0)
    graph.update(GOLDEN_OVERRIDES)
    return graph


def resolve_fallback(
    requested_id: int,
    present_animation_ids: Iterable[int],
    graph: Dict[int, int],
) -> int:
    present = set(int(x) for x in present_animation_ids)
    current = int(requested_id)
    seen: set[int] = set()
    while current not in present:
        if current in seen:
            return 0
        seen.add(current)
        if current < 0 or current >= PLAYABLE_COUNT:
            return 0
        current = int(graph.get(current, 0))
    return current


def build_playable_records(
    present_animation_ids: Iterable[int],
    graph: Dict[int, int],
) -> List[Tuple[int, int]]:
    present = set(int(x) for x in present_animation_ids)
    records: List[Tuple[int, int]] = []
    for requested in range(PLAYABLE_COUNT):
        real = resolve_fallback(requested, present, graph)
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
    return b"".join(struct.pack("<hh", int(a), int(f)) for a, f in records)


def build_playable_bytes(
    present_animation_ids: Iterable[int],
    animation_data_dbc: Path,
) -> bytes:
    graph = graph_from_build12340_animation_data(animation_data_dbc)
    return serialize_playable(build_playable_records(present_animation_ids, graph))
