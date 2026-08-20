#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Turtle335Converter targeted feature scan V4.3

Strategy:
1) Scan all 335 M2 headers only (cheap).
2) Deep-compare only models relevant to current unresolved feature groups:
   - animations > 0
   - ribbons > 0
   - particles > 0
   - texanims > 0
   - events > 0
   - external .anim sidecars
   - version/count anomalies
3) Static/simple models are not deep-scanned.

Output remains analysis-only. No Orange/private format.
"""
from __future__ import annotations
import argparse, csv, json, os, struct, sys, shutil
from pathlib import Path
from dataclasses import dataclass
from typing import Optional

PLAYABLE_COUNT = 226

LEGACY_FALLBACK = {
    0:147, 6:1, 8:25, 9:8, 10:9, 13:4, 17:16, 18:17, 19:18, 20:8,
    21:8, 22:8, 23:8, 24:8, 26:25, 27:25, 28:25, 30:8, 32:16, 33:32,
    36:8, 42:4, 45:42, 51:52, 52:31, 53:54, 54:33, 55:16, 57:17,
    58:18, 59:87, 71:100, 85:17, 86:19, 87:88, 88:16, 95:16,
    97:96, 98:96, 100:99, 101:99, 107:16, 115:114, 116:114,
    117:87, 118:57, 119:4, 123:128, 124:52, 125:31, 129:128,
    131:1, 132:131, 135:42, 136:62, 137:14, 138:63, 141:115,
    143:5, 146:0, 147:146, 148:146, 149:148, 150:148, 151:150,
    152:150, 187:5, 188:50, 189:50, 196:1, 197:69, 199:61,
    203:115, 208:60, 209:84, 210:113, 211:69, 212:16,
    223:119, 224:127,
}
GOLDEN_EXT = {
    170:16,171:16,172:16,173:16,174:16,175:30,176:16,178:16,
    179:16,181:16,191:159,
}
FALLBACK = dict(LEGACY_FALLBACK)
FALLBACK.update(GOLDEN_EXT)
PLAY_THEN_STOP = {6,97,100,115,123,132,188}
PLAY_BACKWARDS = {13,45,101,189}

def u32(d,o): return struct.unpack_from("<I",d,o)[0]
def u16(d,o): return struct.unpack_from("<H",d,o)[0]
def i16(d,o): return struct.unpack_from("<h",d,o)[0]
def pair(d,o): return struct.unpack_from("<II",d,o)

def read_head(path: Path, need=324) -> bytes:
    with path.open("rb") as f:
        return f.read(need)

def parse_header(path: Path):
    d = read_head(path, 324)
    if len(d) < 304 or d[:4] != b"MD20":
        raise ValueError("not MD20")
    ver = u32(d,4)
    if ver == 264:
        return {
            "version":264,
            "animations":pair(d,0x1c),
            "anim_lookup":pair(d,0x24),
            "bones":pair(d,0x2c),
            "vertices":pair(d,0x3c),
            "views":u32(d,0x44),
            "textures":pair(d,0x50),
            "texanims":pair(d,0x60),
            "events":pair(d,0x100),
            "ribbons":pair(d,0x120),
            "particles":pair(d,0x128),
        }
    if ver == 256:
        return {
            "version":256,
            "animations":pair(d,0x1c),
            "anim_lookup":pair(d,0x24),
            "playable":pair(d,0x2c),
            "bones":pair(d,0x34),
            "vertices":pair(d,0x44),
            "views":pair(d,0x4c)[0],
            "textures":pair(d,0x5c),
            "texanims":pair(d,0x74),
            "events":pair(d,0x114),
            "ribbons":pair(d,0x134),
            "particles":pair(d,0x13c),
        }
    raise ValueError(f"unsupported version {ver}")

def count_external_anim(m2: Path) -> int:
    stem = m2.stem
    return sum(1 for p in m2.parent.glob(stem + "*.anim") if p.is_file())

def read_all(path: Path) -> bytes:
    return path.read_bytes()

def parse_sequences(d: bytes, h: dict):
    count, off = h["animations"]
    out=[]
    if h["version"] == 264:
        stride=64
        for pos in range(count):
            o=off+pos*stride
            if o+64 > len(d): raise ValueError(f"source sequence {pos} OOB")
            anim,sub,length=struct.unpack_from("<HHI",d,o)
            out.append({
                "pos":pos, "anim":anim, "sub":sub, "length":length,
                "tail":d[o+8:o+64], "index":u16(d,o+62),
                "flags":u32(d,o+12),
            })
    else:
        stride=68
        for pos in range(count):
            o=off+pos*stride
            if o+68 > len(d): raise ValueError(f"target sequence {pos} OOB")
            anim,sub,start,end=struct.unpack_from("<HHII",d,o)
            out.append({
                "pos":pos, "anim":anim, "sub":sub, "length":end-start,
                "start":start, "end":end,
                "tail":d[o+12:o+68], "index":u16(d,o+66),
                "flags":u32(d,o+16),
            })
    return out

def build_lookup(seqs):
    if not seqs: return []
    out=[-1]*(max(s["anim"] for s in seqs)+1)
    for s in seqs:
        if s["sub"] == 0 and out[s["anim"]] == -1:
            out[s["anim"]] = s["pos"]
    for s in seqs:
        if out[s["anim"]] == -1:
            out[s["anim"]] = s["pos"]
    return out

def read_lookup(d,h):
    c,o=h["anim_lookup"]
    if not c: return []
    if o+c*2 > len(d): raise ValueError("AnimationLookup OOB")
    return list(struct.unpack_from("<"+"h"*c,d,o))

def resolve_playable(q, lookup):
    present={i for i,v in enumerate(lookup) if v >= 0}
    x=q
    seen=set()
    while x not in present:
        if x in seen: return 0
        seen.add(x)
        if x < 0 or x >= PLAYABLE_COUNT: return 0
        x=FALLBACK.get(x,0)
    return x

def expected_playable(lookup):
    out=[]
    for q in range(PLAYABLE_COUNT):
        real=resolve_playable(q,lookup)
        flags=0
        if real != q:
            if q in PLAY_THEN_STOP: flags=3
            elif q in PLAY_BACKWARDS: flags=1
        out.append((real,flags))
    return out

def read_playable(d,h):
    c,o=h.get("playable",(0,0))
    if c != PLAYABLE_COUNT:
        return None
    if o+c*4 > len(d): raise ValueError("Playable OOB")
    return [struct.unpack_from("<hh",d,o+i*4) for i in range(c)]

def deep_compare(src: Path, dst: Path):
    sd=read_all(src); td=read_all(dst)
    sh=parse_header(src); th=parse_header(dst)

    ss=parse_sequences(sd,sh); ts=parse_sequences(td,th)

    seq_ok = len(ss)==len(ts)
    timeline_ok = seq_ok
    if seq_ok:
        timeline=0
        for a,b in zip(ss,ts):
            timeline += 3333
            estart=timeline
            eend=estart+a["length"]
            timeline=eend
            if not (
                a["anim"]==b["anim"] and
                a["sub"]==b["sub"] and
                a["length"]==b["length"] and
                a["index"]==b["index"] and
                a["tail"]==b["tail"]
            ):
                seq_ok=False
            if b.get("start")!=estart or b.get("end")!=eend:
                timeline_ok=False

    exp_lookup=build_lookup(ts)
    act_lookup=read_lookup(td,th)
    lookup_ok=(exp_lookup==act_lookup)

    playable=read_playable(td,th)
    playable_ok=(playable is not None and playable==expected_playable(exp_lookup))

    alias=sum(1 for s in ss if (s["flags"] & 0x40)!=0)
    sub=sum(1 for s in ss if s["sub"]!=0)

    return {
        "SequenceMetadataMatch":seq_ok,
        "TimelineRulePass":timeline_ok,
        "AnimationLookupRulePass":lookup_ok,
        "PlayableV4RulePass":playable_ok,
        "AliasSequences":alias,
        "SubAnimationSequences":sub,
        "MaxAnimID":max((s["anim"] for s in ss), default=-1),
        "SequenceSignature":";".join(
            f'{s["anim"]}:{s["sub"]}:0x{s["flags"]:X}:{s["index"]}' for s in ss
        ),
    }

def write_csv(path: Path, rows: list[dict]):
    path.parent.mkdir(parents=True,exist_ok=True)
    fields=[]
    for r in rows:
        for k in r:
            if k not in fields: fields.append(k)
    with path.open("w",newline="",encoding="utf-8-sig") as f:
        w=csv.DictWriter(f,fieldnames=fields)
        w.writeheader(); w.writerows(rows)

def copy_sample(root: Path, rel: str, dest: Path):
    src=root/Path(rel)
    if not src.exists(): return
    out=dest/Path(rel)
    out.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(src,out)

    stem=src.stem
    for p in src.parent.glob(stem+"*.anim"):
        if p.is_file():
            r=p.relative_to(root)
            o=dest/r
            o.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(p,o)
    for p in src.parent.glob(stem+"*.skin"):
        if p.is_file():
            r=p.relative_to(root)
            o=dest/r
            o.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(p,o)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--target", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--progress-every",type=int,default=500)
    args=ap.parse_args()

    srcroot=Path(args.source); dstroot=Path(args.target); out=Path(args.out)
    if not srcroot.exists(): raise SystemExit(f"missing source: {srcroot}")
    if not dstroot.exists(): raise SystemExit(f"missing target: {dstroot}")

    out.mkdir(parents=True,exist_ok=True)
    meta=out/"STAGING"/"00_Metadata"
    meta.mkdir(parents=True,exist_ok=True)

    src_m2=sorted(
        [p for p in srcroot.rglob("*") if p.is_file() and p.suffix.lower()==".m2"],
        key=lambda p: str(p).lower()
    )

    header_rows=[]
    deep_candidates=[]

    print(f"[phase1] header scan total={len(src_m2)}",flush=True)
    for idx,p in enumerate(src_m2,1):
        rel=str(p.relative_to(srcroot))
        t=dstroot/Path(rel)

        row={"RelativePath":rel,"Has112":t.exists()}
        try:
            sh=parse_header(p)
            row.update({
                "SourceVersion":sh["version"],
                "Animations":sh["animations"][0],
                "Bones":sh["bones"][0],
                "Vertices":sh["vertices"][0],
                "Views":sh["views"],
                "Textures":sh["textures"][0],
                "TexAnims":sh["texanims"][0],
                "Events":sh["events"][0],
                "Ribbons":sh["ribbons"][0],
                "Particles":sh["particles"][0],
                "ExternalAnimFiles":count_external_anim(p),
            })
            if t.exists():
                th=parse_header(t)
                row["TargetVersion"]=th["version"]
                row["TargetAnimations"]=th["animations"][0]
                row["TargetRibbons"]=th["ribbons"][0]
                row["TargetParticles"]=th["particles"][0]

                relevant = (
                    sh["animations"][0] > 0 or
                    sh["ribbons"][0] > 0 or
                    sh["particles"][0] > 0 or
                    sh["texanims"][0] > 0 or
                    sh["events"][0] > 0 or
                    row["ExternalAnimFiles"] > 0 or
                    sh["version"] != 264 or
                    th["version"] != 256 or
                    sh["animations"][0] != th["animations"][0]
                )
                row["DeepRelevant"]=relevant
                if relevant:
                    deep_candidates.append((p,t,rel,row))
            else:
                row["DeepRelevant"]=False
        except Exception as e:
            row["HeaderError"]=f"{type(e).__name__}: {e}"
            row["DeepRelevant"]=False
        header_rows.append(row)

        if args.progress_every and idx % args.progress_every == 0:
            print(f"[phase1] {idx}/{len(src_m2)}",flush=True)

    write_csv(meta/"V43_HeaderFeatureIndex.csv",header_rows)

    print(f"[phase2] deep candidates={len(deep_candidates)}",flush=True)
    deep_rows=[]
    for idx,(s,t,rel,hrow) in enumerate(deep_candidates,1):
        row={
            "RelativePath":rel,
            "Animations":hrow.get("Animations",0),
            "Ribbons":hrow.get("Ribbons",0),
            "Particles":hrow.get("Particles",0),
            "TexAnims":hrow.get("TexAnims",0),
            "Events":hrow.get("Events",0),
            "ExternalAnimFiles":hrow.get("ExternalAnimFiles",0),
        }
        try:
            row.update(deep_compare(s,t))
            row["DeepError"]=""
        except Exception as e:
            row.update({
                "SequenceMetadataMatch":False,
                "TimelineRulePass":False,
                "AnimationLookupRulePass":False,
                "PlayableV4RulePass":False,
                "AliasSequences":-1,
                "SubAnimationSequences":-1,
                "MaxAnimID":-1,
                "SequenceSignature":"",
                "DeepError":f"{type(e).__name__}: {e}",
            })
        deep_rows.append(row)

        if args.progress_every and idx % args.progress_every == 0:
            print(f"[phase2] {idx}/{len(deep_candidates)}",flush=True)

    write_csv(meta/"V43_DeepFeatureValidation.csv",deep_rows)

    clean=[r for r in deep_rows if not r["DeepError"]]
    summary={
        "source_m2_total":len(src_m2),
        "paired_m2":sum(1 for r in header_rows if r.get("Has112")),
        "deep_candidates":len(deep_candidates),
        "deep_errors":sum(1 for r in deep_rows if r["DeepError"]),
        "sequence_mismatch":sum(1 for r in clean if not r["SequenceMetadataMatch"]),
        "timeline_mismatch":sum(1 for r in clean if not r["TimelineRulePass"]),
        "animation_lookup_mismatch":sum(1 for r in clean if not r["AnimationLookupRulePass"]),
        "playable_mismatch":sum(1 for r in clean if not r["PlayableV4RulePass"]),
        "true_ribbon_models":sum(1 for r in deep_rows if r["Ribbons"]>0),
        "particle_models":sum(1 for r in deep_rows if r["Particles"]>0),
        "texanim_models":sum(1 for r in deep_rows if r["TexAnims"]>0),
        "external_anim_models":sum(1 for r in deep_rows if r["ExternalAnimFiles"]>0),
        "alias_models":sum(1 for r in clean if r["AliasSequences"]>0),
        "subanimation_models":sum(1 for r in clean if r["SubAnimationSequences"]>0),
    }
    (meta/"V43_Summary.json").write_text(
        json.dumps(summary,ensure_ascii=False,indent=2),encoding="utf-8"
    )

    txt="\n".join(f"{k}: {v}" for k,v in summary.items())
    (meta/"V43_Summary.txt").write_text(txt,encoding="utf-8-sig")

    selected=[]
    seen=set()

    def add(cat, candidates, limit):
        n=0
        for r in candidates:
            rel=r["RelativePath"]
            if rel in seen: continue
            seen.add(rel)
            selected.append((cat,r))
            n+=1
            if n>=limit: break

    mismatch=sorted(
        [r for r in deep_rows if r["DeepError"] or
         not r["SequenceMetadataMatch"] or
         not r["TimelineRulePass"] or
         not r["AnimationLookupRulePass"] or
         not r["PlayableV4RulePass"]],
        key=lambda r:(r["Ribbons"],r["Particles"],r["Animations"]), reverse=True
    )
    add("01_RuleMismatch", mismatch, 6)

    ribbons=sorted(
        [r for r in deep_rows if r["Ribbons"]>0],
        key=lambda r:(r["Ribbons"],r["Particles"],r["TexAnims"],r["Animations"]),
        reverse=True
    )
    add("02_TrueRibbon", ribbons, 5)

    particle_one=sorted(
        [r for r in deep_rows if r["Particles"]>0 and r["Animations"]==1],
        key=lambda r:(r["Particles"],r["TexAnims"]), reverse=True
    )
    add("03_ParticleOneAnimation", particle_one, 4)

    particle_complex=sorted(
        [r for r in deep_rows if r["Particles"]>0 and r["Animations"]>1],
        key=lambda r:(r["Particles"],r["AliasSequences"],r["SubAnimationSequences"],r["Animations"]),
        reverse=True
    )
    add("04_ParticleComplex", particle_complex, 4)

    alias_sub=sorted(
        [r for r in clean if r["AliasSequences"]>0 or r["SubAnimationSequences"]>0],
        key=lambda r:(r["AliasSequences"],r["SubAnimationSequences"],r["Animations"]),
        reverse=True
    )
    add("05_AliasSubAnimation", alias_sub, 4)

    sel_rows=[{"Category":c,**r} for c,r in selected]
    write_csv(meta/"V43_SelectedSamples.csv",sel_rows)

    for num,(cat,r) in enumerate(selected,1):
        rel=r["RelativePath"]
        safe=Path(rel).stem.replace(" ","_")
        base=out/"STAGING"/cat/f"{num:02d}_{safe}"
        copy_sample(srcroot,rel,base/"335")
        copy_sample(dstroot,rel,base/"112")

    zip_path=out/"ModelPort_GoldenReference_Targeted_V43_ALL.zip"
    if zip_path.exists(): zip_path.unlink()
    import zipfile
    with zipfile.ZipFile(zip_path,"w",zipfile.ZIP_DEFLATED) as z:
        stage=out/"STAGING"
        for p in stage.rglob("*"):
            if p.is_file():
                z.write(p,p.relative_to(stage))

    print(json.dumps(summary,ensure_ascii=False,indent=2),flush=True)
    print(f"[ZIP] {zip_path}",flush=True)

if __name__=="__main__":
    main()
