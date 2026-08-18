#!/usr/bin/env python3
import struct
from playable_lookup_v4 import build_playable_records
from animation_metadata_v4 import build_animation_lookup

# Duplicate AnimationID must prefer SubAnimationID==0 even when it is not the
# first physical occurrence.
seqs = [
    {"animation_id": 0,  "sub_animation_id": 1, "physical_index": 0, "index": 9},
    {"animation_id": 0,  "sub_animation_id": 0, "physical_index": 1, "index": 3},
    {"animation_id": 53, "sub_animation_id": 0, "physical_index": 2, "index": 4},
    {"animation_id": 54, "sub_animation_id": 0, "physical_index": 3, "index": 2},
]
lookup = build_animation_lookup(seqs)
vals = struct.unpack("<55H", lookup)
assert vals[0] == 1
assert vals[53] == 2
assert vals[54] == 3
# Sequence.Index is source metadata, not physical position.
assert seqs[0]["index"] == 9
assert seqs[1]["index"] == 3

# Second-batch Golden fallback extension edges.
present = {0, 4, 16, 25, 30, 159}
records = build_playable_records(present)
for requested in (170,171,172,173,174,176,178,179,181):
    assert records[requested] == (16, 0), (requested, records[requested])
assert records[175] == (30, 0)
assert records[191] == (159, 0)

# Historical playback flags retained when a fallback occurs.
only_stand = build_playable_records({0})
assert only_stand[6][1] == 3
assert only_stand[13][1] == 1

print("PASS")
