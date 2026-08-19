#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""WotLK v264 -> Classic/Turtle v256 ParticleEmitter semantic writer.

Golden evidence (V4.4 selected corpus):
- source WotLK particle record stride: 476 bytes
- target Classic particle record stride: 504 bytes
- 24 paired Golden models / 930 emitters validated by semantic comparison
- ten main float M2 tracks flatten with the generic legacy Range/Times/Keys rule
- enabled track uses the same rule but has default key 1; an empty outer track
  becomes the target constant Times=[0], Keys=[1]
- WotLK fake color/opacity/size/head/tail tracks are collapsed into the legacy
  fixed midpoint/color/size/tile fields

The writer deliberately refuses non-zero nUnknownReference/ofsUnknownReference,
because no selected Golden emitter exercises that payload. WotLK-only fields
that the successful legacy target discards are surfaced as explicit losses.

Output structures are standard MD20 v256 semantics only; no Orange/private
container is produced.
"""
from __future__ import annotations

import math
import struct
from dataclasses import dataclass, field
from typing import List, Sequence, Tuple

SOURCE_PARTICLE_STRIDE = 476
TARGET_PARTICLE_STRIDE = 504


@dataclass(frozen=True)
class SequenceWindow:
    start: int
    end: int


@dataclass
class ParticleLoss:
    emitter_index: int
    code: str
    source_offset: int
    raw_hex: str


@dataclass
class ParticleConversionReport:
    emitters: int = 0
    losses: List[ParticleLoss] = field(default_factory=list)

    @property
    def lossy(self) -> bool:
        return bool(self.losses)


class ByteBuilder:
    def __init__(self, initial: bytes | bytearray = b""):
        self.data = bytearray(initial)

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
        if off < 0 or off + len(raw) > len(self.data):
            raise ValueError("patch out of range")
        self.data[off:off + len(raw)] = raw


def build_sequence_windows(lengths: Sequence[int], gap: int = 3333) -> List[SequenceWindow]:
    timeline = 0
    out: List[SequenceWindow] = []
    for length in lengths:
        length = int(length)
        if length < 0:
            raise ValueError("negative animation length")
        timeline += int(gap)
        start = timeline
        timeline += length
        out.append(SequenceWindow(start, timeline))
    return out


def _pair(d: bytes, off: int) -> Tuple[int, int]:
    if off < 0 or off + 8 > len(d):
        raise ValueError("array pair out of range")
    return struct.unpack_from("<II", d, off)


def _source_track(d: bytes, off: int, key_size: int):
    if off < 0 or off + 20 > len(d):
        raise ValueError("source track header out of range")
    typ, seq, tn, to, kn, ko = struct.unpack_from("<hhIIII", d, off)
    if tn != kn:
        raise ValueError("source track outer times/keys mismatch")
    times: List[List[int]] = []
    keys: List[List[bytes]] = []
    for i in range(tn):
        tc, tp = _pair(d, to + i * 8)
        kc, kp = _pair(d, ko + i * 8)
        if tc != kc:
            raise ValueError("source track inner times/keys mismatch")
        if tc:
            if tp <= 0 or tp + tc * 4 > len(d):
                raise ValueError("source track timestamps out of range")
            ts = list(struct.unpack_from("<" + "I" * tc, d, tp))
        else:
            ts = []
        vs: List[bytes] = []
        if kc:
            if kp <= 0 or kp + kc * key_size > len(d):
                raise ValueError("source track keys out of range")
            vs = [d[kp + j * key_size:kp + (j + 1) * key_size] for j in range(kc)]
        times.append(ts)
        keys.append(vs)
    return typ, seq, times, keys


def _ranges(counts: Sequence[int]) -> List[Tuple[int, int]]:
    cursor = 0
    out: List[Tuple[int, int]] = []
    for n in counts:
        n = int(n)
        start = cursor
        if n <= 1:
            cursor += 1
        else:
            cursor += n - 1
        out.append((start, cursor))
        cursor += 1
    out.append((0, 0))
    return out


def _flatten(
    times: Sequence[Sequence[int]],
    keys: Sequence[Sequence[bytes]],
    windows: Sequence[SequenceWindow],
    default_key: bytes,
    global_sequence: bool,
):
    if len(times) != len(keys):
        raise ValueError("track outer mismatch")
    if not times:
        return [], [], []
    if global_sequence:
        if len(times) != 1:
            raise ValueError("global sequence track must have one outer group")
        if len(times[0]) != len(keys[0]):
            raise ValueError("global sequence inner mismatch")
        return [], list(times[0]), list(keys[0])
    if len(times) == len(windows):
        counts: List[int] = []
        out_t: List[int] = []
        out_k: List[bytes] = []
        for ts, vs, win in zip(times, keys, windows):
            if len(ts) != len(vs):
                raise ValueError("track inner mismatch")
            n = len(ts)
            counts.append(n)
            if n == 0:
                out_t.extend((win.start, win.end))
                out_k.extend((default_key, default_key))
            elif n == 1:
                t0 = int(ts[0])
                out_t.extend((win.start + t0, win.end + t0))
                out_k.extend((vs[0], vs[0]))
            else:
                out_t.extend(win.start + int(t) for t in ts)
                out_k.extend(vs)
        return _ranges(counts), out_t, out_k
    if len(times) == 1:
        if len(times[0]) != len(keys[0]):
            raise ValueError("single-group track mismatch")
        return [], list(times[0]), list(keys[0])
    raise ValueError(f"unsupported track outer count {len(times)} for {len(windows)} sequences")


def _write_track(builder: ByteBuilder, typ: int, seq: int, ranges, times, keys) -> bytes:
    rr = b"".join(struct.pack("<II", int(a), int(b)) for a, b in ranges)
    tr = b"".join(struct.pack("<I", int(t)) for t in times)
    kr = b"".join(keys)
    ro = builder.append(rr) if rr else 0
    to = builder.append(tr) if tr else 0
    ko = builder.append(kr) if kr else 0
    return struct.pack("<hhIIIIII", int(typ), int(seq), len(ranges), ro, len(times), to, len(keys), ko)


def _read_fake(d: bytes, off: int, key_size: int):
    """Read WotLK particle FakeAnimBlock: uint16 timestamps + fixed-size keys."""
    tn, to = _pair(d, off)
    kn, ko = _pair(d, off + 8)
    if tn != kn:
        raise ValueError("particle fake track time/key count mismatch")
    times: List[int] = []
    if tn:
        if to <= 0 or to + tn * 2 > len(d):
            raise ValueError("particle fake timestamps out of range")
        times = list(struct.unpack_from("<" + "H" * tn, d, to))
    keys: List[bytes] = []
    if kn:
        if ko <= 0 or ko + kn * key_size > len(d):
            raise ValueError("particle fake keys out of range")
        keys = [d[ko + i * key_size:ko + (i + 1) * key_size] for i in range(kn)]
    return times, keys


def _copy_byte_array(builder: ByteBuilder, d: bytes, count: int, off: int) -> Tuple[int, int]:
    count = int(count)
    if count == 0:
        return 0, 0
    if off <= 0 or off + count > len(d):
        raise ValueError("particle filename payload out of range")
    return count, builder.append(d[off:off + count])


def _float_to_u8(v: float) -> int:
    if not math.isfinite(v):
        return 0
    return max(0, min(255, int(v)))


def _alpha_i16_to_u8(v: int) -> int:
    return max(0, min(255, int(v) >> 7))


def _gradient_fields(source: bytes, so: int):
    color_t, color_k = _read_fake(source, so + 260, 12)
    _alpha_t, alpha_k = _read_fake(source, so + 276, 2)
    _size_t, size_k = _read_fake(source, so + 292, 8)
    _head_t, head_k = _read_fake(source, so + 316, 2)
    _tail_t, tail_k = _read_fake(source, so + 332, 2)

    midpoint = 0.0
    colors = bytearray(12)
    if len(color_k) == 3:
        if len(color_t) >= 2:
            midpoint = float(color_t[1]) / 32767.0
        for i, raw in enumerate(color_k[:3]):
            r, g, b = struct.unpack("<fff", raw)
            a = 0
            if i < len(alpha_k):
                a = _alpha_i16_to_u8(struct.unpack("<h", alpha_k[i])[0])
            colors[i * 4:i * 4 + 4] = bytes((_float_to_u8(b), _float_to_u8(g), _float_to_u8(r), a))

    sizes = [0.0, 0.0, 0.0]
    if len(size_k) == 3:
        sizes = [struct.unpack("<ff", raw)[0] for raw in size_k[:3]]

    heads = [struct.unpack("<h", raw)[0] for raw in head_k[:4]]
    tails = [struct.unpack("<h", raw)[0] for raw in tail_k[:4]]
    while len(heads) < 4:
        heads.append(0)
    while len(tails) < 4:
        tails.append(0)
    tiles = [heads[0], heads[1], 1, heads[2], heads[3], 1, tails[0], tails[1], tails[2], tails[3]]
    return midpoint, bytes(colors), sizes, tiles


def _loss_if_nonzero(report: ParticleConversionReport, emitter: int, code: str, source_off: int, raw: bytes):
    if any(raw):
        report.losses.append(ParticleLoss(emitter, code, source_off, raw.hex()))


def convert_particles(
    builder: ByteBuilder,
    source: bytes,
    source_count: int,
    source_offset: int,
    sequence_windows: Sequence[SequenceWindow],
    *,
    allow_lossy: bool = True,
) -> Tuple[int, int, ParticleConversionReport]:
    """Append Classic particle records and payloads.

    Returns `(count, target_record_offset, report)`.
    A non-zero source nUnknownReference/ofsUnknownReference is a hard blocker.
    """
    count = int(source_count)
    report = ParticleConversionReport(emitters=count)
    if count == 0:
        return 0, 0, report
    if source_offset <= 0 or source_offset + count * SOURCE_PARTICLE_STRIDE > len(source):
        raise ValueError("source particle record array out of range")

    target_offset = builder.reserve(count * TARGET_PARTICLE_STRIDE)
    source_track_offsets = (52, 72, 92, 112, 132, 152, 176, 200, 220, 240)

    for i in range(count):
        so = source_offset + i * SOURCE_PARTICLE_STRIDE
        to = target_offset + i * TARGET_PARTICLE_STRIDE
        rec = bytearray(TARGET_PARTICLE_STRIDE)

        unknown0 = struct.unpack_from("<i", source, so)[0]
        flags = struct.unpack_from("<I", source, so + 4)[0] & 0xFFFF
        struct.pack_into("<iI", rec, 0, unknown0, flags)
        rec[8:20] = source[so + 8:so + 20]
        rec[20:24] = source[so + 20:so + 24]

        mn, mo = _pair(source, so + 24)
        cn, co = _pair(source, so + 32)
        struct.pack_into("<II", rec, 24, *_copy_byte_array(builder, source, mn, mo))
        struct.pack_into("<II", rec, 32, *_copy_byte_array(builder, source, cn, co))

        rec[40] = source[so + 40]
        rec[41] = 0
        struct.pack_into("<H", rec, 42, source[so + 41])
        rec[44:52] = source[so + 44:so + 52]

        for ti, src_rel in enumerate(source_track_offsets):
            typ, seq, times, keys = _source_track(source, so + src_rel, 4)
            ranges, flat_times, flat_keys = _flatten(times, keys, sequence_windows, b"\0\0\0\0", global_sequence=(seq >= 0))
            rec[52 + ti * 28:52 + (ti + 1) * 28] = _write_track(builder, typ, seq, ranges, flat_times, flat_keys)

        midpoint, colors, sizes, tiles = _gradient_fields(source, so)
        struct.pack_into("<f", rec, 332, midpoint)
        rec[336:348] = colors
        struct.pack_into("<fff", rec, 348, *sizes)
        struct.pack_into("<10h", rec, 360, *tiles)

        rec[380:392] = source[so + 348:so + 360]
        rec[392:404] = source[so + 360:so + 372]
        rec[404:408] = source[so + 372:so + 376]
        rec[408:412] = source[so + 384:so + 388]
        struct.pack_into("<f", rec, 412, 0.0)
        for src_rel, dst_rel in ((396, 416), (408, 428), (420, 440)):
            vals = struct.unpack_from("<fff", source, so + src_rel)
            struct.pack_into("<fff", rec, dst_rel, *vals)
        vals4 = struct.unpack_from("<ffff", source, so + 432)
        struct.pack_into("<ffff", rec, 452, *vals4)

        nref, oref = struct.unpack_from("<II", source, so + 448)
        if nref or oref:
            raise ValueError(f"particle emitter {i} has unvalidated unknown reference pair {nref}/{oref}")
        struct.pack_into("<II", rec, 468, nref, oref)

        typ, seq, times, keys = _source_track(source, so + 456, 1)
        if not times:
            ranges, flat_times, flat_keys = [], [0], [b"\x01"]
        else:
            ranges, flat_times, flat_keys = _flatten(times, keys, sequence_windows, b"\x01", global_sequence=(seq >= 0))
        rec[476:504] = _write_track(builder, typ, seq, ranges, flat_times, flat_keys)

        _loss_if_nonzero(report, i, "DROP_SOURCE_PARTICLE_COLOR_INDEX", so + 42, source[so + 42:so + 44])
        _loss_if_nonzero(report, i, "DROP_WOTLK_UNKNOWN1", so + 172, source[so + 172:so + 176])
        _loss_if_nonzero(report, i, "DROP_WOTLK_UNKNOWN2", so + 196, source[so + 196:so + 200])
        _loss_if_nonzero(report, i, "DROP_WOTLK_SCALE_VARY", so + 308, source[so + 308:so + 316])
        _loss_if_nonzero(report, i, "DROP_WOTLK_UNKNOWN3", so + 376, source[so + 376:so + 384])
        _loss_if_nonzero(report, i, "DROP_WOTLK_UNKNOWN4", so + 388, source[so + 388:so + 396])

        builder.patch(to, bytes(rec))

    if report.lossy and not allow_lossy:
        codes = sorted({x.code for x in report.losses})
        raise ValueError("particle conversion would be lossy: " + ", ".join(codes))
    return count, target_offset, report
