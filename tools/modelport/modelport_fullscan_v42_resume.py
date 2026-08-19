#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Checkpoint/resume wrapper for modelport_fullscan_v41.

V4.1 kept all scan rows in memory and only wrote the final CSV at the end.
If Windows hibernate/restart killed Python, already scanned rows were lost.

V4.2 writes one JSONL checkpoint row per completed pair and automatically
resumes from completed RelativePath entries on the next run. Use --fresh only
when intentionally restarting from zero.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from pathlib import Path

from modelport_fullscan_v41 import (
    CSV_FIELDS,
    compare_pair,
    pack_selected,
    write_csv,
)


def load_checkpoint(path: Path) -> dict[str, dict]:
    rows: dict[str, dict] = {}
    if not path.exists():
        return rows
    with path.open("r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                # A hard power transition can truncate only the last write.
                print(f"[resume] ignore truncated checkpoint line {line_no}", file=sys.stderr)
                continue
            rel = str(row.get("RelativePath", "")).lower()
            if rel:
                rows[rel] = row
    return rows


def append_checkpoint(handle, row: dict, ordinal: int) -> None:
    handle.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")
    handle.flush()
    if ordinal % 25 == 0:
        os.fsync(handle.fileno())


def scan(source_root: Path, target_root: Path, out_root: Path, progress_every: int = 250):
    source_files = [p for p in source_root.rglob("*") if p.is_file() and p.suffix.lower() == ".m2"]
    pairs = []
    missing = 0
    for p in source_files:
        rel = str(p.relative_to(source_root))
        t = target_root / Path(rel)
        if t.exists():
            pairs.append((p, t, rel))
        else:
            missing += 1
    pairs.sort(key=lambda x: x[2].lower())
    if not pairs:
        raise RuntimeError("no same-path M2 pairs found")

    pre = compare_pair(*pairs[0])
    if pre["ConversionClass"] == "SCAN_ERROR":
        raise RuntimeError(
            "preflight pair failed before full scan: "
            f"{pre['RelativePath']} -> {pre['ErrorMessage']}"
        )

    meta = out_root / "STAGING" / "00_Metadata"
    meta.mkdir(parents=True, exist_ok=True)
    checkpoint = meta / "M2_FeatureIndex_335_112_V42.checkpoint.jsonl"

    completed = load_checkpoint(checkpoint)
    if completed:
        print(f"[resume] checkpoint rows: {len(completed)}/{len(pairs)}", flush=True)

    valid_keys = {rel.lower() for _, _, rel in pairs}
    completed = {k: v for k, v in completed.items() if k in valid_keys}

    newly_scanned = 0
    with checkpoint.open("a", encoding="utf-8", newline="\n") as cp:
        for idx, (s, t, rel) in enumerate(pairs, 1):
            key = rel.lower()
            if key in completed:
                if progress_every and idx % progress_every == 0:
                    print(f"[scan] {idx}/{len(pairs)} (resume)", flush=True)
                continue

            row = compare_pair(s, t, rel)
            completed[key] = row
            newly_scanned += 1
            append_checkpoint(cp, row, newly_scanned)

            if progress_every and idx % progress_every == 0:
                print(f"[scan] {idx}/{len(pairs)}", flush=True)

        cp.flush()
        os.fsync(cp.fileno())

    rows = [completed[rel.lower()] for _, _, rel in pairs if rel.lower() in completed]
    if len(rows) != len(pairs):
        raise RuntimeError(f"checkpoint incomplete after scan: rows={len(rows)} pairs={len(pairs)}")

    write_csv(meta / "M2_FeatureIndex_335_112_V42.csv", rows, CSV_FIELDS)

    counts = {
        "source_m2_total": len(source_files),
        "paired_m2": len(pairs),
        "missing_target_pair": missing,
        "checkpoint_rows_loaded": len(rows) - newly_scanned,
        "newly_scanned_this_run": newly_scanned,
        "scan_errors": sum(r["ConversionClass"] == "SCAN_ERROR" for r in rows),
        "sequence_metadata_mismatch": sum(not r["SequenceMetadataMatch"] and r["ConversionClass"] != "SCAN_ERROR" for r in rows),
        "timeline_mismatch": sum(not r["TimelineRulePass"] and r["ConversionClass"] != "SCAN_ERROR" for r in rows),
        "animation_lookup_mismatch": sum(not r["AnimationLookupRulePass"] and r["ConversionClass"] != "SCAN_ERROR" for r in rows),
        "playable_v4_mismatch": sum(not r["PlayableV4RulePass"] and r["ConversionClass"] != "SCAN_ERROR" for r in rows),
        "true_ribbon_models": sum(r["Ribbons"] > 0 for r in rows),
        "particle_models": sum(r["Particles"] > 0 for r in rows),
        "texanim_models": sum(r["TexAnims"] > 0 for r in rows),
        "external_anim_models": sum(r["ExternalAnimFiles"] > 0 for r in rows),
        "alias_models": sum(r["AliasSequences"] > 0 for r in rows),
        "subanimation_models": sum(r["SubAnimationSequences"] > 0 for r in rows),
    }

    (meta / "M2_Scan_Summary_V42.json").write_text(
        json.dumps(counts, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    (meta / "M2_Scan_Summary_V42.txt").write_text(
        "\n".join([
            "模型移植 Golden Reference V4.2 可恢复全库扫描",
            "",
            f"335 M2 总数：{counts['source_m2_total']}",
            f"存在成功112同路径 pair：{counts['paired_m2']}",
            f"无112同路径：{counts['missing_target_pair']}",
            f"从checkpoint恢复：{counts['checkpoint_rows_loaded']}",
            f"本次新扫描：{counts['newly_scanned_this_run']}",
            f"扫描错误：{counts['scan_errors']}",
            "",
            f"Sequence metadata mismatch：{counts['sequence_metadata_mismatch']}",
            f"Timeline mismatch：{counts['timeline_mismatch']}",
            f"AnimationLookup V4 mismatch：{counts['animation_lookup_mismatch']}",
            f"Playable V4 mismatch：{counts['playable_v4_mismatch']}",
            "",
            f"真实 RibbonEmitter>0：{counts['true_ribbon_models']}",
            f"Particle>0：{counts['particle_models']}",
            f"TexAnim>0：{counts['texanim_models']}",
            f"External .anim sidecar：{counts['external_anim_models']}",
            f"Alias sequence(flags&0x40)>0：{counts['alias_models']}",
            f"SubAnimationID>0：{counts['subanimation_models']}",
            "",
            "中断后重新运行同一命令即可续扫；不要使用 --fresh。",
        ]),
        encoding="utf-8-sig",
    )
    return rows, counts


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", default=r"E:\335_FinalExtract_V5")
    ap.add_argument("--target", default=r"E:\335to112_Converted_FinalExtract_V1")
    ap.add_argument("--out", default=r"E:\ModelPort_GoldenUpload_ThirdBatch_V42")
    ap.add_argument("--no-pack", action="store_true")
    ap.add_argument("--fresh", action="store_true", help="delete checkpoint/output and restart from zero")
    args = ap.parse_args()

    source = Path(args.source)
    target = Path(args.target)
    out = Path(args.out)
    if not source.exists():
        raise SystemExit(f"source does not exist: {source}")
    if not target.exists():
        raise SystemExit(f"target does not exist: {target}")

    if args.fresh and out.exists():
        print(f"[fresh] deleting previous checkpoint/output: {out}", flush=True)
        shutil.rmtree(out)
    out.mkdir(parents=True, exist_ok=True)

    rows, counts = scan(source, target, out)
    print(json.dumps(counts, ensure_ascii=False, indent=2))
    if counts["scan_errors"]:
        print("[WARN] scan_errors > 0; inspect ErrorMessage before trusting global rules.", file=sys.stderr)
    if not args.no_pack:
        z = pack_selected(source, target, out, rows)
        # V4.1 packer names the zip V41. Rename it for the resumable run.
        wanted = out / "ModelPort_GoldenReference_ThirdBatch_V42_ALL.zip"
        if z != wanted:
            if wanted.exists():
                wanted.unlink()
            z.replace(wanted)
            z = wanted
        print(f"[ZIP] {z}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
