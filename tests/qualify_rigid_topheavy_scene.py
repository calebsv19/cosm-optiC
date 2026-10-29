#!/usr/bin/env python3
"""Reverse the previously qualified masses without changing poses or gates."""
from functools import partial
import math
from pathlib import Path
import sys
from qualify_rigid_resting_scene import load
from qualify_rigid_six_brick_scene import BRICKS,main
from qualify_rigid_toppling_scene import asset_sizes,items as equal_items,projectile_velocity
from qualify_rigid_unequal_mass_scene import mass_checks as check_masses,measure as measure_mass

name='six_brick_topheavy_v1'
limits_file='rigid_settling_limits.json'
contract=load(Path(__file__).with_name('rigid_topheavy_brick_masses.json'))
masses=contract['brick_mass_kg']
assert set(masses)==set(BRICKS) and len(set(masses.values()))==6
assert all(math.isfinite(m) and m>0 for m in masses.values()) and sum(masses.values())==12
mass_checks=partial(check_masses,mass_contract=contract)
measure=partial(measure_mass,mass_contract=contract)


def items():
    return [(oid,asset,bid,pos,masses.get(oid,mass),yaw)
            for oid,asset,bid,pos,mass,yaw in equal_items()]


if __name__=='__main__':raise SystemExit(main(sys.modules[__name__]))
