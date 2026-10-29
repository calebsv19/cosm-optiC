#!/usr/bin/env python3
"""Negative controls for overturning: yaw, sliding and pre-impact falls refuse."""
import copy
import math
from pathlib import Path
from qualify_rigid_resting_scene import load
from qualify_rigid_toppling_scene import toppling_checks

limits=load(Path(__file__).with_name('rigid_toppling_limits.json'))
ids=('brick_4','brick_5')
initial=[{'object_id':o,'orientation_wxyz':[1,0,0,0],'position_m':[0,0,1]} for o in ids]
cache={'physical_input':{'bindings':[{'object_id':o,'principal_to_source':[1,0,0,0,1,0,0,0,1]} for o in ids]},
       'frames':[{'tick':0,'bodies':initial},{'tick':100,'bodies':copy.deepcopy(initial)}]}
s,c=math.sin(math.radians(35)),math.cos(math.radians(35))
for p in cache['frames'][1]['bodies']:p.update(orientation_wxyz=[c,s,0,0],position_m=[1,0,.4])
def passes(value,first=50):
    return toppling_checks(value,limits,{'checks':{},'first_projectile_hit_tick':first})['ok']
assert passes(cache)
for mode in ('yaw_only','slide_only','tilt_without_fall','one_brick_only'):
    candidate=copy.deepcopy(cache)
    for p in candidate['frames'][1]['bodies']:
        if mode=='yaw_only':p['orientation_wxyz']=[c,0,0,s]
        elif mode=='slide_only':p['orientation_wxyz']=[1,0,0,0]
        elif mode=='tilt_without_fall':p['position_m'][2]=1
        elif mode=='one_brick_only' and p['object_id']=='brick_5':p['orientation_wxyz']=[1,0,0,0]
    assert not passes(candidate),mode
assert not passes(cache,first=101)
assert not passes(cache,first=None)
# Principal axes need not coincide with source axes. Rotate the metadata frame
# 90 degrees about Y and express the same physical upright/yaw/tilt in it.
from qualify_rigid_six_brick_scene import multiply
q=[math.sqrt(.5),0,math.sqrt(.5),0]
for axis,expected in [([c,s,0,0],True),([c,0,0,s],False)]:
    candidate=copy.deepcopy(cache)
    for b in candidate['physical_input']['bindings']:b['principal_to_source']=[0,0,1,0,1,0,-1,0,0]
    for p in candidate['frames'][0]['bodies']:p['orientation_wxyz']=q
    for p in candidate['frames'][1]['bodies']:p['orientation_wxyz']=multiply(axis,q)
    assert passes(candidate)==expected
print('toppling qualification: 9 positive/negative controls pass')
