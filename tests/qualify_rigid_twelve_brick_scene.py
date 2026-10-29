#!/usr/bin/env python3
"""Twelve dynamic 2kg bricks, four crossed layers, unchanged settling gates."""
import math
import sys
from qualify_rigid_six_brick_scene import main, measure as base_measure
from qualify_rigid_toppling_scene import settling_checks, toppling_checks

name='twelve_brick_capacity_v1'
scene_id_prefix='twelve_brick_capacity_'
limits_file='rigid_settling_limits.json'
BRICKS=tuple('brick_'+str(i) for i in range(12))
asset_sizes=[('floor',(12,8,.4)),('brick',(.65,.22,.4)),('projectile',(.4,.4,.4))]
projectile_velocity=[4,0,1]

def items():
    result=[('ground','floor',17,[0,0,-.2],0,0)]
    for i,oid in enumerate(BRICKS):
        layer=i//3; side=(i%3-1)*.225
        result.append((oid,'brick',46+29*i,
            [side,0,.2+.4*layer] if layer%2 else [0,side,.2+.4*layer],
            2,math.pi/2 if layer%2 else 0))
    return result+[('projectile','projectile',900,[-1.7,0,1.35],8,0)]

def measure(cache,meshes,limits,impact):
    result=base_measure(cache,meshes,limits,impact,BRICKS)
    if not impact:return result
    for oid in BRICKS:result['checks'].pop(oid+' moves')
    result['checks'].pop('impulse_path_to_all_six')
    result=toppling_checks(cache,limits,result,BRICKS[9:])
    result['checks']['two_of_three_upper_bricks_overturn_and_fall']=result['checks'].pop('both_upper_bricks_overturn_and_fall')
    return settling_checks(cache,limits,result,BRICKS)

if __name__=='__main__':raise SystemExit(main(sys.modules[__name__]))
