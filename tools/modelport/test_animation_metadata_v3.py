#!/usr/bin/env python3
"""V3 is deprecated; production assertions live in test_animation_metadata_v4.py."""
from animation_metadata_v3 import repair_v256_animation_metadata

try:
    repair_v256_animation_metadata(None, None, None)
except RuntimeError as exc:
    assert "V3 repair is disabled" in str(exc)
else:
    raise AssertionError("V3 repair must fail closed")

print("PASS: V3 repair is fail-closed; use V4")
