#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Validate V4.5 Ribbon rules against the V4.4 Golden ZIP."""
from __future__ import annotations
import argparse,json,struct,zipfile
from pathlib import Path
from collections import Counter
from legacy_track_codec_v45 import convert_track,parse_wotlk_track,SequenceWindow

SRC_SIZE=176
DST_SIZE=220
TRACKS=(
 ("color",36,36,12,struct.pack("<fff",1.0,1.0,1.0)),
 ("alpha",56,64,2,struct.pack("<h",0)),
 ("height_above",76,92,4,struct.pack("<f",0.0)),
 ("height_below",96,120,4,struct.pack("<f",0.0)),
 ("tex_slot",132,164,2,struct.pack("<H",0)),
 ("visibility",152,192,1,b"\x00"),
)

def u32(d,o): return struct.unpack_from("<I",d,o)[0]
def pair(d,o): return struct.unpack_from("<II",d,o)

def header(d):
    if d[:4]!=b"MD20": raise ValueError("not MD20")
    v=u32(d,4)
    if v==264:return v,pair(d,0x1c),pair(d,0x120)
    if v==256:return v,pair(d,0x1c),pair(d,0x134)
    raise ValueError(f"unsupported M2 version {v}")

def windows(d):
    v,(c,o),_=header(d)
    if v!=256: raise ValueError("windows require target v256")
    return [SequenceWindow(*struct.unpack_from("<II",d,o+i*68+4)) for i in range(c)]

def arr_u16(d,o):
    c,p=pair(d,o)
    return list(struct.unpack_from("<"+"H"*c,d,p)) if c else []

def classic_track(d,o,base):
    interp,gseq=struct.unpack_from("<Hh",d,o)
    ks=base*(3 if interp in (2,3) else 1)
    rc,ro=pair(d,o+4);tc,to=pair(d,o+12);kc,ko=pair(d,o+20)
    ranges=[struct.unpack_from("<II",d,ro+i*8) for i in range(rc)]
    times=[u32(d,to+i*4) for i in range(tc)]
    keys=[d[ko+i*ks:ko+(i+1)*ks] for i in range(kc)]
    return interp,gseq,ranges,times,keys

def validate_pair(src,dst):
    _,_,(sc,so)=header(src); _,_,(tc,to)=header(dst)
    if sc!=tc: raise ValueError(f"ribbon count mismatch {sc}!={tc}")
    wins=windows(dst); stats=Counter()
    for i in range(sc):
        s=so+i*SRC_SIZE;t=to+i*DST_SIZE
        if src[s:s+20]!=dst[t:t+20]:raise ValueError(f"ribbon {i}: prefix")
        if arr_u16(src,s+20)!=arr_u16(dst,t+20):raise ValueError(f"ribbon {i}: textures")
        if arr_u16(src,s+28)!=arr_u16(dst,t+28):raise ValueError(f"ribbon {i}: materials")
        if src[s+116:s+132]!=dst[t+148:t+164]:raise ValueError(f"ribbon {i}: body")
        stats["static"]+=1
        for name,soff,toff,base,default in TRACKS:
            st=parse_wotlk_track(src,s+soff,base)
            exp=convert_track(st,wins,default)
            ai,ag,ar,at,ak=classic_track(dst,t+toff,base)
            if (ai,ag,ar,at,ak)!=(exp.interpolation,exp.global_sequence,exp.ranges,exp.times,exp.values):
                raise ValueError(f"ribbon {i}: track {name}")
            stats["tracks"]+=1
    return sc,stats

def main():
    ap=argparse.ArgumentParser();ap.add_argument("golden_zip");ap.add_argument("--json-out");args=ap.parse_args()
    total=Counter();groups=[]
    with zipfile.ZipFile(args.golden_zip) as z:
        names=z.namelist()
        roots=sorted({"/".join(n.split("/")[:2]) for n in names if n.startswith("02_TrueRibbon/") and n.lower().endswith(".m2")})
        for root in roots:
            m=[n for n in names if n.startswith(root+"/") and n.lower().endswith(".m2")]
            s=[n for n in m if "/335/" in n][0];t=[n for n in m if "/112/" in n][0]
            count,stats=validate_pair(z.read(s),z.read(t));total.update(stats)
            groups.append({"group":root,"emitters":count,"tracks":stats["tracks"],"pass":True})
    emitters=sum(x["emitters"] for x in groups)
    result={"golden_groups":len(groups),"ribbon_emitters":emitters,"static_pass":total["static"],"animation_tracks_pass":total["tracks"],"expected_tracks":emitters*6,"pass":total["static"]==emitters and total["tracks"]==emitters*6,"groups":groups}
    txt=json.dumps(result,ensure_ascii=False,indent=2);print(txt)
    if args.json_out:Path(args.json_out).write_text(txt,encoding="utf-8")
    raise SystemExit(0 if result["pass"] else 2)

if __name__=="__main__":main()
