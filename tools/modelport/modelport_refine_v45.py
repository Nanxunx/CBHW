#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Targeted V4.5 refinement after a V4.4 scan.

No full-corpus rescan is performed. The tool opens only V4.4 rows that failed
old Playable V4 and/or Sequence/Timeline validation. Besides pass/fail counts,
it writes exact requested-ID Playable differences and exact Sequence fields,
so the next Golden rule revision can be evidence-driven without another large
model upload.
"""
from __future__ import annotations

import argparse
import csv
import json
import shutil
import struct
import zipfile
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Tuple

from playable_lookup_v45 import (
    PLAYABLE_COUNT,
    build_playable_records,
    graph_from_build12340_animation_data,
)


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


def _pair(d: bytes, off: int) -> Tuple[int, int]:
    if off < 0 or off + 8 > len(d):
        raise ValueError(f"array pair outside file at {off}")
    return struct.unpack_from("<II", d, off)


def parse_source_sequences(path: Path) -> List[dict]:
    d = path.read_bytes()
    if len(d) < 304 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 264:
        raise ValueError("source is not MD20 v264")
    count, off = _pair(d, 0x1C)
    out=[]
    for pos in range(count):
        p=off+pos*64
        if p+64>len(d): raise ValueError(f"source sequence {pos} truncated")
        anim,sub,length=struct.unpack_from("<HHI",d,p)
        out.append({
            "pos":pos,"anim":anim,"sub":sub,"length":length,
            "move_bits":struct.unpack_from("<I",d,p+8)[0],
            "flags":struct.unpack_from("<I",d,p+12)[0],
            "prob":struct.unpack_from("<H",d,p+16)[0],
            "unused":struct.unpack_from("<H",d,p+18)[0],
            "d1":struct.unpack_from("<I",d,p+20)[0],
            "d2":struct.unpack_from("<I",d,p+24)[0],
            "play":struct.unpack_from("<I",d,p+28)[0],
            "bounds":d[p+32:p+60],
            "next":struct.unpack_from("<h",d,p+60)[0],
            "index":struct.unpack_from("<H",d,p+62)[0],
        })
    return out


def parse_target_sequences(path: Path) -> List[dict]:
    d = path.read_bytes()
    if len(d) < 324 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 256:
        raise ValueError("target is not MD20 v256")
    count, off = _pair(d, 0x1C)
    out=[]
    for pos in range(count):
        p=off+pos*68
        if p+68>len(d): raise ValueError(f"target sequence {pos} truncated")
        anim,sub,start,end=struct.unpack_from("<HHII",d,p)
        out.append({
            "pos":pos,"anim":anim,"sub":sub,"length":end-start,
            "start":start,"end":end,
            "move_bits":struct.unpack_from("<I",d,p+12)[0],
            "flags":struct.unpack_from("<I",d,p+16)[0],
            "prob":struct.unpack_from("<H",d,p+20)[0],
            "unused":struct.unpack_from("<H",d,p+22)[0],
            "d1":struct.unpack_from("<I",d,p+24)[0],
            "d2":struct.unpack_from("<I",d,p+28)[0],
            "play":struct.unpack_from("<I",d,p+32)[0],
            "bounds":d[p+36:p+64],
            "next":struct.unpack_from("<h",d,p+64)[0],
            "index":struct.unpack_from("<H",d,p+66)[0],
        })
    return out


def read_target_playable(path: Path) -> List[Tuple[int, int]]:
    d = path.read_bytes()
    if len(d) < 324 or d[:4] != b"MD20" or struct.unpack_from("<I", d, 4)[0] != 256:
        raise ValueError("target is not MD20 v256")
    count, off = _pair(d, 0x2C)
    if count != PLAYABLE_COUNT or off + count * 4 > len(d):
        raise ValueError(f"invalid target PlayableAnimationLookup count={count}")
    return [struct.unpack_from("<hh", d, off + i * 4) for i in range(count)]


def compare_sequence_details(source: List[dict], target: List[dict]) -> List[dict]:
    if len(source)!=len(target):
        return [{"SequencePosition":-1,"Fields":"sequence_count","SourceValue":len(source),"TargetValue":len(target)}]
    rows=[]
    timeline=0
    fields=("anim","sub","length","move_bits","flags","prob","unused","d1","d2","play","bounds","next","index")
    for a,t in zip(source,target):
        differing=[name for name in fields if a[name]!=t[name]]
        timeline += 3333
        expected_start=timeline
        expected_end=expected_start+a["length"]
        timeline=expected_end
        if t["start"]!=expected_start: differing.append("start")
        if t["end"]!=expected_end: differing.append("end")
        if differing:
            rows.append({
                "SequencePosition":a["pos"],
                "AnimationID":a["anim"],
                "SubAnimationID":a["sub"],
                "Fields":";".join(differing),
                "SourceIndex":a["index"],
                "TargetIndex":t["index"],
                "SourceFlags":f"0x{a['flags']:X}",
                "TargetFlags":f"0x{t['flags']:X}",
                "ExpectedStart":expected_start,
                "TargetStart":t["start"],
                "ExpectedEnd":expected_end,
                "TargetEnd":t["end"],
            })
    return rows


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

    source=Path(args.source); target=Path(args.target); deep=Path(args.v44_deep); anim_dbc=Path(args.animation_data); out=Path(args.out)
    for p,label in ((source,"source"),(target,"target"),(deep,"V44 DeepValidation"),(anim_dbc,"AnimationData.dbc")):
        if not p.exists(): raise SystemExit(f"missing {label}: {p}")

    rows=read_csv(deep)
    graph=graph_from_build12340_animation_data(anim_dbc)
    meta=out/"STAGING"/"00_Metadata"; meta.mkdir(parents=True,exist_ok=True)

    old_playable_fail=[r for r in rows if not b(r.get("PlayableV4RulePass","false"))]
    seq_exception=[r for r in rows if (not b(r.get("SequenceMetadataMatch","false"))) or (not b(r.get("TimelineRulePass","false")))]
    old_playable_paths={r["RelativePath"] for r in old_playable_fail}
    selected_paths=old_playable_paths | {r["RelativePath"] for r in seq_exception}

    model_results=[]; playable_detail=[]; seq_detail=[]; errors=[]
    distribution: Dict[int, Counter] = defaultdict(Counter)

    for idx,rel in enumerate(sorted(selected_paths),1):
        rec={"RelativePath":rel,"V45PlayablePass":True,"V45PlayableMismatchCount":0,"V45PlayableMismatchIDs":"","SequenceDetailCount":0,"Error":""}
        try:
            ss=parse_source_sequences(source/Path(rel)); ts=parse_target_sequences(target/Path(rel))
            sdiff=compare_sequence_details(ss,ts)
            rec["SequenceDetailCount"]=len(sdiff)
            for d in sdiff: seq_detail.append({"RelativePath":rel,**d})

            if rel in old_playable_paths:
                present={s["anim"] for s in ss}
                expected=build_playable_records(present,graph)
                actual=read_target_playable(target/Path(rel))
                diffs=[]
                for requested,(e,a) in enumerate(zip(expected,actual)):
                    if e!=a:
                        diffs.append(requested)
                        distribution[requested][a]+=1
                        playable_detail.append({
                            "RelativePath":rel,
                            "RequestedID":requested,
                            "V45ExpectedID":e[0],"V45ExpectedFlags":e[1],
                            "GoldenActualID":a[0],"GoldenActualFlags":a[1],
                            "PresentAnimationIDs":";".join(map(str,sorted(present))),
                        })
                rec["V45PlayablePass"]=not diffs
                rec["V45PlayableMismatchCount"]=len(diffs)
                rec["V45PlayableMismatchIDs"]=",".join(map(str,diffs))
        except Exception as e:
            rec["Error"]=f"{type(e).__name__}: {e}"
            errors.append({"RelativePath":rel,"Error":rec["Error"]})
        model_results.append(rec)
        if idx%100==0: print(f"[v45-targeted] {idx}/{len(selected_paths)}",flush=True)

    aggregate=[]
    for requested in sorted(distribution):
        counter=distribution[requested]
        aggregate.append({
            "RequestedID":requested,
            "MismatchOccurrences":sum(counter.values()),
            "GoldenActualDistribution":";".join(
                f"{aid}/{flags}:{count}" for (aid,flags),count in counter.most_common()
            ),
        })

    write_csv(meta/"V45_ModelResults.csv",model_results)
    write_csv(meta/"Playable_V45_Remaining_Detail.csv",playable_detail)
    write_csv(meta/"Playable_V45_Remaining_Aggregate.csv",aggregate)
    write_csv(meta/"Sequence_Timeline_V45_Detail.csv",seq_detail)
    write_csv(meta/"V45_Errors.csv",errors)
    write_csv(meta/"Sequence_Timeline_Exceptions_From_V44.csv",seq_exception)

    remaining_models={r["RelativePath"] for r in playable_detail}
    sequence_models={r["RelativePath"] for r in seq_detail}
    for idx,rel in enumerate(sorted(sequence_models),1):
        base=out/"STAGING"/"01_SequenceTimelineExceptions"/f"{idx:02d}_{Path(rel).stem.replace(' ','_')}"
        copy_family(source,rel,base/"335"); copy_family(target,rel,base/"112")
    for idx,rel in enumerate(sorted(remaining_models)[:12],1):
        base=out/"STAGING"/"02_PlayableV45Remaining"/f"{idx:02d}_{Path(rel).stem.replace(' ','_')}"
        copy_family(source,rel,base/"335"); copy_family(target,rel,base/"112")

    playable_error_paths={r["RelativePath"] for r in errors if r["RelativePath"] in old_playable_paths}
    summary={
        "v44_deep_rows":len(rows),
        "v44_playable_fail_models":len(old_playable_fail),
        "v44_sequence_or_timeline_exception_rows":len(seq_exception),
        "targeted_unique_models":len(selected_paths),
        "v45_errors":len(errors),
        "v45_playable_pass_models":len(old_playable_fail)-len(remaining_models)-len(playable_error_paths),
        "v45_remaining_playable_fail_models":len(remaining_models),
        "v45_remaining_playable_mismatch_records":len(playable_detail),
        "v45_remaining_requested_ids":len(aggregate),
        "sequence_timeline_detail_models":len(sequence_models),
        "sequence_timeline_detail_rows":len(seq_detail),
    }
    (meta/"V45_Refine_Summary.json").write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding="utf-8")
    (meta/"V45_Refine_Summary.txt").write_text("\n".join(f"{k}: {v}" for k,v in summary.items()),encoding="utf-8-sig")

    zip_path=out/"ModelPort_GoldenReference_V45_Refine_ALL.zip"
    if zip_path.exists(): zip_path.unlink()
    with zipfile.ZipFile(zip_path,"w",zipfile.ZIP_DEFLATED) as z:
        for p in (out/"STAGING").rglob("*"):
            if p.is_file(): z.write(p,p.relative_to(out/"STAGING"))
    print(json.dumps(summary,ensure_ascii=False,indent=2),flush=True)
    print(f"[ZIP] {zip_path}",flush=True)
    return 0


if __name__=="__main__":
    raise SystemExit(main())
