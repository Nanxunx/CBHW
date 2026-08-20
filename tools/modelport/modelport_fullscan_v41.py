#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Turtle335Converter - full paired M2 feature/metadata scan V4.1

Purpose
-------
Scan the immutable 3.3.5a raw library against the successful 1.12 Golden
library without modifying either tree.

Validated rules:
- WotLK source: MD20 v264
- Classic/Turtle target: MD20 v256
- Sequence semantic metadata is preserved from source.
- Classic sequence timeline inserts +3333 ms before each sequence.
- AnimationLookup count=max(AnimationID)+1, missing=-1, duplicate ID prefers
  SubAnimationID 0 and otherwise first physical occurrence.
- PlayableAnimationLookup contains 226 x 4-byte records and is rebuilt from
  the Golden V4 fallback graph.
- The scan records actual feature counts from M2 headers (Ribbon, Particle,
  TexAnim, Event, etc.) and external .anim sidecars.

V4.1 fixes the failed PowerShell V4 third-batch scan:
- parse/validation is performed in Python with explicit binary bounds checks;
- one pair failing no longer destroys all feature metadata;
- exact exception text is written to ErrorMessage;
- DBC copies are separated into 335/ and 112/ namespaces;
- preflight self-test aborts before a long scan if the parser cannot validate
  the first paired model.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import shutil
import struct
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

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
    170:16, 171:16, 172:16, 173:16, 174:16, 175:30,
    176:16, 178:16, 179:16, 181:16, 191:159,
}
FALLBACK = dict(LEGACY_FALLBACK)
FALLBACK.update(GOLDEN_EXT)

PLAY_THEN_STOP = {6, 97, 100, 115, 123, 132, 188}
PLAY_BACKWARDS = {13, 45, 101, 189}


@dataclass
class Pair:
    count: int
    offset: int


@dataclass
class Header:
    version: int
    animations: Pair
    animation_lookup: Pair
    playable: Pair | None
    bones: Pair
    vertices: Pair
    views: int
    textures: Pair
    tex_anims: Pair
    events: Pair
    ribbons: Pair
    particles: Pair


@dataclass
class Seq:
    position: int
    anim_id: int
    sub_id: int
    length: int
    move_bits: int
    flags: int
    probability: int
    unused: int
    d1: int
    d2: int
    play_speed: int
    next_bits: int
    index: int


