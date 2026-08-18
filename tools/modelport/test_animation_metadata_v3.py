import struct
from animation_metadata_v3 import build_animation_lookup, build_model_aware_playable

lk = build_animation_lookup([0, 160, 161])
assert len(lk) == 162 * 2
vals = struct.unpack("<162H", lk)
assert vals[0] == 0
assert vals[160] == 1
assert vals[161] == 2
assert vals[1] == 0xFFFF

base = b"\0" * (226 * 4)
p = build_model_aware_playable(base, [(0, 0, 0), (160, 0, 1), (161, 0, 2)])
assert struct.unpack_from("<HH", p, 160 * 4) == (160, 0)
assert struct.unpack_from("<HH", p, 161 * 4) == (161, 0)

print("PASS")
