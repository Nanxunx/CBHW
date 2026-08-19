#!/usr/bin/env python3
from ribbon_v1 import SequenceWindow, _flatten_track

w=[SequenceWindow(3333,3433),SequenceWindow(6766,6966)]
r,t,k=_flatten_track([[0],[10,20]],[[b'A'],[b'B',b'C']],w,b'Z',False)
assert r==[(0,1),(2,3),(0,0)]
assert t==[3333,3433,6776,6786]
assert k==[b'A',b'A',b'B',b'C']
print('PASS ribbon v1')
