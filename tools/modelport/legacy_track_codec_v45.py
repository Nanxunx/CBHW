#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Binary WotLK->Classic animation-track codec used by ModelPort V4.5.

This module complements legacy_tracks.py with binary parsing/serialization.
It deliberately owns no M2 header logic: callers provide source track offsets,
sequence lengths and the target block absolute offset.

Golden rules:
- WotLK track header: 20 bytes (interpolation/global + nested Times/Keys refs)
- Classic/Turtle track header: 28 bytes (Ranges/Times/Keys)
- per-sequence n=0 -> start/end + semantic default twice
- per-sequence n=1 -> start/end + source value twice
- per-sequence n>1 -> source relative times + target sequence start
- Classic range count = sequence_count + 1, final [0,0]
- global sequence / free-standing single outer array -> no Classic ranges
- one-model-sequence + one outer array uses the per-sequence branch, which is
  required by successful Golden Ribbon and Particle targets.
"""
from __future__ import annotations
from dataclasses import dataclass
import struct
from typing import List, Sequence, Tuple


@dataclass(frozen=True)
class SequenceWindow:
    start: int
    end: int


@dataclass
class WotLKTrack:
    interpolation: int
    global_sequence: int
    times: List[List[int]]
    values: List[List[bytes]]
    key_size: int


@dataclass
class ClassicTrack:
    interpolation: int
    global_sequence: int
    ranges: List[Tuple[int, int]]
    times: List[int]
    values: List[bytes]
    key_size: int


def check_span(data: bytes, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError(
            f"{label} out of range: offset={offset} size={size} file={len(data)}"
        )


def pair(data: bytes, offset: int) -> Tuple[int, int]:
    check_span(data, offset, 8, "array pair")
    return struct.unpack_from("<II", data, offset)


def build_sequence_windows(lengths: Sequence[int], gap: int = 3333) -> List[SequenceWindow]:
    timeline = 0
    out: List[SequenceWindow] = []
    for length in lengths:
        if int(length) < 0:
            raise ValueError("negative sequence length")
        timeline += gap
        start = timeline
        timeline += int(length)
        out.append(SequenceWindow(start, timeline))
    return out


def _read_nested_array(
    data: bytes,
    pair_offset: int,
    element_size: int,
    label: str,
) -> List[List[bytes]]:
    outer_count, outer_offset = pair(data, pair_offset)
    if outer_count == 0:
        return []
    check_span(data, outer_offset, outer_count * 8, label + " outer refs")
    result: List[List[bytes]] = []
    for i in range(outer_count):
        count, offset = pair(data, outer_offset + i * 8)
        if count:
            check_span(data, offset, count * element_size, label + f"[{i}]")
        result.append(
            [
                data[offset + j * element_size: offset + (j + 1) * element_size]
                for j in range(count)
            ]
        )
    return result


def parse_wotlk_track(data: bytes, offset: int, base_key_size: int) -> WotLKTrack:
    check_span(data, offset, 20, "WotLK track")
    interpolation, global_sequence = struct.unpack_from("<Hh", data, offset)
    key_size = base_key_size * (3 if interpolation in (2, 3) else 1)
    raw_times = _read_nested_array(data, offset + 4, 4, "timestamps")
    raw_values = _read_nested_array(data, offset + 12, key_size, "keys")
    if len(raw_times) != len(raw_values):
        raise ValueError("track outer timestamp/key array count mismatch")
    times = [[struct.unpack("<I", x)[0] for x in inner] for inner in raw_times]
    return WotLKTrack(
        interpolation=interpolation,
        global_sequence=global_sequence,
        times=times,
        values=raw_values,
        key_size=key_size,
    )


def classic_ranges(per_sequence_counts: Sequence[int]) -> List[Tuple[int, int]]:
    cursor = 0
    out: List[Tuple[int, int]] = []
    for count in per_sequence_counts:
        start = cursor
        if count == 1:
            cursor += 1
        elif count > 1:
            cursor += count - 1
        else:
            cursor += 1
        out.append((start, cursor))
        cursor += 1
    out.append((0, 0))
    return out


def convert_track(
    source: WotLKTrack,
    windows: Sequence[SequenceWindow],
    base_default: bytes,
    *,
    empty_outer_mode: str = "empty",
) -> ClassicTrack:
    """Convert a WotLK nested track to Classic Ranges/Times/Keys.

    empty_outer_mode:
      - "empty": zero Ranges/Times/Keys (normal Ribbon/Particle tracks)
      - "enabled_one": zero Ranges, Times=[0], Keys=[1] (Particle enabled)
    """
    expected_base = source.key_size // (3 if source.interpolation in (2, 3) else 1)
    if len(base_default) != expected_base:
        raise ValueError(
            f"default key size {len(base_default)} does not match base key size {expected_base}"
        )
    default = base_default * (3 if source.interpolation in (2, 3) else 1)

    if not source.times:
        if empty_outer_mode == "empty":
            return ClassicTrack(
                source.interpolation, source.global_sequence, [], [], [], source.key_size
            )
        if empty_outer_mode == "enabled_one":
            if source.key_size != 1:
                raise ValueError("enabled_one requires a 1-byte key")
            return ClassicTrack(
                source.interpolation, source.global_sequence, [], [0], [b"\x01"], 1
            )
        raise ValueError(f"unknown empty_outer_mode {empty_outer_mode!r}")

    if source.global_sequence >= 0:
        return ClassicTrack(
            source.interpolation,
            source.global_sequence,
            [],
            list(source.times[0]),
            list(source.values[0]),
            source.key_size,
        )

    # Order is intentional: for a one-sequence model, outer_count==1 is a
    # per-sequence track and Golden targets create ranges/start-end expansion.
    if len(source.times) == len(windows):
        counts: List[int] = []
        out_times: List[int] = []
        out_values: List[bytes] = []
        for ts, vs, window in zip(source.times, source.values, windows):
            if len(ts) != len(vs):
                raise ValueError("inner timestamp/key count mismatch")
            n = len(ts)
            counts.append(n)
            if n == 0:
                out_times.extend((window.start, window.end))
                out_values.extend((default, default))
            elif n == 1:
                out_times.extend((window.start + ts[0], window.end + ts[0]))
                out_values.extend((vs[0], vs[0]))
            else:
                out_times.extend(window.start + t for t in ts)
                out_values.extend(vs)
        return ClassicTrack(
            source.interpolation,
            source.global_sequence,
            classic_ranges(counts),
            out_times,
            out_values,
            source.key_size,
        )

    if len(source.times) == 1:
        return ClassicTrack(
            source.interpolation,
            source.global_sequence,
            [],
            list(source.times[0]),
            list(source.values[0]),
            source.key_size,
        )

    raise ValueError(
        "legacy track outer count is neither sequence count nor one: "
        f"outer={len(source.times)} sequences={len(windows)}"
    )


class BlockBuilder:
    """Build fixed records followed by owned payload with absolute offsets."""

    def __init__(self, fixed_size: int, absolute_offset: int):
        self.fixed = bytearray(fixed_size)
        self.payload = bytearray()
        self.absolute_offset = int(absolute_offset)

    def append(self, data: bytes, alignment: int = 4) -> int:
        if not data:
            return 0
        if alignment > 1:
            pad = (-len(self.payload)) % alignment
            if pad:
                self.payload.extend(b"\x00" * pad)
        absolute = self.absolute_offset + len(self.fixed) + len(self.payload)
        self.payload.extend(data)
        return absolute

    def finish(self) -> bytes:
        return bytes(self.fixed + self.payload)


def serialize_classic_track(
    builder: BlockBuilder,
    record_offset: int,
    track: ClassicTrack,
) -> None:
    ranges_raw = b"".join(struct.pack("<II", a, b) for a, b in track.ranges)
    times_raw = b"".join(struct.pack("<I", t) for t in track.times)
    values_raw = b"".join(track.values)
    range_offset = builder.append(ranges_raw, 4)
    time_offset = builder.append(times_raw, 4)
    value_offset = builder.append(values_raw, 4)
    struct.pack_into(
        "<HhIIIIII",
        builder.fixed,
        record_offset,
        track.interpolation,
        track.global_sequence,
        len(track.ranges),
        range_offset,
        len(track.times),
        time_offset,
        len(track.values),
        value_offset,
    )
