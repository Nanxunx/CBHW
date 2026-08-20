#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DEPRECATED: use validate_animation_metadata_v4.py.

V3's Index==physical-sequence assumption was disproved by the second Golden
Reference batch. This entry point intentionally reports invalid rather than
silently approving or repairing a model with guessed Sequence.Index values.
"""
import json
import sys


def validate(*args, **kwargs):
    return {
        "valid": False,
        "issues": [
            "V3 validator deprecated: Sequence.Index must be compared with the original v264 source; use V4"
        ],
    }


if __name__ == "__main__":
    print(json.dumps(validate(), ensure_ascii=False, indent=2))
    sys.exit(2)
