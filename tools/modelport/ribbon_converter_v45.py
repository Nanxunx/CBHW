#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""WotLK MD20 v264 RibbonEmitter -> Classic/Turtle MD20 v256 Ribbon block.

Golden evidence frozen for V4.5:
- 6 successful 335 -> 1.12/Turtle-compatible paired models
- 420 RibbonEmitter records
- 2520 animation tracks (6 per emitter)
- 420/420 static prefix + texture/material arrays + static body matched
- 2520/2520 converted animation tracks matched successful target semantics

Confirmed fixed record sizes:
- WotLK v264 RibbonEmitter = 176 bytes
- Classic/Turtle v256 RibbonEmitter = 220 bytes

This module only serializes the Ribbon block plus its owned payload. The
whole-M2 writer must insert the returned block at `target_absolute_offset`
and use that same offset in the target M2 header.
"""
from __future__ import annotations

import struct
from typing import Sequence

from legacy_track_codec_v45 import (
    BlockBuilder,
    build_sequence_windows,
    check_span,
    convert_track,
    pair,
    parse_wotlk_track,
    serialize_classic_track,
)

WOTLK_RIBBON_SIZE = 176
CLASSIC_RIBBON_SIZE = 220

TRACK_SPECS = (
    ("color",       36,  36, 12, struct.pack("<fff", 1.0, 1.0, 1.0)),
    ("alpha",       56,  64,  2, struct.pack("<h", 0)),
    ("height_above",76,  92,  4, struct.pack("<f", 0.0)),
    ("height_below",96, 120,  4, struct.pack("<f", 0.0)),
    ("tex_slot",   132, 164,  2, struct.pack("<H", 0)),
    ("visibility", 152, 192,  1, b"\x00"),
)


def _copy_u16_array(
    source: bytes,
    source_pair_offset: int,
    builder: BlockBuilder,
    target_pair_offset: int,
) -> None:
    count, offset = pair(source, source_pair_offset)
    raw = b""
    if count:
        check_span(source, offset, count * 2, "Ribbon uint16 array")
        raw = source[offset:offset + count * 2]
    new_offset = builder.append(raw, 4)
    struct.pack_into("<II", builder.fixed, target_pair_offset, count, new_offset)


def convert_ribbon_block(
    source_m2: bytes,
    source_ribbon_offset: int,
    ribbon_count: int,
    sequence_lengths: Sequence[int],
    target_absolute_offset: int,
) -> bytes:
    """Convert WotLK RibbonEmitters to a self-contained Classic/Turtle block.

    Layout returned:
        [ribbon_count * 220-byte fixed records][owned array/track payload]

    Internal offsets are absolute M2 offsets based on `target_absolute_offset`.
    """
    if ribbon_count < 0:
        raise ValueError("negative ribbon_count")
    check_span(
        source_m2,
        source_ribbon_offset,
        ribbon_count * WOTLK_RIBBON_SIZE,
        "WotLK RibbonEmitter table",
    )

    windows = build_sequence_windows(sequence_lengths)
    builder = BlockBuilder(
        ribbon_count * CLASSIC_RIBBON_SIZE,
        target_absolute_offset,
    )

    for i in range(ribbon_count):
        source_record = source_ribbon_offset + i * WOTLK_RIBBON_SIZE
        target_record = i * CLASSIC_RIBBON_SIZE

        builder.fixed[target_record:target_record + 20] = (
            source_m2[source_record:source_record + 20]
        )

        _copy_u16_array(
            source_m2,
            source_record + 20,
            builder,
            target_record + 20,
        )
        _copy_u16_array(
            source_m2,
            source_record + 28,
            builder,
            target_record + 28,
        )

        for _, source_off, target_off, base_size, default_value in TRACK_SPECS:
            source_track = parse_wotlk_track(
                source_m2,
                source_record + source_off,
                base_size,
            )
            classic_track = convert_track(
                source_track,
                windows,
                default_value,
            )
            serialize_classic_track(
                builder,
                target_record + target_off,
                classic_track,
            )

        builder.fixed[target_record + 148:target_record + 164] = (
            source_m2[source_record + 116:source_record + 132]
        )

    return builder.finish()
