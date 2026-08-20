#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import json
import tempfile
from pathlib import Path

from modelport_fullscan_v42_resume import load_checkpoint


def test_checkpoint_ignores_truncated_tail():
    root = Path(tempfile.mkdtemp())
    cp = root / "scan.jsonl"
    cp.write_text(
        '{"RelativePath":"A\\\\B.m2","ConversionClass":"PASS"}\n'
        '{"RelativePath":"C\\\\D.m2","ConversionClass":"PASS"}\n'
        '{"RelativePath":"TRUNCATED"',
        encoding="utf-8",
    )
    rows = load_checkpoint(cp)
    assert len(rows) == 2


def test_checkpoint_last_duplicate_wins():
    root = Path(tempfile.mkdtemp())
    cp = root / "scan.jsonl"
    cp.write_text(
        json.dumps({"RelativePath":"A\\B.m2","ConversionClass":"OLD"}) + "\n" +
        json.dumps({"RelativePath":"A\\B.m2","ConversionClass":"PASS"}) + "\n",
        encoding="utf-8",
    )
    rows = load_checkpoint(cp)
    assert rows[r"a\b.m2"]["ConversionClass"] == "PASS"


if __name__ == "__main__":
    test_checkpoint_ignores_truncated_tail()
    test_checkpoint_last_duplicate_wins()
    print("PASS")
