from ribbon_converter_v45 import WOTLK_RIBBON_SIZE, CLASSIC_RIBBON_SIZE, TRACK_SPECS

assert WOTLK_RIBBON_SIZE == 176
assert CLASSIC_RIBBON_SIZE == 220
assert len(TRACK_SPECS) == 6
assert [x[0] for x in TRACK_SPECS] == [
    "color",
    "alpha",
    "height_above",
    "height_below",
    "tex_slot",
    "visibility",
]
print("PASS")
