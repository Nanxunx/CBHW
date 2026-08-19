#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Targeted V4.5 refinement after a V4.4 scan.

This does NOT rescan the entire corpus.
It reads V44_DeepValidation.csv and re-checks only rows that failed the old
Playable V4 rule. It also packages the tiny Sequence/Timeline exception set
for the next Golden diff pass.
"""
from __future__ import annotations

import argparse
import csv
import json
import shutil
import struct
import zipfile
from pathlib import Path
from typing import Dict, List, Tuple

from playable_lookup_v45 import PLAYABLE_COUNT, build_playable_records, graph_from_build12340_animation_data


def b(v: str) -> bool:
    return str(v).strip().lower() in {"1", "true", "yes"}


def read_csv(path: Path) -> List[dict]:
    with path.open("r", encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: List[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields: List[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    with path.open("w", encoding="utf-8-sig", newline="") as f:
        if not fields:
            return
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)


def parse_source_anim_ids(path: Path) -> List[int]:
    d = path.read_bytes()
    if len(d) < 304 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 264:
        raise ValueError("source is not MD20 v264")
    count, off = struct.unpack_from("<II", d, 28)
    out = []
    for i in range(count):
        p = off + i * 64
        if p + 64 > len(d):
            raise ValueError("source sequence table truncated")
        out.append(struct.unpack_from("<H", d, p)[0])
    return out


def read_target_playable(path: Path) -> List[Tuple[int, int]]:
    d = path.read_bytes()
    if len(d) < 324 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 256:
        raise ValueError("target is not MD20 v256")
    count, off = struct.unpack_from("<II", d, 44)
    if count != PLAYABLE_COUNT or off + count * 4 > len(d):
        raise ValueError(f"invalid target PlayableAnimationLookup count={count}")
    return [struct.unpack_from("<hh", d, off + i * 4) for i in range(count)]


def copy_family(root: Path, relative: str, dest: Path) -> None:
    src = root / Path(relative)
    if not src.exists():
        return
    out = dest / Path(relative)
    out.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, out)
    stem = src.stem
    for pattern in (stem + "*.skin", stem + "*.anim", stem + "*.blp"):
        for p in src.parent.glob(pattern):
            if p.is_file():
                rel = p.relative_to(root)
                q = dest / rel
                q.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(p, q)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", default=r"E:\335_FinalExtract_V5")
    ap.add_argument("--target", default=r"E:\335to112_Converted_FinalExtract_V1")
    ap.add_argument("--v44-deep", default=r"E:\ModelPort_GoldenUpload_Targeted_V44\STAGING\00_Metadata\V44_DeepValidation.csv")
    ap.add_argument("--animation-data", default=r"E:\335_FinalExtract_V5\DBFilesClient\AnimationData.dbc")
    ap.add_argument("--out", default=r"E:\ModelPort_GoldenUpload_V45_Refine")
    args = ap.parse_args()

    source = Path(args.source)
    target = Path(args.target)
    rows = read_csv(Path(args.v44_deep))
    graph = graph_from_build12340_animation_data(Path(args.animation_data))
    out = Path(args.out)
    meta = out / "STAGING" / "00_Metadata"
    meta.mkdir(parents=True, exist_ok=True)

    old_playable_fail = [r for r in rows if not b(r.get("PlayableV4RulePass", "false"))]
    seq_exception = [r for r in rows if (not b(r.get("SequenceMetadataMatch", "false"))) or (not b(r.get("TimelineRulePass", "false")))]

    validation = []
    mismatch_ids: Dict[int, int] = {}
    for i, row in enumerate(old_playable_fail, 1):
        rel = row["RelativePath"]
        rec = {"RelativePath": rel, "V45Pass": False, "MismatchIDs": "", "Error": ""}
        try:
            expected = build_playable_records(parse_source_anim_ids(source / Path(rel)), graph)
            actual = read_target_playable(target / Path(rel))
            diffs = [j for j in range(PLAYABLE_COUNT) if expected[j] != actual[j]]
            rec["V45Pass"] = not diffs
            rec["MismatchIDs"] = ",".join(str(j) for j in diffs)
            for j in diffs:
                mismatch_ids[j] = mismatch_ids.get(j, 0) + 1
        except Exception as e:
            rec["Error"] = f"{type(e).__name__}: {e}"
        validation.append(rec)
        if i % 100 == 0:
            print(f"[playable-v45] {i}/{len(old_playable_fail)}", flush=True)

    write_csv(meta / "Playable_V45_Validation.csv", validation)
    write_csv(meta / "Playable_V45_MismatchID_Frequency.csv", [{"AnimationID": k, "ModelCount": v} for k, v in sorted(mismatch_ids.items(), key=lambda x: (-x[1], x[0]))])
    write_csv(meta / "Sequence_Timeline_Exceptions.csv", seq_exception)

    for idx, row in enumerate(seq_exception, 1):
        rel = row["RelativePath"]
        base = out / "STAGING" / "01_SequenceTimelineExceptions" / f"{idx:02d}_{Path(rel).stem.replace(' ', '_')}"
        copy_family(source, rel, base / "335")
        copy_family(target, rel, base / "112")

    survivors = [r for r in validation if not bool(r["V45Pass"])]
    for idx, row in enumerate(survivors[:12], 1):
        rel = row["RelativePath"]
        base = out / "STAGING" / "02_PlayableV45Remaining" / f"{idx:02d}_{Path(rel).stem.replace(' ', '_')}"
        copy_family(source, rel, base / "335")
        copy_family(target, rel, base / "112")

    summary = {
        "v44_playable_fail_models": len(old_playable_fail),
        "v45_pass_models": sum(bool(r["V45Pass"]) for r in validation),
        "v45_remaining_fail_models": len(survivors),
        "v45_errors": sum(bool(r["Error"]) for r in validation),
        "sequence_or_timeline_exception_models": len(seq_exception),
        "remaining_mismatch_ids": mismatch_ids,
    }
    (meta / "V45_Refine_Summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    (meta / "V45_Refine_Summary.txt").write_text("\n".join(f"{k}: {v}" for k, v in summary.items()), encoding="utf-8-sig")

    zip_path = out / "ModelPort_GoldenReference_V45_Refine_ALL.zip"
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for p in (out / "STAGING").rglob("*"):
            if p.is_file():
                z.write(p, p.relative_to(out / "STAGING"))
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    print(f"[ZIP] {zip_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
