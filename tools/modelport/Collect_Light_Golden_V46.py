#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Collect a tiny Light-bearing 335/112 Golden pack.

This is NOT a deep/full scan. It reads only M2 headers, keeps same-relative-path
pairs where both source v264 and successful target v256 contain Light records,
then copies at most `--limit` high-value families.
"""
from __future__ import annotations
import argparse, csv, json, shutil, struct, zipfile
from pathlib import Path

def u32(d,o): return struct.unpack_from("<I",d,o)[0]
def pair(d,o): return struct.unpack_from("<II",d,o)

def hdr(path: Path):
    with path.open("rb") as f:
        d=f.read(324)
    if len(d)<304 or d[:4]!=b"MD20": raise ValueError("not MD20")
    v=u32(d,4)
    if v==264:
        return {"version":v,"animations":pair(d,0x1c)[0],"lights":pair(d,0x108)[0],"ribbons":pair(d,0x120)[0],"particles":pair(d,0x128)[0]}
    if v==256:
        return {"version":v,"animations":pair(d,0x1c)[0],"lights":pair(d,0x11c)[0],"ribbons":pair(d,0x134)[0],"particles":pair(d,0x13c)[0]}
    raise ValueError(f"unsupported v{v}")

def copy_family(root: Path, rel: str, dest: Path):
    m2=root/Path(rel); out=dest/Path(rel); out.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(m2,out)
    stem=m2.stem
    for pattern in (stem+"*.skin",stem+"*.anim",stem+"*.blp"):
        for p in m2.parent.glob(pattern):
            if p.is_file():
                q=dest/p.relative_to(root); q.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(p,q)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--target", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--limit",type=int,default=5)
    args=ap.parse_args(); sroot=Path(args.source); troot=Path(args.target); out=Path(args.out)
    if not sroot.exists(): raise SystemExit(f"missing source: {sroot}")
    if not troot.exists(): raise SystemExit(f"missing target: {troot}")
    stage=out/"STAGING"; stage.mkdir(parents=True,exist_ok=True)
    candidates=[]
    files=[p for p in sroot.rglob("*") if p.is_file() and p.suffix.lower()==".m2"]
    print(f"[header] source M2={len(files)}",flush=True)
    for i,p in enumerate(files,1):
        rel=str(p.relative_to(sroot)); t=troot/Path(rel)
        if not t.exists(): continue
        try: sh=hdr(p); th=hdr(t)
        except Exception: continue
        if sh["version"]!=264 or th["version"]!=256 or sh["lights"]<=0 or th["lights"]<=0: continue
        row={"RelativePath":rel,"SourceLights":sh["lights"],"TargetLights":th["lights"],"LightCountMatch":sh["lights"]==th["lights"],"SourceAnimations":sh["animations"],"TargetAnimations":th["animations"],"SourceRibbons":sh["ribbons"],"SourceParticles":sh["particles"],"TargetRibbons":th["ribbons"],"TargetParticles":th["particles"]}
        row["_score"]=(1 if row["LightCountMatch"] else 0,-(sh["ribbons"]+sh["particles"]),sh["lights"],sh["animations"])
        candidates.append(row)
        if i%2000==0: print(f"[header] {i}/{len(files)}",flush=True)
    candidates.sort(key=lambda r:r["_score"],reverse=True); chosen=candidates[:max(0,args.limit)]
    for r in chosen: r.pop("_score",None)
    meta=stage/"00_Metadata"; meta.mkdir(parents=True,exist_ok=True)
    fields=list(chosen[0].keys()) if chosen else ["RelativePath"]
    with (meta/"LightGolden_Selected.csv").open("w",newline="",encoding="utf-8-sig") as f:
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(chosen)
    summary={"header_candidates":len(candidates),"selected":len(chosen),"selection_requires_source_and_target_light_gt_0":True}
    (meta/"LightGolden_Summary.json").write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding="utf-8")
    for n,r in enumerate(chosen,1):
        rel=r["RelativePath"]; name=Path(rel).stem.replace(" ","_"); base=stage/"01_LightPairs"/f"{n:02d}_{name}"
        copy_family(sroot,rel,base/"335"); copy_family(troot,rel,base/"112")
    zpath=out/"ModelPort_GoldenReference_Light_V46_ALL.zip"
    if zpath.exists(): zpath.unlink()
    with zipfile.ZipFile(zpath,"w",zipfile.ZIP_DEFLATED) as z:
        for p in stage.rglob("*"):
            if p.is_file(): z.write(p,p.relative_to(stage))
    print(json.dumps(summary,ensure_ascii=False,indent=2)); print(f"[ZIP] {zpath}")

if __name__=="__main__": main()
