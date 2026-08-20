#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""WotLK v264 -> Classic/Turtle v256 RibbonEmitter writer.

Golden-validated structure:
- source WotLK ribbon record: 176 bytes
- target Classic ribbon record: 220 bytes
- texture_refs / blend_refs payloads are preserved (uint16 arrays)
- WotLK trailing uint32 unknown1 is dropped
- the six animated tracks are flattened with the same legacy Range/Times/Keys
  algorithm used by bones/texture animations:
    color(Vec3), opacity(uint16), heightAbove(float), heightBelow(float),
    texSlot(uint16), enabled(uint8)

This module writes standard MD20 v256 structures only. It is intentionally
independent of any Orange/private format.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Iterable, List, Sequence, Tuple


SOURCE_RIBBON_STRIDE = 176
TARGET_RIBBON_STRIDE = 220


@dataclass(frozen=True)
class SequenceWindow:
    start: int
    end: int


class ByteBuilder:
    def __init__(self, initial_size: int = 0):
        self.data = bytearray(initial_size)

    def append(self, raw: bytes) -> int:
        if not raw:
            return 0
        off = len(self.data)
        self.data.extend(raw)
        return off

    def reserve(self, size: int) -> int:
        off = len(self.data)
        self.data.extend(b"\0" * size)
        return off

    def patch(self, off: int, raw: bytes) -> None:
        end = off + len(raw)
        if off < 0 or end > len(self.data):
            raise ValueError("patch out of range")
        self.data[off:end] = raw


def build_sequence_windows(lengths: Sequence[int], gap: int = 3333) -> List[SequenceWindow]:
    timeline = 0
    out: List[SequenceWindow] = []
    for length in lengths:
        if int(length) < 0:
            raise ValueError("negative animation length")
        timeline += gap
        start = timeline
        timeline += int(length)
        out.append(SequenceWindow(start, timeline))
    return out


def _pair(d: bytes, off: int) -> Tuple[int, int]:
    return struct.unpack_from("<II", d, off)


def _read_source_track(d: bytes, off: int, key_size: int):
    typ, seq, tn, to, kn, ko = struct.unpack_from("<hhIIII", d, off)
    if tn != kn:
        raise ValueError("source track outer times/keys count mismatch")
    times: List[List[int]] = []
    keys: List[List[bytes]] = []
    for i in range(tn):
        tcnt, toff = _pair(d, to + i * 8)
        kcnt, koff = _pair(d, ko + i * 8)
        if tcnt != kcnt:
            raise ValueError("source track inner times/keys count mismatch")
        if tcnt:
            if toff <= 0 or toff + tcnt * 4 > len(d):
                raise ValueError("source track time payload out of range")
            ts = list(struct.unpack_from("<" + "I" * tcnt, d, toff))
        else:
            ts = []
        vals: List[bytes] = []
        if kcnt:
            if koff <= 0 or koff + kcnt * key_size > len(d):
                raise ValueError("source track key payload out of range")
            vals = [
                d[koff + j * key_size:koff + (j + 1) * key_size]
                for j in range(kcnt)
            ]
        times.append(ts)
        keys.append(vals)
    return typ, seq, times, keys


def _legacy_ranges(counts: Sequence[int]) -> List[Tuple[int, int]]:
    cursor = 0
    out: List[Tuple[int, int]] = []
    for count in counts:
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


def _flatten_track(
    times: Sequence[Sequence[int]],
    keys: Sequence[Sequence[bytes]],
    windows: Sequence[SequenceWindow],
    default_key: bytes,
    global_sequence: bool,
):
    if len(times) != len(keys):
        raise ValueError("times/keys outer count mismatch")
    if not times:
        return [], [], []

    if global_sequence:
        if len(times) != 1:
            raise ValueError("global-sequence ribbon track must have one outer group")
        if len(times[0]) != len(keys[0]):
            raise ValueError("global-sequence inner mismatch")
        return [], list(times[0]), list(keys[0])

    if len(times) == len(windows):
        counts: List[int] = []
        out_times: List[int] = []
        out_keys: List[bytes] = []
        for ts, vs, win in zip(times, keys, windows):
            if len(ts) != len(vs):
                raise ValueError("per-sequence inner mismatch")
            n = len(ts)
            counts.append(n)
            if n == 0:
                out_times.extend((win.start, win.end))
                out_keys.extend((default_key, default_key))
            elif n == 1:
                out_times.extend((win.start + int(ts[0]), win.end + int(ts[0])))
                out_keys.extend((vs[0], vs[0]))
            else:
                out_times.extend(win.start + int(t) for t in ts)
                out_keys.extend(vs)
        return _legacy_ranges(counts), out_times, out_keys

    # Golden legacy convention: a lone non-global group is preserved raw.
    if len(times) == 1:
        if len(times[0]) != len(keys[0]):
            raise ValueError("single-group track mismatch")
        return [], list(times[0]), list(keys[0])

    raise ValueError(
        f"unsupported ribbon track outer count {len(times)} for {len(windows)} sequences"
    )


