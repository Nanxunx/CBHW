#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Turtle335Converter targeted feature scan V4.4

V4.4 is intentionally NOT a 11,597-pair full deep scan.

Phases:
1. Header census over all paired M2s: read only the M2 header.
2. Source sequence risk classification for animated models: read only 64-byte
   WotLK sequence records, not whole files.
3. Deep compare only:
   - RibbonEmitter > 0
   - Particle > 0
   - TexAnim > 0
   - external .anim sidecars
   - alias sequences (flags & 0x40)
   - SubAnimationID > 0
   - duplicate AnimationID groups
   - source/target version/count anomalies
   - a capped regression sample of animation-only models

This keeps current research focused on unresolved feature classes rather than
re-validating ordinary static geometry thousands of times.
"""
from __future__ import annotations
import argparse, csv, json, os, struct, sys, shutil, zipfile
from pathlib import Path
from collections import Counter

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

def u32(d,o): return struct.unpack_from("<I", d, o)[0]
def u16(d,o): return struct.unpack_from("<H", d, o)[0]
def pair(d,o): return struct.unpack_from("<II", d, o)

def write_csv(path: Path, rows: list[dict]):
    path.parent.mkdir(parents=True, exist_ok=True)
    fields=[]
    for r in rows:
        for k in r:
            if k not in fields:
                fields.append(k)
    with path.open("w", newline="", encoding="utf-8-sig") as f:
        if not fields:
            return
        w=csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)

def parse_header(path: Path):
    with path.open("rb") as f:
        d=f.read(324)
    if len(d) < 304 or d[:4] != b"MD20":
        raise ValueError("not MD20")
    ver=u32(d,4)
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

def count_external_anim(path: Path) -> int:
    return sum(1 for p in path.parent.glob(path.stem + "*.anim") if p.is_file())

def parse_source_sequences_light(path: Path, header: dict):
    count,off=header["animations"]
    out=[]
    if not count:
        return out
    with path.open("rb") as f:
        f.seek(off)
        raw=f.read(count*64)
    if len(raw) != count*64:
        raise ValueError("source sequence table truncated")
    for pos in range(count):
        o=pos*64
        anim,sub,length=struct.unpack_from("<HHI", raw, o)
        flags=u32(raw,o+12)
        index=u16(raw,o+62)
        out.append({
            "pos":pos,"anim":anim,"sub":sub,"length":length,
            "flags":flags,"index":index,
        })
    return out

def parse_sequences_full(d: bytes, h: dict):
    count,off=h["animations"]
    out=[]
    stride=64 if h["version"]==264 else 68
    for pos in range(count):
        o=off+pos*stride
        if o+stride > len(d):
            raise ValueError(f"sequence {pos} OOB")
        if h["version"]==264:
            anim,sub,length=struct.unpack_from("<HHI",d,o)
            out.append({
                "pos":pos,"anim":anim,"sub":sub,"length":length,
                "tail":d[o+8:o+64],"index":u16(d,o+62),
                "flags":u32(d,o+12),
            })
        else:
            anim,sub,start,end=struct.unpack_from("<HHII",d,o)
            out.append({
                "pos":pos,"anim":anim,"sub":sub,"length":end-start,
                "start":start,"end":end,
                "tail":d[o+12:o+68],"index":u16(d,o+66),
                "flags":u32(d,o+16),
            })
    return out

def build_lookup(seqs):
    if not seqs: return []
    out=[-1]*(max(s["anim"] for s in seqs)+1)
    for s in seqs:
        if s["sub"]==0 and out[s["anim"]]==-1:
            out[s["anim"]]=s["pos"]
    for s in seqs:
        if out[s["anim"]]==-1:
            out[s["anim"]]=s["pos"]
    return out

def read_lookup(d,h):
    c,o=h["anim_lookup"]
    if not c: return []
    if o+c*2 > len(d): raise ValueError("AnimationLookup OOB")
    return list(struct.unpack_from("<"+"h"*c,d,o))

def resolve_playable(q, lookup):
    present={i for i,v in enumerate(lookup) if v>=0}
    x=q; seen=set()
    while x not in present:
        if x in seen: return 0
        seen.add(x)
        if x<0 or x>=PLAYABLE_COUNT: return 0
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

def deep_compare(src: Path,dst: Path):
    sd=src.read_bytes()
    td=dst.read_bytes()
    sh=parse_header(src)
    th=parse_header(dst)
    ss=parse_sequences_full(sd,sh)
    ts=parse_sequences_full(td,th)

    seq_ok=len(ss)==len(ts)
    timeline_ok=seq_ok
    if seq_ok:
        timeline=0
        for a,b in zip(ss,ts):
            timeline+=3333
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
            if b["start"]!=estart or b["end"]!=eend:
                timeline_ok=False

    exp_lookup=build_lookup(ts)
    lookup_ok=(read_lookup(td,th)==exp_lookup)
    playable=read_playable(td,th)
    playable_ok=(playable is not None and playable==expected_playable(exp_lookup))
    return {
        "SequenceMetadataMatch":seq_ok,
        "TimelineRulePass":timeline_ok,
        "AnimationLookupRulePass":lookup_ok,
        "PlayableV4RulePass":playable_ok,
    }

def texture_paths(m2: Path, header: dict):
    c,o=header["textures"]
    if not c: return []
    with m2.open("rb") as f:
        f.seek(o)
        defs=f.read(c*16)
        if len(defs)!=c*16:
            return []
        out=[]
        for i in range(c):
            p=i*16
            typ=u32(defs,p)
            nlen=u32(defs,p+8)
            noff=u32(defs,p+12)
            if typ!=0 or nlen==0:
                continue
            cur=f.tell()
            try:
                f.seek(noff)
                s=f.read(nlen).split(b"\0",1)[0].decode("utf-8","ignore")
                if s and s not in out:
                    out.append(s)
            finally:
                f.seek(cur)
        return out

def copy_relative(root: Path,rel: str,dest: Path):
    s=root/Path(rel)
    if not s.exists():
        return
    o=dest/Path(rel)
    o.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(s,o)

def copy_sample(root: Path,rel: str,dest: Path):
    m2=root/Path(rel)
    if not m2.exists(): return
    copy_relative(root,rel,dest)
    try:
        h=parse_header(m2)
    except Exception:
        return
    for p in m2.parent.glob(m2.stem+"*.anim"):
        if p.is_file():
            copy_relative(root,str(p.relative_to(root)),dest)
    for p in m2.parent.glob(m2.stem+"*.skin"):
        if p.is_file():
            copy_relative(root,str(p.relative_to(root)),dest)
    for p in m2.parent.glob(m2.stem+"*.blp"):
        if p.is_file():
            copy_relative(root,str(p.relative_to(root)),dest)
    for tex in texture_paths(m2,h):
        copy_relative(root,tex,dest)

def classify_conversion(row):
    if row.get("HeaderError"):
        return "BLOCK_HEADER_ERROR"
    if not row.get("Has112"):
        return "NO_GOLDEN_PAIR"
    if row.get("Ribbons",0)>0:
        return "NEEDS_RIBBON_FULL_WRITER"
    if row.get("Particles",0)>0:
        return "NEEDS_PARTICLE_FULL_WRITER"
    if row.get("TexAnims",0)>0:
        return "TEXANIM_GOLDEN_VALIDATED"
    if row.get("ExternalAnimFiles",0)>0:
        return "EXTERNAL_ANIM_COPY"
    if row.get("AliasSequences",0)>0 or row.get("SubAnimationSequences",0)>0 or row.get("DuplicateAnimIDs",0)>0:
        return "ANIMATION_HIGH_RISK_GOLDEN"
    if row.get("Animations",0)>0:
        return "ANIMATION_BASELINE"
    return "STATIC_GEOMETRY_SAFE"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--target", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--animation-regression-limit",type=int,default=24)
    ap.add_argument("--progress-every",type=int,default=500)
    args=ap.parse_args()

    srcroot=Path(args.source)
    dstroot=Path(args.target)
    out=Path(args.out)
    if not srcroot.exists(): raise SystemExit(f"missing source: {srcroot}")
    if not dstroot.exists(): raise SystemExit(f"missing target: {dstroot}")
    meta=out/"STAGING"/"00_Metadata"
    meta.mkdir(parents=True,exist_ok=True)

    src_m2=sorted(
        [p for p in srcroot.rglob("*") if p.is_file() and p.suffix.lower()==".m2"],
        key=lambda p:str(p).lower()
    )

    # Phase 1: header census
    rows=[]
    pairs=[]
    print(f"[phase1] header census: {len(src_m2)} M2",flush=True)
    for idx,p in enumerate(src_m2,1):
        rel=str(p.relative_to(srcroot))
        t=dstroot/Path(rel)
        row={"RelativePath":rel,"Has112":t.exists(),"HeaderError":""}
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
                row.update({
                    "TargetVersion":th["version"],
                    "TargetAnimations":th["animations"][0],
                    "TargetRibbons":th["ribbons"][0],
                    "TargetParticles":th["particles"][0],
                })
                pairs.append((p,t,rel,row,sh,th))
        except Exception as e:
            row["HeaderError"]=f"{type(e).__name__}: {e}"
        rows.append(row)
        if args.progress_every and idx%args.progress_every==0:
            print(f"[phase1] {idx}/{len(src_m2)}",flush=True)

    # Phase 2: source sequence risk classification only.
    animated=[x for x in pairs if x[4]["animations"][0]>0]
    print(f"[phase2] source sequence risk scan: {len(animated)} animated pairs",flush=True)
    byrel={r["RelativePath"]:r for r in rows}
    for idx,(s,t,rel,row,sh,th) in enumerate(animated,1):
        try:
            seq=parse_source_sequences_light(s,sh)
            ids=[x["anim"] for x in seq]
            counts=Counter(ids)
            row.update({
                "AliasSequences":sum(1 for x in seq if x["flags"] & 0x40),
                "SubAnimationSequences":sum(1 for x in seq if x["sub"]!=0),
                "DuplicateAnimIDs":sum(1 for n in counts.values() if n>1),
                "MaxAnimID":max(ids,default=-1),
            })
        except Exception as e:
            row.update({
                "AliasSequences":-1,
                "SubAnimationSequences":-1,
                "DuplicateAnimIDs":-1,
                "MaxAnimID":-1,
                "SequenceRiskError":f"{type(e).__name__}: {e}",
            })
        if args.progress_every and idx%args.progress_every==0:
            print(f"[phase2] {idx}/{len(animated)}",flush=True)

    for r in rows:
        r.setdefault("AliasSequences",0)
        r.setdefault("SubAnimationSequences",0)
        r.setdefault("DuplicateAnimIDs",0)
        r.setdefault("MaxAnimID",-1)
        r["ConversionPlan"]=classify_conversion(r)

    write_csv(meta/"V44_HeaderAndRiskIndex.csv",rows)

    # Phase 3: deep only unresolved/high-risk features, plus capped animation baseline regression.
    must_deep=[]
    animation_baseline=[]
    for s,t,rel,row,sh,th in pairs:
        anomaly=(
            sh["version"]!=264 or th["version"]!=256 or
            sh["animations"][0]!=th["animations"][0] or
            sh["ribbons"][0]!=th["ribbons"][0] or
            sh["particles"][0]!=th["particles"][0]
        )
        highrisk=(
            sh["ribbons"][0]>0 or
            sh["particles"][0]>0 or
            sh["texanims"][0]>0 or
            row.get("ExternalAnimFiles",0)>0 or
            row.get("AliasSequences",0)>0 or
            row.get("SubAnimationSequences",0)>0 or
            row.get("DuplicateAnimIDs",0)>0 or
            anomaly
        )
        if highrisk:
            must_deep.append((s,t,rel,row))
        elif sh["animations"][0]>0:
            animation_baseline.append((s,t,rel,row))

    animation_baseline.sort(
        key=lambda x:(x[3].get("MaxAnimID",-1),x[3].get("Animations",0),x[3].get("Vertices",0)),
        reverse=True
    )
    sampled=animation_baseline[:max(0,args.animation_regression_limit)]
    deep_targets=must_deep+sampled

    print(
        f"[phase3] deep compare: must={len(must_deep)} "
        f"+ animation regression sample={len(sampled)} "
        f"= {len(deep_targets)}",
        flush=True
    )

    deep_rows=[]
    for idx,(s,t,rel,row) in enumerate(deep_targets,1):
        d={
            "RelativePath":rel,
            "ConversionPlan":row["ConversionPlan"],
            "Animations":row.get("Animations",0),
            "Ribbons":row.get("Ribbons",0),
            "Particles":row.get("Particles",0),
            "TexAnims":row.get("TexAnims",0),
            "ExternalAnimFiles":row.get("ExternalAnimFiles",0),
            "AliasSequences":row.get("AliasSequences",0),
            "SubAnimationSequences":row.get("SubAnimationSequences",0),
            "DuplicateAnimIDs":row.get("DuplicateAnimIDs",0),
            "DeepError":"",
        }
        try:
            d.update(deep_compare(s,t))
        except Exception as e:
            d.update({
                "SequenceMetadataMatch":False,
                "TimelineRulePass":False,
                "AnimationLookupRulePass":False,
                "PlayableV4RulePass":False,
                "DeepError":f"{type(e).__name__}: {e}",
            })
        deep_rows.append(d)
        if args.progress_every and idx%args.progress_every==0:
            print(f"[phase3] {idx}/{len(deep_targets)}",flush=True)

    write_csv(meta/"V44_DeepValidation.csv",deep_rows)

    clean=[r for r in deep_rows if not r["DeepError"]]
    summary={
        "source_m2_total":len(src_m2),
        "paired_m2":len(pairs),
        "animated_pairs":len(animated),
        "must_deep_pairs":len(must_deep),
        "animation_regression_sample":len(sampled),
        "deep_total":len(deep_targets),
        "deep_errors":sum(bool(r["DeepError"]) for r in deep_rows),
        "sequence_mismatch":sum(not r["SequenceMetadataMatch"] for r in clean),
        "timeline_mismatch":sum(not r["TimelineRulePass"] for r in clean),
        "animation_lookup_mismatch":sum(not r["AnimationLookupRulePass"] for r in clean),
        "playable_mismatch":sum(not r["PlayableV4RulePass"] for r in clean),
        "true_ribbon_models":sum(r.get("Ribbons",0)>0 for r in rows),
        "particle_models":sum(r.get("Particles",0)>0 for r in rows),
        "texanim_models":sum(r.get("TexAnims",0)>0 for r in rows),
        "external_anim_models":sum(r.get("ExternalAnimFiles",0)>0 for r in rows),
        "alias_models":sum(r.get("AliasSequences",0)>0 for r in rows),
        "subanimation_models":sum(r.get("SubAnimationSequences",0)>0 for r in rows),
        "duplicate_animid_models":sum(r.get("DuplicateAnimIDs",0)>0 for r in rows),
    }
    (meta/"V44_Summary.json").write_text(
        json.dumps(summary,ensure_ascii=False,indent=2),encoding="utf-8"
    )
    (meta/"V44_Summary.txt").write_text(
        "\n".join(f"{k}: {v}" for k,v in summary.items()),
        encoding="utf-8-sig"
    )

    # Separate conversion queues.
    queues={}
    for r in rows:
        queues.setdefault(r["ConversionPlan"],[]).append(r)
    qdir=meta/"ConversionQueues"
    for name,qrows in queues.items():
        write_csv(qdir/(name+".csv"),qrows)

    # Select only evidence needed for next research.
    selected=[]; seen=set()
    def add(cat,cands,limit):
        n=0
        for r in cands:
            rel=r["RelativePath"]
            if rel in seen: continue
            seen.add(rel); selected.append((cat,r)); n+=1
            if n>=limit: break

    mismatch=sorted(
        [r for r in deep_rows if r["DeepError"] or
         not r["SequenceMetadataMatch"] or not r["TimelineRulePass"] or
         not r["AnimationLookupRulePass"] or not r["PlayableV4RulePass"]],
        key=lambda r:(r["Ribbons"],r["Particles"],r["Animations"]),
        reverse=True
    )
    add("01_RuleMismatch",mismatch,8)

    ribbon=sorted(
        [r for r in rows if r.get("Ribbons",0)>0 and r.get("Has112")],
        key=lambda r:(r["Ribbons"],r["Particles"],r["TexAnims"],r["Animations"]),
        reverse=True
    )
    add("02_TrueRibbon",ribbon,6)

    pone=sorted(
        [r for r in rows if r.get("Particles",0)>0 and r.get("Animations",0)==1 and r.get("Has112")],
        key=lambda r:(r["Particles"],r["TexAnims"],r["Vertices"]),
        reverse=True
    )
    add("03_ParticleOneAnimation",pone,5)

    pcomplex=sorted(
        [r for r in rows if r.get("Particles",0)>0 and r.get("Animations",0)>1 and r.get("Has112")],
        key=lambda r:(r["Particles"],r["AliasSequences"],r["SubAnimationSequences"],r["Animations"]),
        reverse=True
    )
    add("04_ParticleComplex",pcomplex,5)

    aliassub=sorted(
        [r for r in rows if r.get("Has112") and
         (r.get("AliasSequences",0)>0 or r.get("SubAnimationSequences",0)>0 or r.get("DuplicateAnimIDs",0)>0)],
        key=lambda r:(r["AliasSequences"],r["SubAnimationSequences"],r["DuplicateAnimIDs"],r["Animations"]),
        reverse=True
    )
    add("05_AliasSubAnimation",aliassub,5)

    sel=[{"Category":c,**r} for c,r in selected]
    write_csv(meta/"V44_SelectedSamples.csv",sel)

    for num,(cat,r) in enumerate(selected,1):
        rel=r["RelativePath"]
        safe=Path(rel).stem.replace(" ","_")
        base=out/"STAGING"/cat/f"{num:02d}_{safe}"
        copy_sample(srcroot,rel,base/"335")
        copy_sample(dstroot,rel,base/"112")

    zip_path=out/"ModelPort_GoldenReference_Targeted_V44_ALL.zip"
    if zip_path.exists(): zip_path.unlink()
    with zipfile.ZipFile(zip_path,"w",zipfile.ZIP_DEFLATED) as z:
        stage=out/"STAGING"
        for p in stage.rglob("*"):
            if p.is_file():
                z.write(p,p.relative_to(stage))

    print(json.dumps(summary,ensure_ascii=False,indent=2),flush=True)
    print(f"[ZIP] {zip_path}",flush=True)

if __name__=="__main__":
    main()
