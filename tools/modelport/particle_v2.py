#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Golden-gated WotLK v264 ParticleEmitter -> Classic/Turtle v256 physical writer V2.

V2 supersedes particle_v1.py for final physical tail offsets.

V4.5 Golden coverage:
- 10 successful 335 -> 1.12 model pairs
- 772 ParticleEmitters
- 8492 animation tracks (11 tracks/emitter)
- fake color/alpha/size and head/tail cell downgrade reproduced 772/772

Confirmed record sizes:
- WotLK ParticleEmitter: 476 bytes
- Classic/Turtle ParticleEmitter: 504 bytes

Safety gate:
Selected Golden emitters all have nUnknownReference == 0. Until a nonzero
Golden pair is analyzed, this converter refuses such emitters rather than
silently inventing a payload mapping.
"""
from __future__ import annotations
import struct
from typing import List, Sequence, Tuple

from legacy_track_codec_v45 import (
    BlockBuilder,
    build_sequence_windows,
    check_span,
    convert_track,
    pair,
    parse_wotlk_track,
    serialize_classic_track,
)

WOTLK_PARTICLE_SIZE = 476
CLASSIC_PARTICLE_SIZE = 504

SOURCE_FLOAT_TRACK_OFFSETS = (52, 72, 92, 112, 132, 152, 176, 200, 220, 240)
TARGET_FLOAT_TRACK_OFFSETS = tuple(52 + 28 * i for i in range(10))


def _copy_string_pair(
    source: bytes,
    source_pair_offset: int,
    builder: BlockBuilder,
    target_pair_offset: int,
) -> None:
    count, offset = pair(source, source_pair_offset)
    raw = b""
    if count:
        check_span(source, offset, count, "particle filename")
        raw = source[offset:offset + count]
    new_offset = builder.append(raw, 4)
    struct.pack_into("<II", builder.fixed, target_pair_offset, count, new_offset)


def _read_fake_u16(source: bytes, record_offset: int, fake_offset: int) -> Tuple[List[int], List[int]]:
    nt, ot = pair(source, record_offset + fake_offset)
    nk, ok = pair(source, record_offset + fake_offset + 8)
    if nt:
        check_span(source, ot, nt * 2, "fake uint16 times")
    if nk:
        check_span(source, ok, nk * 2, "fake uint16 keys")
    times = list(struct.unpack_from("<" + "H" * nt, source, ot)) if nt else []
    keys = list(struct.unpack_from("<" + "H" * nk, source, ok)) if nk else []
    return times, keys


def _read_fake_i16(source: bytes, record_offset: int, fake_offset: int) -> Tuple[List[int], List[int]]:
    nt, ot = pair(source, record_offset + fake_offset)
    nk, ok = pair(source, record_offset + fake_offset + 8)
    if nt:
        check_span(source, ot, nt * 2, "fake int16 times")
    if nk:
        check_span(source, ok, nk * 2, "fake int16 keys")
    times = list(struct.unpack_from("<" + "H" * nt, source, ot)) if nt else []
    keys = list(struct.unpack_from("<" + "h" * nk, source, ok)) if nk else []
    return times, keys


def _read_fake_vec3(source: bytes, record_offset: int, fake_offset: int):
    nt, ot = pair(source, record_offset + fake_offset)
    nk, ok = pair(source, record_offset + fake_offset + 8)
    if nt:
        check_span(source, ot, nt * 2, "fake vec3 times")
    if nk:
        check_span(source, ok, nk * 12, "fake vec3 keys")
    times = list(struct.unpack_from("<" + "H" * nt, source, ot)) if nt else []
    keys = [struct.unpack_from("<fff", source, ok + i * 12) for i in range(nk)]
    return times, keys


def _read_fake_vec2(source: bytes, record_offset: int, fake_offset: int):
    nt, ot = pair(source, record_offset + fake_offset)
    nk, ok = pair(source, record_offset + fake_offset + 8)
    if nt:
        check_span(source, ot, nt * 2, "fake vec2 times")
    if nk:
        check_span(source, ok, nk * 8, "fake vec2 keys")
    times = list(struct.unpack_from("<" + "H" * nt, source, ot)) if nt else []
    keys = [struct.unpack_from("<ff", source, ok + i * 8) for i in range(nk)]
    return times, keys


def _byte_from_float(v: float) -> int:
    return max(0, min(255, int(v)))


def _particle_gradient(source: bytes, record_offset: int):
    color_times, colors = _read_fake_vec3(source, record_offset, 260)
    _, opacity = _read_fake_i16(source, record_offset, 276)
    _, sizes = _read_fake_vec2(source, record_offset, 292)

    midpoint = 0.0
    bgra = bytearray(12)
    if len(color_times) == 3 and len(colors) == 3:
        midpoint = float(color_times[1]) / 32767.0
        for i, (r, g, b) in enumerate(colors):
            alpha = ((opacity[i] >> 7) & 0xFF) if i < len(opacity) else 0
            bgra[i * 4:i * 4 + 4] = bytes(
                (_byte_from_float(b), _byte_from_float(g), _byte_from_float(r), alpha)
            )

    scale = (0.0, 0.0, 0.0)
    if len(sizes) == 3:
        # Golden target keeps the X component; selected data has X==Y.
        scale = (float(sizes[0][0]), float(sizes[1][0]), float(sizes[2][0]))

    return midpoint, bytes(bgra), scale


def _particle_cells(source: bytes, record_offset: int) -> Tuple[int, ...]:
    """Map WotLK head/tail FakeAnimBlock keys to Classic fixed cell fields.

    772/772 Golden emitters match:
      head_cell_begin = first 2 head keys (zero padded)
      pad = 1
      head_cell_end   = next 2 head keys (zero padded)
      pad = 1
      tiles/tail      = first 4 tail keys (zero padded)
    """
    _, head = _read_fake_u16(source, record_offset, 316)
    _, tail = _read_fake_u16(source, record_offset, 332)
    h = (head + [0, 0, 0, 0])[:4]
    t = (tail + [0, 0, 0, 0])[:4]
    return (h[0], h[1], 1, h[2], h[3], 1, t[0], t[1], t[2], t[3])


def convert_particle_block(
    source_m2: bytes,
    source_particle_offset: int,
    particle_count: int,
    sequence_lengths: Sequence[int],
    target_absolute_offset: int,
) -> bytes:
    """Convert WotLK particles to a self-contained Classic/Turtle block.

    Unknown-reference payloads are not yet Golden-covered. Emitters with
    nUnknownReference != 0 raise NotImplementedError.
    """
    if particle_count < 0:
        raise ValueError("negative particle_count")
    check_span(
        source_m2,
        source_particle_offset,
        particle_count * WOTLK_PARTICLE_SIZE,
        "WotLK ParticleEmitter table",
    )
    windows = build_sequence_windows(sequence_lengths)
    builder = BlockBuilder(particle_count * CLASSIC_PARTICLE_SIZE, target_absolute_offset)

    for i in range(particle_count):
        so = source_particle_offset + i * WOTLK_PARTICLE_SIZE
        to = i * CLASSIC_PARTICLE_SIZE

        builder.fixed[to:to + 4] = source_m2[so:so + 4]
        flags = struct.unpack_from("<I", source_m2, so + 4)[0] & 0xFFFF
        struct.pack_into("<I", builder.fixed, to + 4, flags)
        builder.fixed[to + 8:to + 24] = source_m2[so + 8:so + 24]

        _copy_string_pair(source_m2, so + 24, builder, to + 24)
        _copy_string_pair(source_m2, so + 32, builder, to + 32)

        # WotLK: u8 blending, u8 emitter, u16 ParticleColorIndex.
        # Classic: uint16 blending, uint16 emitter. ParticleColorIndex is dropped.
        struct.pack_into("<H", builder.fixed, to + 40, source_m2[so + 40])
        struct.pack_into("<H", builder.fixed, to + 42, source_m2[so + 41])
        builder.fixed[to + 44:to + 52] = source_m2[so + 44:so + 52]

        for src_off, dst_off in zip(SOURCE_FLOAT_TRACK_OFFSETS, TARGET_FLOAT_TRACK_OFFSETS):
            src_track = parse_wotlk_track(source_m2, so + src_off, 4)
            classic = convert_track(src_track, windows, struct.pack("<f", 0.0))
            serialize_classic_track(builder, to + dst_off, classic)

        midpoint, colors, sizes = _particle_gradient(source_m2, so)
        struct.pack_into("<f", builder.fixed, to + 332, midpoint)
        builder.fixed[to + 336:to + 348] = colors
        struct.pack_into("<fff", builder.fixed, to + 348, *sizes)
        struct.pack_into("<10H", builder.fixed, to + 360, *_particle_cells(source_m2, so))

        builder.fixed[to + 380:to + 392] = source_m2[so + 348:so + 360]
        builder.fixed[to + 392:to + 404] = source_m2[so + 360:so + 372]
        builder.fixed[to + 404:to + 408] = source_m2[so + 372:so + 376]
        # source unknown3 Vec2 at +376 is dropped
        builder.fixed[to + 408:to + 412] = source_m2[so + 384:so + 388]
        builder.fixed[to + 412:to + 420] = b"\x00" * 8
        builder.fixed[to + 420:to + 432] = source_m2[so + 396:so + 408]
        # Rot2: successful Golden converter normalizes signed -0.0 to +0.0.
        rot2 = list(struct.unpack_from("<fff", source_m2, so + 408))
        rot2 = [0.0 if value == 0.0 else value for value in rot2]
        struct.pack_into("<fff", builder.fixed, to + 432, *rot2)
        # Classic keeps only source Trans X/Y; source Z is dropped.
        builder.fixed[to + 444:to + 452] = source_m2[so + 420:so + 428]
        builder.fixed[to + 452:to + 468] = source_m2[so + 432:so + 448]

        n_ref, _ = struct.unpack_from("<II", source_m2, so + 448)
        if n_ref != 0:
            raise NotImplementedError(
                "Particle nUnknownReference != 0 is not yet Golden-covered"
            )
        struct.pack_into("<II", builder.fixed, to + 468, 0, 0)

        enabled = parse_wotlk_track(source_m2, so + 456, 1)
        classic_enabled = convert_track(
            enabled,
            windows,
            b"\x01",
            empty_outer_mode="enabled_one",
        )
        serialize_classic_track(builder, to + 476, classic_enabled)

    return builder.finish()
