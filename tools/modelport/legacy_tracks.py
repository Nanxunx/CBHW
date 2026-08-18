# -*- coding: utf-8 -*-
"""Neutral legacy-animation algorithms for 3.3.5a -> Classic v256.

These functions encode behavioral rules cross-checked against historical
converters and successful Golden Reference outputs. They do not read/write any
private format and have no dependency on reference DLLs.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import Sequence, TypeVar

T = TypeVar("T")


@dataclass(frozen=True)
class ClassicSequenceWindow:
    time_start: int
    time_end: int


@dataclass(frozen=True)
class Range:
    start: int
    end: int


def comp_quat_short_to_float(v: int) -> float:
    """Convert signed 16-bit WotLK compressed quaternion component.

    -1 is an endpoint and maps to exactly +1.0.
    """
    if not -32768 <= v <= 32767:
        raise ValueError("component must fit int16")
    if v == -1:
        return 1.0
    if v <= 0:
        return (v + 32767) / 32767.0
    return (v - 32767) / 32767.0


def comp_quat_to_float4(q: Sequence[int]) -> tuple[float, float, float, float]:
    if len(q) != 4:
        raise ValueError("quaternion requires 4 components")
    return tuple(comp_quat_short_to_float(int(x)) for x in q)  # type: ignore


def build_classic_sequence_windows(
    lengths: Sequence[int],
    gap: int = 3333,
) -> list[ClassicSequenceWindow]:
    timeline = 0
    out = []
    for length in lengths:
        if length < 0:
            raise ValueError("negative animation length")
        timeline += gap
        start = timeline
        timeline += int(length)
        out.append(ClassicSequenceWindow(start, timeline))
    return out


def old_value_ranges(per_sequence_counts: Sequence[int]) -> list[Range]:
    """Classic generic value-track ranges with trailing Range(0,0)."""
    cursor = 0
    out = []
    for count in per_sequence_counts:
        if count < 0:
            raise ValueError("negative key count")
        start = cursor
        if count == 1:
            cursor += 1
        elif count > 1:
            cursor += count - 1
        else:
            cursor += 1
        out.append(Range(start, cursor))
        cursor += 1
    out.append(Range(0, 0))
    return out


def flatten_value_track(
    timestamps: Sequence[Sequence[int]],
    values: Sequence[Sequence[T]],
    windows: Sequence[ClassicSequenceWindow],
    default_value: T,
    global_sequence: bool = False,
) -> tuple[list[Range], list[int], list[T]]:
    if len(timestamps) != len(values):
        raise ValueError("outer times/values mismatch")
    if global_sequence:
        if not timestamps:
            return [], [], []
        return [], list(timestamps[0]), list(values[0])
    if len(timestamps) == len(windows):
        counts = []
        out_t = []
        out_v = []
        for ts, vs, w in zip(timestamps, values, windows):
            if len(ts) != len(vs):
                raise ValueError("inner times/values mismatch")
            n = len(ts)
            counts.append(n)
            if n == 0:
                out_t += [w.time_start, w.time_end]
                out_v += [default_value, default_value]
            elif n == 1:
                out_t += [w.time_start + int(ts[0]), w.time_end + int(ts[0])]
                out_v += [vs[0], vs[0]]
            else:
                out_t += [w.time_start + int(t) for t in ts]
                out_v += list(vs)
        return old_value_ranges(counts), out_t, out_v
    if len(timestamps) == 1:
        if len(timestamps[0]) != len(values[0]):
            raise ValueError("inner mismatch")
        return [], list(timestamps[0]), list(values[0])
    raise ValueError("legacy track outer count is neither sequence count nor 1")


def event_ranges(per_sequence_counts: Sequence[int]) -> list[Range]:
    """Generate legacy event ranges; events have timestamps but no value keys."""
    cursor = 0
    out = []
    for n in per_sequence_counts:
        if n < 0:
            raise ValueError("negative event count")
        start = cursor
        cursor += n
        out.append(Range(start, cursor))
    out.append(Range(0, 0))
    return out


def flatten_event_track(
    timestamps: Sequence[Sequence[int]],
    windows: Sequence[ClassicSequenceWindow],
    global_sequence: bool = False,
) -> tuple[list[Range], list[int]]:
    if global_sequence:
        return [], list(timestamps[0]) if timestamps else []
    if len(timestamps) == len(windows):
        out = []
        counts = []
        for ts, w in zip(timestamps, windows):
            counts.append(len(ts))
            out.extend(w.time_start + int(t) for t in ts)
        return event_ranges(counts), out
    if len(timestamps) == 1:
        return [], list(timestamps[0])
    raise ValueError("legacy event outer count is neither sequence count nor 1")
