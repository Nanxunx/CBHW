import struct
from legacy_track_codec_v45 import WotLKTrack, build_sequence_windows, convert_track

w=build_sequence_windows([100,200])
assert [(x.start,x.end) for x in w] == [(3333,3433),(6766,6966)]

src=WotLKTrack(
    interpolation=1,
    global_sequence=-1,
    times=[[10],[20]],
    values=[[struct.pack('<f',2.0)],[struct.pack('<f',3.0)]],
    key_size=4,
)
dst=convert_track(src,w,struct.pack('<f',0.0))
assert dst.ranges == [(0,1),(2,3),(0,0)]
assert dst.times == [3343,3443,6786,6986]
assert [struct.unpack('<f',x)[0] for x in dst.values] == [2.0,2.0,3.0,3.0]

spline=WotLKTrack(2,-1,[[]],[[]],12)
one=build_sequence_windows([50])
out=convert_track(spline,one,struct.pack('<f',0.0))
assert len(out.values)==2 and all(len(v)==12 for v in out.values)

enabled=WotLKTrack(0,-1,[],[],1)
out=convert_track(enabled,one,b'\x01',empty_outer_mode='enabled_one')
assert out.ranges==[] and out.times==[0] and out.values==[b'\x01']