def _write_legacy_track(
    builder: ByteBuilder,
    typ: int,
    seq: int,
    ranges: Sequence[Tuple[int, int]],
    times: Sequence[int],
    keys: Sequence[bytes],
) -> bytes:
    range_raw = b"".join(struct.pack("<II", int(a), int(b)) for a, b in ranges)
    time_raw = b"".join(struct.pack("<I", int(t)) for t in times)
    key_raw = b"".join(keys)
    ro = builder.append(range_raw) if ranges else 0
    to = builder.append(time_raw) if times else 0
    ko = builder.append(key_raw) if keys else 0
    return struct.pack(
        "<hhIIIIII",
        int(typ), int(seq),
        len(ranges), ro,
        len(times), to,
        len(keys), ko,
    )


def _copy_u16_array(builder: ByteBuilder, source: bytes, count: int, off: int) -> Tuple[int, int]:
    if count == 0:
        return 0, 0
    if off <= 0 or off + count * 2 > len(source):
        raise ValueError("ribbon uint16 array out of range")
    return count, builder.append(source[off:off + count * 2])


def convert_ribbons(
    builder: ByteBuilder,
    source: bytes,
    source_count: int,
    source_offset: int,
    sequence_windows: Sequence[SequenceWindow],
) -> Tuple[int, int]:
    """Append all converted Classic ribbon records/data.

    Returns `(count, target_record_offset)`.
    """
    count = int(source_count)
    if count == 0:
        return 0, 0
    if source_offset <= 0 or source_offset + count * SOURCE_RIBBON_STRIDE > len(source):
        raise ValueError("source ribbon record array out of range")

    target_offset = builder.reserve(count * TARGET_RIBBON_STRIDE)

    specs = [
        # source_rel, target_rel, key_size, default
        (36, 36, 12, b"\0" * 12),   # color
        (56, 64, 2, b"\0" * 2),     # opacity
        (76, 92, 4, b"\0" * 4),     # heightAbove
        (96, 120, 4, b"\0" * 4),    # heightBelow
        (132, 164, 2, b"\0" * 2),   # texSlot
        (152, 192, 1, b"\0"),        # enabled
    ]

    for i in range(count):
        so = source_offset + i * SOURCE_RIBBON_STRIDE
        to = target_offset + i * TARGET_RIBBON_STRIDE

        unknown0, bone = struct.unpack_from("<iI", source, so)
        pos = source[so + 8:so + 20]

        tex_n, tex_o = _pair(source, so + 20)
        blend_n, blend_o = _pair(source, so + 28)
        tex_pair = _copy_u16_array(builder, source, tex_n, tex_o)
        blend_pair = _copy_u16_array(builder, source, blend_n, blend_o)

        rec = bytearray(TARGET_RIBBON_STRIDE)
        struct.pack_into("<iI", rec, 0, unknown0, bone)
        rec[8:20] = pos
        struct.pack_into("<II", rec, 20, *tex_pair)
        struct.pack_into("<II", rec, 28, *blend_pair)

        # Copy non-track scalar tail excluding WotLK's trailing unknown1.
        rec[148:164] = source[so + 116:so + 132]

        for src_rel, dst_rel, key_size, default_key in specs:
            typ, seq, times, keys = _read_source_track(source, so + src_rel, key_size)
            ranges, flat_times, flat_keys = _flatten_track(
                times, keys, sequence_windows, default_key, global_sequence=(seq >= 0)
            )
            rec[dst_rel:dst_rel + 28] = _write_legacy_track(
                builder, typ, seq, ranges, flat_times, flat_keys
            )

        builder.patch(to, bytes(rec))

    return count, target_offset
