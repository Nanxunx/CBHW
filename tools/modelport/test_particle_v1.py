#!/usr/bin/env python3
from particle_v1 import SequenceWindow, _flatten

w=[SequenceWindow(3333,3433)]
r,t,k=_flatten([[5]],[[b'ABCD']],w,b'\0'*4,False)
assert r==[(0,1),(0,0)]
assert t==[3338,3438]
assert k==[b'ABCD',b'ABCD']
r,t,k=_flatten([[]],[[]],w,b'\x01',False)
assert t==[3333,3433] and k==[b'\x01',b'\x01']
print('PASS particle v1')
