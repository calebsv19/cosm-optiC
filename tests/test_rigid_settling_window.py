#!/usr/bin/env python3
"""Real pre-repair toppling cache must fail quiet-position acceptance.

This catches the former false inference from nearly zero velocity. Callers
supply an independently retained cache/metrics pair; no scene is overwritten.
"""
import argparse
from pathlib import Path
from qualify_rigid_resting_scene import load,write
from qualify_rigid_toppling_scene import settling_checks


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for key in ('cache','metrics','out'):ap.add_argument('--'+key,type=Path,required=True)
    a=ap.parse_args()
    if a.out.exists():ap.error('create-only output exists')
    cache=load(a.cache);metrics=load(a.metrics)
    limits=load(Path(__file__).with_name('rigid_toppling_limits.json'))
    assert metrics['ok'],'baseline must pass its older toppling-only gates'
    result=settling_checks(cache,limits,metrics)
    failed=[k for k,v in result['checks'].items() if not v]
    assert not result['ok'] and any('settled window_drift_m' in k for k in failed)
    assert all(m['window_linear_speed_mps']<=limits['rest_linear_speed_mps'] and
               m['window_angular_speed_radps']<=limits['rest_angular_speed_radps'] for m in result['rest'].values())
    write(a.out,{'ok':True,'zero_velocity_does_not_imply_settled':True,'expected_failed_gates':failed})
    print('pre-repair toppling passes its old checks but correctly fails quiet-position acceptance')


if __name__=='__main__':raise SystemExit(main())
