#!/usr/bin/env python3
from pathlib import Path
import struct
import tempfile
from playable_lookup_v45 import graph_from_build12340_animation_data, build_playable_records


def make_dbc(path: Path):
    records=226; recsize=32
    d=bytearray(20+records*recsize+1)
    d[:4]=b'WDBC'
    struct.pack_into('<IIII',d,4,records,8,recsize,1)
    for i in range(records):
        o=20+i*recsize
        struct.pack_into('<I',d,o,i)
        struct.pack_into('<I',d,o+20,0)
    for a,b in ((170,19),(19,18),(18,17),(17,16),(146,148)):
        struct.pack_into('<I',d,20+a*recsize+20,b)
    path.write_bytes(d)

with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'AnimationData.dbc'
    make_dbc(p)
    g=graph_from_build12340_animation_data(p)
    assert g[146]==0 and g[172]==16 and g[181]==19
    assert build_playable_records([0,16],g)[170]==(16,0)
    assert build_playable_records([0,19],g)[170]==(19,0)
print('PASS playable v45')
