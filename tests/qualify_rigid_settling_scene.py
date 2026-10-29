#!/usr/bin/env python3
"""Same toppling inputs and error gates, with six seconds to observe settling.

The offset brick rocks until about3.3s. Retain the failed3..5s check; this
explicit4..6s observation preserves the two-second window and all tolerances.
"""
import sys
from qualify_rigid_six_brick_scene import main
from qualify_rigid_toppling_scene import asset_sizes,items,measure,projectile_velocity

name='six_brick_settling_v1'
limits_file='rigid_settling_limits.json'

if __name__=='__main__':raise SystemExit(main(sys.modules[__name__]))
