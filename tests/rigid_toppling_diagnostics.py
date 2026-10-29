#!/usr/bin/env python3
"""Independent motion/budget audit; settling is a separate reported outcome.

Contact iteration-activity scalars are NOT external impulse vectors. The optional
native observer reconstructs net impulses, including warm starts, from solved
cache points; compare them to accepted mass*velocity changes plus gravity.
"""
import argparse
import json
from pathlib import Path
from qualify_rigid_resting_scene import kinetic,load,write
from qualify_rigid_six_brick_scene import BRICKS
from rigid_scene_qualification_math import norm


def audit(cache,metrics,limits,momentum=None,upper_bricks=BRICKS[4:]):
    bindings={b['object_id']:b for b in cache['physical_input']['bindings']}
    names={b['body_id']:b['object_id'] for b in bindings.values()}
    dt=1/limits['rate_hz'];g=cache['physical_input']['gravity_mps2'];frames=cache['frames']
    poses=[{p['object_id']:p for p in f['bodies']} for f in frames]
    energy=[sum(kinetic(p,bindings[o])+bindings[o]['mass_kg']*9.81*p['position_m'][2]
                for o,p in row.items() if not p['static']) for row in poses]
    first=metrics['first_projectile_hit_tick'];flight={}
    for oid in upper_bricks:
        start=None;best=None;error=0.
        for i,f in enumerate(frames[1:],1):
            touched=any(oid in (names[c['body_a_id']],names[c['body_b_id']]) for c in f['contacts'])
            eligible=first is not None and i>first and not touched
            err=max(abs(poses[i][oid]['velocity_mps'][k]-poses[i-1][oid]['velocity_mps'][k]-g[k]*dt) for k in range(3))
            if eligible and err<=limits['free_flight_velocity_error_mps']:
                if start is None:start=i
                if best is None or i-start+1>best[1]-best[0]+1:best=(start,i)
                error=max(error,err)
            else:start=None
        flight[oid]={'longest_contact_free_ballistic_ticks':0 if best is None else best[1]-best[0]+1,
                     'interval_ticks':best,'maximum_velocity_step_error_mps':error}
    ballistic=None
    if first is not None:
        p0=poses[0]['projectile'];ve=pe=0.
        for t in range(1,first):
            p=poses[t]['projectile']
            ve=max(ve,max(abs(p['velocity_mps'][k]-p0['velocity_mps'][k]-g[k]*dt*t) for k in range(3)))
            pe=max(pe,max(abs(p['position_m'][k]-p0['position_m'][k]-p0['velocity_mps'][k]*dt*t-g[k]*dt*dt*t*(t+1)/2) for k in range(3)))
        ballistic={'preimpact_velocity_error_mps':ve,'semiimplicit_position_error_m':pe,'ok':max(ve,pe)<=limits['free_flight_velocity_error_mps']}
    settling={o:{'position_drift_m':m['window_drift_m'],'linear_speed_mps':m['window_linear_speed_mps'],
                  'angular_speed_radps':m['window_angular_speed_radps'],
                  'position_within_existing_rest_gate':m['window_drift_m']<=limits['rest_window_drift_m'],
                  'velocity_within_existing_rest_gates':m['window_linear_speed_mps']<=limits['rest_linear_speed_mps'] and m['window_angular_speed_radps']<=limits['rest_angular_speed_radps']}
              for o,m in metrics['rest'].items()}
    budget=None
    if momentum is not None:
        rows=[json.loads(line) for line in momentum.read_text().splitlines()];assert len(rows)==len(frames)-1
        residual=parity=0.;total=[0.,0.,0.];gravity=[0.,0.,0.]
        def p(row):return [sum(bindings[o]['mass_kg']*b['velocity_mps'][k] for o,b in row.items() if not b['static']) for k in range(3)]
        for i,row in enumerate(rows,1):
            assert row['tick']==i
            before=p(poses[i-1]);after=p(poses[i])
            parity=max(parity,max(abs(a-b) for a,b in zip(before,row['before_kg_mps'])),max(abs(a-b) for a,b in zip(after,row['after_kg_mps'])))
            for k in range(3):
                gj=row['dynamic_mass_kg']*g[k]*dt;reaction=row['static_reaction_kg_mps'][k]
                residual=max(residual,abs(after[k]-before[k]-gj-reaction));total[k]+=reaction;gravity[k]+=gj
        budget={'maximum_step_component_residual_kg_mps':residual,'accepted_momentum_parity_error_kg_mps':parity,
                'static_floor_impulse_on_dynamic_bodies_kg_mps':total,'gravity_impulse_kg_mps':gravity,
                'initial_dynamic_momentum_kg_mps':p(poses[0]),'final_dynamic_momentum_kg_mps':p(poses[-1]),
                'ok':residual<=limits['momentum_step_error_kg_mps'] and parity<=limits['momentum_parity_error_kg_mps'],
                'boundary':'Floor held fixed by external world; its opposite reaction and gravity are external. Internal dynamic-pair impulses cancel. Public scalar contact activity is not a signed budget.'}
    return {'toppling_qualification_passed':metrics['ok'],'unsupported_ballistic_intervals':flight,
            'contact_free_support_loss_observed':first is not None and all(f['longest_contact_free_ballistic_ticks']>=limits['minimum_unsupported_ticks'] for f in flight.values()),
            'preimpact_ballistic_reference':ballistic,'energy_j':{'initial':energy[0],'final':energy[-1],'maximum':max(energy),'largest_positive_step':max(b-a for a,b in zip(energy,energy[1:]))},
            'settling_final_two_seconds':settling,'all_brick_positions_within_existing_rest_window_gate':all(v['position_within_existing_rest_gate'] for v in settling.values()),
            'momentum_budget':budget}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for v in ['cache','metrics','limits','out']:ap.add_argument('--'+v,type=Path,required=True)
    ap.add_argument('--momentum',type=Path);a=ap.parse_args()
    if a.out.exists():ap.error('create-only output exists')
    result=audit(load(a.cache),load(a.metrics),load(a.limits),a.momentum);write(a.out,result)
    return 0 if result['momentum_budget'] is None or result['momentum_budget']['ok'] else 2


if __name__=='__main__':raise SystemExit(main())
