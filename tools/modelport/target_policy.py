# -*- coding: utf-8 -*-
"""Target-native / Golden-Reference policy for Turtle/custom 1.18.1."""
from dataclasses import dataclass

@dataclass(frozen=True)
class TargetPolicy:
    m2_magic: bytes = b"MD20"
    m2_version: int = 256
    m2_header_size: int = 324
    classic_bone_size: int = 108
    classic_sequence_size: int = 68
    classic_animation_block_size: int = 28
    classic_submesh_size: int = 32
    classic_texunit_size: int = 24
    playable_record_size: int = 4
    playable_count: int = 226
    force_16_byte_alignment: bool = False
    embedded_views: bool = True

    # Golden Reference V3
    generate_animation_lookup: bool = True
    animation_lookup_missing_value: int = 0xFFFF
    preserve_sequence_index: bool = True
    quaternion_minus_one_exact: float = 1.0
    playable_is_model_aware: bool = True

POLICY = TargetPolicy()
