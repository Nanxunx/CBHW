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
    particle_source_size: int = 476
    particle_target_size: int = 504
    ribbon_source_size: int = 176
    ribbon_target_size: int = 220
    playable_record_size: int = 4
    playable_count: int = 226
    force_16_byte_alignment: bool = False
    embedded_views: bool = True

    # Golden Reference V4
    preserve_source_sequence_index: bool = True
    animation_lookup_uses_first_physical_sequence: bool = True
    generate_animation_lookup: bool = True
    animation_lookup_missing_value: int = 0xFFFF
    playable_is_model_aware: bool = True
    playable_uses_build12340_animationdata_fallback: bool = True
    quaternion_minus_one_exact: float = 1.0
    external_anim_copy_observed_safe: bool = True
    multi_view_embed_all_profiles: bool = True
    particle_full_writer_enabled: bool = False
    ribbon_full_writer_enabled: bool = False


POLICY = TargetPolicy()
