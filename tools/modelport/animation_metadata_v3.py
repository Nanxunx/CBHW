#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DEPRECATED Golden Reference V3 animation helper.

V3 is intentionally fail-closed.

Second-batch Golden References disproved the V3 assumption that
M2Sequence.Index equals the physical sequence position. Successful targets
preserve the WotLK source Index exactly; AnimationLookup independently maps
AnimationID to the first PHYSICAL sequence index.

Use animation_metadata_v4.py + playable_lookup_v4.py instead.
"""
from __future__ import annotations
import struct
from pathlib import Path
from typing import Sequence

PLAYABLE_COUNT = 226


def build_animation_lookup(animation_ids: Sequence[int]) -> bytes:
    """Still-valid helper: IDs are supplied in PHYSICAL sequence order."""
    if not animation_ids:
        return b""
    out = [0xFFFF] * (max(animation_ids) + 1)
    for physical_index, anim_id in enumerate(animation_ids):
        if out[anim_id] == 0xFFFF:
            out[anim_id] = physical_index
    return struct.pack("<" + "H" * len(out), *out)


def build_model_aware_playable(*args, **kwargs):
    raise RuntimeError(
        "V3 playable generation is incomplete. Use playable_lookup_v4.py with build12340 AnimationData.dbc."
    )


def repair_v256_animation_metadata(*args, **kwargs):
    raise RuntimeError(
        "V3 repair is disabled because it can destroy/guess M2Sequence.Index. "
        "Use animation_metadata_v4.repair_v256_animation_metadata and provide the original v264 source M2."
    )
