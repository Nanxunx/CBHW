#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Small no-fixture regression tests for modelport_fullscan_v41."""
from modelport_fullscan_v41 import (
    Seq, build_animation_lookup, expected_playable, resolve_playable
)


def test_animation_lookup_duplicate_prefers_sub0():
    seqs = [
        Seq(0, 16, 1, 100, 0, 0, 0, 0, 0, 0, 0, 0, 7),
        Seq(1, 16, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 9),
        Seq(2, 60, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 3),
    ]
    lookup = build_animation_lookup(seqs)
    assert lookup[16] == 1
    assert lookup[60] == 2


def test_playable_crossbow_ids():
    seqs = [
        Seq(0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        Seq(1, 160, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 1),
        Seq(2, 161, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 2),
    ]
    p = expected_playable(build_animation_lookup(seqs))
    assert len(p) == 226
    assert p[160] == (160, 0)
    assert p[161] == (161, 0)


def test_golden_extension_edges():
    lookup = [-1] * 192
    for aid in (0, 16, 30, 159):
        lookup[aid] = aid
    assert resolve_playable(170, lookup) == 16
    assert resolve_playable(175, lookup) == 30
    assert resolve_playable(191, lookup) == 159


if __name__ == "__main__":
    test_animation_lookup_duplicate_prefers_sub0()
    test_playable_crossbow_ids()
    test_golden_extension_edges()
    print("PASS")
