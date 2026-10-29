#!/usr/bin/env python3
"""Reject mass substitutions that could falsely pass a total-mass-only audit."""
import copy
from qualify_rigid_unequal_mass_scene import mass_checks

def main():
    expected={'ground':0.,'projectile':8.,'brick_0':3.,'brick_1':2.5,
              'brick_2':2.,'brick_3':1.75,'brick_4':1.5,'brick_5':1.25}
    bindings=[]
    for oid,m in expected.items():
        d=(.65,.22,.4) if oid.startswith('brick_') else (.4,.4,.4)
        bindings.append({'object_id':oid,'mass_kg':m,'principal_inertia_kgm2':
                         [m*(d[(i+1)%3]**2+d[(i+2)%3]**2)/12 for i in range(3)]})
    assert all(mass_checks(bindings)[0].values())
    uniform=copy.deepcopy(bindings)
    for b in uniform:
        if b['object_id'].startswith('brick_'):b['mass_kg']=2.
    assert not all(mass_checks(uniform)[0].values())
    swapped=copy.deepcopy(bindings);swapped[2]['mass_kg'],swapped[3]['mass_kg']=swapped[3]['mass_kg'],swapped[2]['mass_kg']
    assert not all(mass_checks(swapped)[0].values())
    bad=copy.deepcopy(bindings);bad[2]['principal_inertia_kgm2'][0]*=2
    assert not all(mass_checks(bad)[0].values())
    print('unequal mass contract: positive case and three substitution controls passed')
if __name__=='__main__':main()
