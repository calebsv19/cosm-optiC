#!/usr/bin/env python3
"""Three crossed pairs: an undisturbed tower and a ballistic upper-layer strike.

Keeps the six-brick safety/rest gates. Impact must overturn both upper bricks
by at least 60 degrees AND lower each COM by 0.35m; translation is insufficient.
Impact also must meet the existing final-window rest limits for every brick.
No pins, sleeping, prescribed motion, or altered simulation parameters.
"""
import math
import sys
from qualify_rigid_six_brick_scene import BRICKS, main, measure as base_measure
from rigid_scene_qualification_math import dot, rotation

name='six_brick_toppling_v1'
asset_sizes=[('floor',(12,8,.4)),('brick',(.65,.22,.4)),('projectile',(.4,.4,.4))]
projectile_velocity=[4,0,1]


def items():
    result=[('ground','floor',17,[0,0,-.2],0,0)]
    for i,oid in enumerate(BRICKS):
        layer=i//2;side=(i%2-.5)*.225
        result.append((oid,'brick',46+29*i,
                       [side,0,.2+.4*layer] if layer%2 else [0,side,.2+.4*layer],
                       2,math.pi/2 if layer%2 else 0))
    return result+[('projectile','projectile',220,[-1.7,0,.95],8,0)]


def measure(cache,meshes,limits,impact):
    result=base_measure(cache,meshes,limits,impact)
    if not impact:return result
    # Lower supports may remain in place: a topple does not require all bricks
    # to slide or an impulse path through every support.
    checks=result['checks']
    for oid in BRICKS:checks.pop(oid+' moves')
    checks.pop('impulse_path_to_all_six')
    result=toppling_checks(cache,limits,result)
    return settling_checks(cache,limits,result)


def settling_checks(cache,limits,result,bricks=BRICKS):
    window=[f for f in cache['frames'] if f['tick']>=limits['rest_window_start_tick']]
    initial={b['object_id']:b for b in window[0]['bodies']}
    for oid in bricks:
        m=result['rest'][oid]
        for field,gate in [('window_drift_m','rest_window_drift_m'),
                           ('window_linear_speed_mps','rest_linear_speed_mps'),
                           ('window_angular_speed_radps','rest_angular_speed_radps'),
                           ('window_kinetic_energy_j','rest_kinetic_energy_j')]:
            result['checks'][oid+' settled '+field]=m[field]<=limits[gate]
        angle=max(2*math.acos(min(1,abs(dot(b['orientation_wxyz'],initial[oid]['orientation_wxyz']))))
                  for f in window for b in f['bodies'] if b['object_id']==oid)
        m['window_orientation_change_rad']=angle
        result['checks'][oid+' settled orientation']=angle<=limits['rest_orientation_change_rad']
    result['ok']=all(result['checks'].values())
    return result


def toppling_checks(cache,limits,result,upper_bricks=BRICKS[4:]):
    checks=result['checks']
    initial={p['object_id']:p for p in cache['frames'][0]['bodies']}
    bindings={b['object_id']:b for b in cache['physical_input']['bindings']}
    def up(oid,pose):
        # Source +Z in the principal body frame, then rotated into world space.
        axis=bindings[oid]['principal_to_source'][6:9]
        return [dot(row,axis) for row in rotation(pose['orientation_wxyz'])]
    qualified={};first=result['first_projectile_hit_tick']
    for oid in upper_bricks:
        events=[]
        for frame in cache['frames']:
            if first is None or frame['tick']<first:continue
            p=next(p for p in frame['bodies'] if p['object_id']==oid)
            angle=math.acos(max(-1,min(1,dot(up(oid,p),up(oid,initial[oid])))))
            drop=initial[oid]['position_m'][2]-p['position_m'][2]
            if angle>=limits['topple_angle_rad'] and drop>=limits['topple_height_loss_m']:
                events.append({'tick':frame['tick'],'tilt_rad':angle,'height_loss_m':drop})
        qualified[oid]=events[0] if events else None
    checks['both_upper_bricks_overturn_and_fall']=sum(v is not None for v in qualified.values())>=limits['minimum_toppled_upper_bricks']
    result.update(toppled_upper_bricks=qualified,ok=all(checks.values()))
    return result


if __name__=='__main__':raise SystemExit(main(sys.modules[__name__]))
