#!/usr/bin/env python3
import struct
from playable_lookup_v4 import build_playable_records
from animation_metadata_v4 import build_animation_lookup

seqs = [
    {"animation_id": 53, "physical_index": 3, "index": 4},
    {"animation_id": 54, "physical_index": 4, "index": 3},
    {"animation_id": 0,  "physical_index": 5, "index": 5},
    {"animation_id": 0,  "physical_index": 11, "index": 11},
]
lookup = build_animation_lookup(seqs)
vals = struct.unpack("<55H", lookup)
assert vals[0] == 5
assert vals[53] == 3
assert vals[54] == 4
assert seqs[0]["index"] == 4

fallbacks = {i: 0 for i in range(226)}
fallbacks.update({172: 16, 174: 16, 181: 16, 191: 159})
records = build_playable_records({16, 159}, fallbacks)
assert records[172] == (16, 0)
assert records[174] == (16, 0)
assert records[181] == (16, 0)
assert records[191] == (159, 0)

print("PASS")
