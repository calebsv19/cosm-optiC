#!/usr/bin/env python3
"""Same six-second tower/strikes and limits, with six distinct brick masses.

Bottom to top:3,2.5,2,1.75,1.5,1.25kg. Total brick mass remains12kg. Geometry,
contact law, initial poses and projectile are unchanged. Uniform-density box
inertia is independently checked rather than accepting a mass label alone.
"""
import math
from pathlib import Path
import sys
from qualify_rigid_resting_scene import load
from qualify_rigid_six_brick_scene import BRICKS,main
from qualify_rigid_toppling_scene import (asset_sizes,items as equal_items,
    measure as common_measure,projectile_velocity)

name='six_brick_unequal_mass_v1'
limits_file='rigid_settling_limits.json'
contract=load(Path(__file__).with_name('rigid_unequal_brick_masses.json'))
masses=contract['brick_mass_kg']
assert set(masses)==set(BRICKS) and len(set(masses.values()))==6
assert all(math.isfinite(m) and m>0 for m in masses.values()) and sum(masses.values())==12


def items():
    return [(oid,asset,bid,pos,masses.get(oid,mass),yaw)
            for oid,asset,bid,pos,mass,yaw in equal_items()]


def mass_checks(bindings,mass_contract=contract):
    masses=mass_contract['brick_mass_kg']
    expected={'ground':0.,'projectile':mass_contract['projectile_mass_kg'],**masses}
    by_id={b['object_id']:b for b in bindings};checks={};errors={}
    checks['eight_explicit_mass_bindings']=len(bindings)==len(expected) and set(by_id)==set(expected)
    for oid,mass in expected.items():
        b=by_id.get(oid,{})
        checks[oid+' declared mass']=b.get('mass_kg')==mass
        dimensions=(.65,.22,.4) if oid in BRICKS else (.4,.4,.4)
        oracle=sorted(mass*(dimensions[(i+1)%3]**2+dimensions[(i+2)%3]**2)/12 for i in range(3))
        actual=sorted(b.get('principal_inertia_kgm2',[]))
        error=max((abs(a-v) for a,v in zip(actual,oracle)),default=math.inf) if len(actual)==3 else math.inf
        errors[oid]=error;checks[oid+' analytic inertia']=error<=mass_contract['absolute_inertia_error_kgm2']
    return checks,errors


def measure(cache,meshes,limits,impact,mass_contract=contract):
    masses=mass_contract['brick_mass_kg']
    result=common_measure(cache,meshes,limits,impact)
    checks,errors=mass_checks(cache['physical_input']['bindings'],mass_contract);result['checks'].update(checks)
    # Independent initial mechanical energy: fixed source COM heights and launch.
    expected=9.81*(sum(masses['brick_'+str(i)]*(.2+.4*(i//2)) for i in range(6))
                   +mass_contract['projectile_mass_kg']*.95)+(68 if impact else 0)
    result['checks']['independent initial energy']=abs(result['initial_energy_j']-expected)<=mass_contract['initial_energy_error_j']
    result['mass_audit']={'expected_brick_mass_kg':masses,'total_brick_mass_kg':sum(masses.values()),
                          'inertia_error_kgm2':errors,'expected_initial_energy_j':expected}
    result['ok']=all(result['checks'].values());return result


if __name__=='__main__':raise SystemExit(main(sys.modules[__name__]))