def _need(data: bytes, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError(
            f"{label} out of range: offset={offset} size={size} file={len(data)}"
        )


def _u32(data: bytes, offset: int) -> int:
    _need(data, offset, 4, "u32")
    return struct.unpack_from("<I", data, offset)[0]


def _pair(data: bytes, offset: int) -> Pair:
    _need(data, offset, 8, "pair")
    return Pair(*struct.unpack_from("<II", data, offset))


def parse_header(data: bytes) -> Header:
    _need(data, 0, 8, "M2 header")
    if data[:4] != b"MD20":
        raise ValueError(f"not MD20 magic: {data[:4]!r}")
    version = _u32(data, 4)
    if version == 264:
        return Header(
            version=version,
            animations=_pair(data, 0x1C),
            animation_lookup=_pair(data, 0x24),
            playable=None,
            bones=_pair(data, 0x2C),
            vertices=_pair(data, 0x3C),
            views=_u32(data, 0x44),
            textures=_pair(data, 0x50),
            tex_anims=_pair(data, 0x60),
            events=_pair(data, 0x100),
            ribbons=_pair(data, 0x120),
            particles=_pair(data, 0x128),
        )
    if version == 256:
        views = _pair(data, 0x4C)
        return Header(
            version=version,
            animations=_pair(data, 0x1C),
            animation_lookup=_pair(data, 0x24),
            playable=_pair(data, 0x2C),
            bones=_pair(data, 0x34),
            vertices=_pair(data, 0x44),
            views=views.count,
            textures=_pair(data, 0x5C),
            tex_anims=_pair(data, 0x74),
            events=_pair(data, 0x114),
            ribbons=_pair(data, 0x134),
            particles=_pair(data, 0x13C),
        )
    raise ValueError(f"unsupported MD20 version {version}")


def parse_sequences(data: bytes, header: Header) -> list[Seq]:
    out: list[Seq] = []
    count, base = header.animations.count, header.animations.offset
    if count == 0:
        return out
    if header.version == 264:
        stride = 64
        _need(data, base, count * stride, "WotLK sequence array")
        for pos in range(count):
            o = base + pos * stride
            anim, sub, length = struct.unpack_from("<HHI", data, o)
            move = _u32(data, o + 8)
            flags = _u32(data, o + 12)
            prob, unused = struct.unpack_from("<HH", data, o + 16)
            d1, d2, play = struct.unpack_from("<III", data, o + 20)
            nxt, idx = struct.unpack_from("<HH", data, o + 60)
            out.append(Seq(pos, anim, sub, length, move, flags, prob, unused, d1, d2, play, nxt, idx))
        return out
    if header.version == 256:
        stride = 68
        _need(data, base, count * stride, "Classic sequence array")
        for pos in range(count):
            o = base + pos * stride
            anim, sub, start, end = struct.unpack_from("<HHII", data, o)
            if end < start:
                raise ValueError(f"sequence {pos} end<start")
            move = _u32(data, o + 12)
            flags = _u32(data, o + 16)
            prob, unused = struct.unpack_from("<HH", data, o + 20)
            d1, d2, play = struct.unpack_from("<III", data, o + 24)
            nxt, idx = struct.unpack_from("<HH", data, o + 64)
            out.append(Seq(pos, anim, sub, end - start, move, flags, prob, unused, d1, d2, play, nxt, idx))
        return out
    raise AssertionError(header.version)


def seq_semantics_equal(source: Sequence[Seq], target: Sequence[Seq]) -> tuple[bool, list[str]]:
    if len(source) != len(target):
        return False, [f"sequence_count {len(source)} != {len(target)}"]
    issues = []
    for i, (a, b) in enumerate(zip(source, target)):
        fields = (
            "anim_id", "sub_id", "length", "move_bits", "flags", "probability",
            "unused", "d1", "d2", "play_speed", "next_bits", "index",
        )
        for field in fields:
            if getattr(a, field) != getattr(b, field):
                issues.append(
                    f"seq[{i}].{field} source={getattr(a, field)} target={getattr(b, field)}"
                )
    return not issues, issues


def timeline_ok(source: Sequence[Seq], target_data: bytes, target_header: Header) -> tuple[bool, list[str]]:
    issues = []
    timeline = 0
    base = target_header.animations.offset
    for i, s in enumerate(source):
        timeline += 3333
        expected_start = timeline
        expected_end = expected_start + s.length
        timeline = expected_end
        o = base + i * 68
        _need(target_data, o + 4, 8, f"target sequence {i} timeline")
        start, end = struct.unpack_from("<II", target_data, o + 4)
        if (start, end) != (expected_start, expected_end):
            issues.append(
                f"seq[{i}] timeline {start}..{end} expected {expected_start}..{expected_end}"
            )
    return not issues, issues


def build_animation_lookup(seqs: Sequence[Seq]) -> list[int]:
    if not seqs:
        return []
    lookup = [-1] * (max(s.anim_id for s in seqs) + 1)
    for s in seqs:
        if s.sub_id == 0 and lookup[s.anim_id] == -1:
            lookup[s.anim_id] = s.position
    for s in seqs:
        if lookup[s.anim_id] == -1:
            lookup[s.anim_id] = s.position
    return lookup


def read_animation_lookup(data: bytes, h: Header) -> list[int]:
    p = h.animation_lookup
    if p.count == 0:
        return []
    _need(data, p.offset, p.count * 2, "AnimationLookup")
    return list(struct.unpack_from("<" + "h" * p.count, data, p.offset))


def resolve_playable(requested: int, lookup: Sequence[int]) -> int:
    current = int(requested)
    seen: set[int] = set()
    while True:
        if current in seen:
            return 0
        seen.add(current)
        if 0 <= current < len(lookup) and lookup[current] >= 0:
            return current
        if current < 0 or current >= PLAYABLE_COUNT:
            return 0
        current = int(FALLBACK.get(current, 0))


def expected_playable(lookup: Sequence[int]) -> list[tuple[int, int]]:
    result = []
    for requested in range(PLAYABLE_COUNT):
        real = resolve_playable(requested, lookup)
        flags = 0
        if real != requested:
            if requested in PLAY_THEN_STOP:
                flags = 3
            elif requested in PLAY_BACKWARDS:
                flags = 1
        result.append((real, flags))
    return result


def read_playable(data: bytes, h: Header) -> list[tuple[int, int]]:
    if h.playable is None:
        return []
    p = h.playable
    if p.count == 0:
        return []
    _need(data, p.offset, p.count * 4, "PlayableAnimationLookup")
    return [struct.unpack_from("<hh", data, p.offset + i * 4) for i in range(p.count)]


def texture_paths(data: bytes, h: Header) -> list[str]:
    p = h.textures
    if p.count == 0:
        return []
    _need(data, p.offset, p.count * 16, "TextureDef array")
    out = []
    seen = set()
    for i in range(p.count):
        o = p.offset + i * 16
        tex_type = _u32(data, o)
        name_len = _u32(data, o + 8)
        name_off = _u32(data, o + 12)
        if tex_type != 0 or name_len == 0:
            continue
        _need(data, name_off, name_len, f"texture[{i}] name")
        raw = data[name_off:name_off + name_len].split(b"\0", 1)[0]
        name = raw.decode("utf-8", "replace")
        key = name.replace("/", "\\").lower()
        if name and key not in seen:
            seen.add(key)
            out.append(name.replace("/", "\\"))
    return out


def external_anim_count(m2_path: Path) -> int:
    stem = m2_path.stem.lower()
    try:
        return sum(
            1 for p in m2_path.parent.iterdir()
            if p.is_file() and p.suffix.lower() == ".anim" and p.stem.lower().startswith(stem)
        )
    except OSError:
        return 0


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def compare_pair(source_path: Path, target_path: Path, relative: str) -> dict:
    row = {
        "RelativePath": relative,
        "Size335": source_path.stat().st_size,
        "Size112": target_path.stat().st_size,
        "Animations": -1,
        "DistinctAnimIDs": -1,
        "MaxAnimID": -1,
        "SubAnimationSequences": -1,
        "AliasSequences": -1,
        "Bones": -1,
        "Vertices": -1,
        "Views": -1,
        "Textures": -1,
        "TexAnims": -1,
        "Events": -1,
        "Ribbons": -1,
        "Particles": -1,
        "ExternalAnimFiles": -1,
        "SequenceMetadataMatch": False,
        "TimelineRulePass": False,
        "AnimationLookupRulePass": False,
        "PlayableCount": -1,
        "PlayableV4RulePass": False,
        "PlayableMismatchIDs": "",
        "SequenceSignature": "",
        "ConversionClass": "",
        "ErrorMessage": "",
    }
    try:
        sd = source_path.read_bytes()
        td = target_path.read_bytes()
        sh = parse_header(sd)
        th = parse_header(td)
        if sh.version != 264 or th.version != 256:
            raise ValueError(f"expected source264/target256, got {sh.version}/{th.version}")
        ss = parse_sequences(sd, sh)
        ts = parse_sequences(td, th)

        row.update({
            "Animations": sh.animations.count,
            "DistinctAnimIDs": len({s.anim_id for s in ss}),
            "MaxAnimID": max((s.anim_id for s in ss), default=-1),
            "SubAnimationSequences": sum(s.sub_id != 0 for s in ss),
            "AliasSequences": sum(bool(s.flags & 0x40) for s in ss),
            "Bones": sh.bones.count,
            "Vertices": sh.vertices.count,
            "Views": sh.views,
            "Textures": sh.textures.count,
            "TexAnims": sh.tex_anims.count,
            "Events": sh.events.count,
            "Ribbons": sh.ribbons.count,
            "Particles": sh.particles.count,
            "ExternalAnimFiles": external_anim_count(source_path),
            "SequenceSignature": ";".join(
                f"{s.anim_id}:{s.sub_id}:0x{s.flags:X}:{s.index}" for s in ss
            ),
        })

        seq_ok, seq_issues = seq_semantics_equal(ss, ts)
        time_ok, time_issues = timeline_ok(ss, td, th)
        row["SequenceMetadataMatch"] = seq_ok
        row["TimelineRulePass"] = time_ok

        expected_lookup = build_animation_lookup(ts)
        actual_lookup = read_animation_lookup(td, th)
        row["AnimationLookupRulePass"] = expected_lookup == actual_lookup

        actual_play = read_playable(td, th)
        expected_play = expected_playable(expected_lookup)
        row["PlayableCount"] = len(actual_play)
        diffs = [
            i for i in range(min(len(actual_play), PLAYABLE_COUNT))
            if actual_play[i] != expected_play[i]
        ]
        if len(actual_play) != PLAYABLE_COUNT:
            diffs.append(-1)
        row["PlayableMismatchIDs"] = ",".join(map(str, diffs))
        row["PlayableV4RulePass"] = len(actual_play) == PLAYABLE_COUNT and not diffs

        if not seq_ok or not time_ok:
            row["ErrorMessage"] = " | ".join(seq_issues + time_issues)
        if (
            seq_ok and time_ok and row["AnimationLookupRulePass"]
            and row["PlayableV4RulePass"]
        ):
            if row["Ribbons"] > 0:
                row["ConversionClass"] = "RIBBON_GOLDEN_REQUIRED"
            elif row["Particles"] > 0 and row["Animations"] == 1:
                row["ConversionClass"] = "PARTICLE_ONE_ANIMATION_GOLDEN_REQUIRED"
            elif row["Particles"] > 0:
                row["ConversionClass"] = "PARTICLE_COMPLEX_GOLDEN_REQUIRED"
            elif row["AliasSequences"] > 0 or row["SubAnimationSequences"] > 0:
                row["ConversionClass"] = "ANIMATION_METADATA_V4_READY_ALIAS_SUBANIM"
            else:
                row["ConversionClass"] = "ANIMATION_METADATA_V4_READY"
        else:
            row["ConversionClass"] = "METADATA_RULE_MISMATCH"
        return row
    except Exception as exc:
        row["ConversionClass"] = "SCAN_ERROR"
        row["ErrorMessage"] = f"{type(exc).__name__}: {exc}"
        return row


CSV_FIELDS = [
    "RelativePath", "Size335", "Size112", "Animations", "DistinctAnimIDs", "MaxAnimID",
    "SubAnimationSequences", "AliasSequences", "Bones", "Vertices", "Views", "Textures",
    "TexAnims", "Events", "Ribbons", "Particles", "ExternalAnimFiles",
    "SequenceMetadataMatch", "TimelineRulePass", "AnimationLookupRulePass",
    "PlayableCount", "PlayableV4RulePass", "PlayableMismatchIDs", "SequenceSignature",
    "ConversionClass", "ErrorMessage",
]


def write_csv(path: Path, rows: Sequence[dict], fields: Sequence[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8-sig") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)


def choose_samples(rows: Sequence[dict]) -> list[dict]:
    selected = []
    seen = set()

    def add(category, candidates, limit):
        count = 0
        for row in candidates:
            rel = row["RelativePath"].lower()
            if rel in seen:
                continue
            selected.append({"Category": category, "RelativePath": row["RelativePath"]})
            seen.add(rel)
            count += 1
            if count >= limit:
                break

    mismatches = sorted(
        [r for r in rows if r["ConversionClass"] in {"SCAN_ERROR", "METADATA_RULE_MISMATCH"}],
        key=lambda r: (r["Animations"], r["Particles"], r["Ribbons"], r["Vertices"]),
        reverse=True,
    )
    add("01_V41_Rule_Mismatch", mismatches, 8)

    ribbons = sorted(
        [r for r in rows if r["Ribbons"] > 0],
        key=lambda r: (r["Ribbons"], r["Particles"], r["TexAnims"], r["Animations"], r["Vertices"]),
        reverse=True,
    )
    add("02_True_Ribbon", ribbons, 6)

    particle_one = sorted(
        [r for r in rows if r["Particles"] > 0 and r["Animations"] == 1],
        key=lambda r: (r["Particles"], r["Vertices"]),
        reverse=True,
    )
    add("03_Particle_OneAnimation", particle_one, 4)

    particle_complex = sorted(
        [r for r in rows if r["Particles"] > 0 and r["Animations"] > 1],
        key=lambda r: (r["Particles"], r["AliasSequences"], r["SubAnimationSequences"], r["Animations"], r["Vertices"]),
        reverse=True,
    )
    add("04_Particle_Complex", particle_complex, 5)

    alias_sub = sorted(
        [r for r in rows if r["AliasSequences"] > 0 or r["SubAnimationSequences"] > 0],
        key=lambda r: (r["AliasSequences"], r["SubAnimationSequences"], r["Animations"], r["DistinctAnimIDs"]),
        reverse=True,
    )
    add("05_Alias_SubAnimation", alias_sub, 5)
    return selected


def copy_one(src_root: Path, rel: str, dst_root: Path, manifest: list[dict],
             category: str, sample_id: str, side: str, optional=False) -> None:
    src = src_root / Path(rel)
    if not src.exists():
        manifest.append({
            "Category": category, "SampleID": sample_id, "Side": side,
            "RelativePath": rel, "Status": "OPTIONAL_MISSING" if optional else "MISSING",
            "Size": "", "SHA256": "",
        })
        return
    dst = dst_root / Path(rel)
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    manifest.append({
        "Category": category, "SampleID": sample_id, "Side": side,
        "RelativePath": rel, "Status": "OK", "Size": src.stat().st_size, "SHA256": sha256(src),
    })


def copy_sample_side(root: Path, rel: str, dst: Path, category: str, sample_id: str,
                     side: str, views: int, manifest: list[dict]) -> None:
    copy_one(root, rel, dst, manifest, category, sample_id, side)
    m2 = root / Path(rel)
    if not m2.exists():
        return
    relp = Path(rel)
    stem = relp.stem
    for i in range(max(0, int(views))):
        srel = str(relp.with_name(f"{stem}{i:02d}.skin"))
        copy_one(root, srel, dst, manifest, category, sample_id, side, optional=True)
    try:
        for p in m2.parent.iterdir():
            if not p.is_file():
                continue
            if p.stem.lower().startswith(stem.lower()) and p.suffix.lower() in {".anim", ".blp"}:
                r = str(p.relative_to(root))
                copy_one(root, r, dst, manifest, category, sample_id, side, optional=True)
    except OSError:
        pass
    try:
        d = m2.read_bytes()
        h = parse_header(d)
        for tex in texture_paths(d, h):
            copy_one(root, tex, dst, manifest, category, sample_id, side, optional=True)
    except Exception:
        pass


def scan(source_root: Path, target_root: Path, out_root: Path, progress_every=250) -> tuple[list[dict], dict]:
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

    if not pairs:
        raise RuntimeError("no same-path M2 pairs found")

    pre = compare_pair(*pairs[0])
    if pre["ConversionClass"] == "SCAN_ERROR":
        raise RuntimeError(
            "preflight pair failed before full scan: "
            f"{pre['RelativePath']} -> {pre['ErrorMessage']}"
        )

    rows = []
    for idx, (s, t, rel) in enumerate(pairs, 1):
        rows.append(compare_pair(s, t, rel))
        if progress_every and idx % progress_every == 0:
            print(f"[scan] {idx}/{len(pairs)}", flush=True)

    meta = out_root / "STAGING" / "00_Metadata"
    meta.mkdir(parents=True, exist_ok=True)
    write_csv(meta / "M2_FeatureIndex_335_112_V41.csv", rows, CSV_FIELDS)

    counts = {
        "source_m2_total": len(source_files),
        "paired_m2": len(pairs),
        "missing_target_pair": missing,
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
    (meta / "M2_Scan_Summary_V41.json").write_text(
        json.dumps(counts, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    summary = "\n".join([
        "模型移植 Golden Reference V4.1 全库扫描",
        "",
        f"335 M2 总数：{counts['source_m2_total']}",
        f"存在成功112同路径 pair：{counts['paired_m2']}",
        f"无112同路径：{counts['missing_target_pair']}",
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
        "只有 scan_errors=0 时，其他 mismatch/feature 统计才允许作为结论。",
    ])
    (meta / "M2_Scan_Summary_V41.txt").write_text(summary, encoding="utf-8-sig")
    return rows, counts


def pack_selected(source_root: Path, target_root: Path, out_root: Path, rows: list[dict]) -> Path:
    stage = out_root / "STAGING"
    meta = stage / "00_Metadata"
    selected = choose_samples(rows)
    write_csv(meta / "Selected_Samples_V41.csv", selected, ["Category", "RelativePath"])
    row_by_rel = {r["RelativePath"].lower(): r for r in rows}
    manifest = []
    for number, s in enumerate(selected, 1):
        row = row_by_rel[s["RelativePath"].lower()]
        safe = Path(s["RelativePath"]).stem
        safe = "".join(ch if ch.isalnum() or ch in "_-" else "_" for ch in safe)
        sample_id = f"{number:02d}_{safe}"
        base = stage / s["Category"] / sample_id
        copy_sample_side(
            source_root, s["RelativePath"], base / "335", s["Category"], sample_id, "335",
            row["Views"], manifest,
        )
        copy_sample_side(
            target_root, s["RelativePath"], base / "112", s["Category"], sample_id, "112",
            row["Views"], manifest,
        )

    for side, base_root in (("335", source_root), ("112", target_root)):
        rel = "DBFilesClient/AnimationData.dbc"
        copy_one(
            base_root, rel, meta / side, manifest, "00_Metadata", "AnimationData", side, optional=True
        )

    write_csv(
        meta / "SELECTED_MANIFEST_SHA256_V41.csv", manifest,
        ["Category", "SampleID", "Side", "RelativePath", "Status", "Size", "SHA256"],
    )

    zip_path = out_root / "ModelPort_GoldenReference_ThirdBatch_V41_ALL.zip"
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for p in stage.rglob("*"):
            if p.is_file():
                z.write(p, p.relative_to(stage))
    return zip_path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--target", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--no-pack", action="store_true")
    args = ap.parse_args()

    source = Path(args.source)
    target = Path(args.target)
    out = Path(args.out)
    if not source.exists():
        raise SystemExit(f"source does not exist: {source}")
    if not target.exists():
        raise SystemExit(f"target does not exist: {target}")
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    rows, counts = scan(source, target, out)
    print(json.dumps(counts, ensure_ascii=False, indent=2))
    if counts["scan_errors"]:
        print(
            "[WARN] scan_errors > 0; inspect ErrorMessage before trusting global rules.",
            file=sys.stderr,
        )
    if not args.no_pack:
        z = pack_selected(source, target, out, rows)
        print(f"[ZIP] {z}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
