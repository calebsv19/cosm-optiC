#!/usr/bin/env python3
"""Guard the reversed-only fixture and independently check native mass rows."""
import copy
import qualify_rigid_unequal_mass_scene as original
import qualify_rigid_topheavy_scene as reversed_mass


def main():
    assert [reversed_mass.masses[o] for o in original.BRICKS]==[original.masses[o] for o in reversed(original.BRICKS)]
    assert reversed_mass.limits_file==original.limits_file
    for before,after in zip(original.items(),reversed_mass.items()):
        assert before[:4]==after[:4] and before[5:]==after[5:]
    bindings=[]
    for oid,m in {'ground':0.,'projectile':8.,**reversed_mass.masses}.items():
        d=(.65,.22,.4) if oid in original.BRICKS else (.4,.4,.4)
        bindings.append({'object_id':oid,'mass_kg':m,'principal_inertia_kgm2':
                         [m*(d[(i+1)%3]**2+d[(i+2)%3]**2)/12 for i in range(3)]})
    assert all(reversed_mass.mass_checks(bindings)[0].values())
    assert not all(original.mass_checks(bindings)[0].values())
    bad=copy.deepcopy(bindings);bad[-1]['principal_inertia_kgm2'][0]*=2
    assert not all(reversed_mass.mass_checks(bad)[0].values())
    assert not all(reversed_mass.mass_checks(bindings+[bindings[0]])[0].values())
    print('reversed-only geometry/limits and mass/inertia/substitution controls pass')

if __name__=='__main__':main()
