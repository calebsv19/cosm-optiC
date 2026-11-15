#!/usr/bin/env python3
"""Native spectral room fixture and reproducible render helper; no denoising."""
import argparse,copy,json,math,shutil,subprocess,time,sys
from pathlib import Path
ROOT=Path.cwd()
REPO=Path(__file__).resolve().parents[1]
CLI=REPO/'build/tools/cli/ray_tracing_render_headless'
sys.path.insert(0,str(REPO/'tools'))
from optical_comparison_artifacts import bmp_rgba,png_bytes

def v(x,y,z):return dict(x=x,y=y,z=z)
def plane(name,pos,u,axis_v,normal,width,height):
    return {'object_id':name,'object_type':'plane_primitive','space_mode_intent':'2d',
            'dimensional_mode':'plane_locked','locked_plane':'xy',
            'transform':{'position':v(*pos),'rotation':v(0,0,0),'scale':v(1,1,1)},
            'primitive':{'kind':'plane_primitive','width':width,'height':height,
                        'frame':{'origin':v(*pos),'axis_u':v(*u),'axis_v':v(*axis_v),'normal':v(*normal)}},
            'flags':{'visible':True,'locked':False,'selectable':True}}
def material(name,rho,model='diffuse',roughness=.3):
    if isinstance(rho,(int,float)):rho=[[380,rho],[780,rho]]
    q={'object_id':name,'model':model,'reflectance':rho}
    if model=='ggx':q['roughness']=roughness
    return q

def scene_root(objects):
    return {'schema_family':'codework_scene','schema_variant':'scene_runtime_v1','schema_version':1,
            'scene_id':'spectral_room','space_mode_default':'3d','unit_system':'meters','world_scale':1,
            'objects':objects,'lights':[]}
def render(name,q,scene,mesh=True,timeout=360):
    d=ROOT/name;d.mkdir()
    (d/'config').symlink_to(REPO/'config',target_is_directory=True)
    (d/'scene.json').write_text(json.dumps(scene,indent=2))
    q=copy.deepcopy(q)
    q['scene']={'runtime_scene_path':'scene.json'}
    q['output']={'root':'output','overwrite':False}
    (d/'request.json').write_text(json.dumps(q,indent=2))
    if mesh:
        (d/'assets').mkdir()
        shutil.copy2(REPO/'tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_64x32.runtime.json',
                     d/'assets/sphere.runtime.json')
    t=time.monotonic()
    with (d/'log.txt').open('w') as log:
        subprocess.run([CLI,'--request','request.json','--render','--summary','summary.json','--summary-file-only'],
                        cwd=d,stdout=log,stderr=subprocess.STDOUT,timeout=timeout,check=True)
    result=json.loads((d/'summary.json').read_text())
    w,h,p=bmp_rgba(d/'output/frames/frame_0000.bmp')
    (d/'image.png').write_bytes(png_bytes(w,h,p))
    print(name,round(time.monotonic()-t,2),result['spectral_surface_path'],flush=True)
    return result

def room():
    objects=[
      plane('floor',(0,1,0),(1,0,0),(0,1,0),(0,0,1),2,2),
      plane('back',(0,2,1),(1,0,0),(0,0,1),(0,-1,0),2,2),
      plane('left',(-1,1,1),(0,1,0),(0,0,1),(1,0,0),2,2),
      plane('right',(1,1,1),(0,-1,0),(0,0,1),(-1,0,0),2,2),
      plane('ceiling',(0,1,2),(1,0,0),(0,-1,0),(0,0,-1),2,2)]
    # A tall opaque box produces a directly shadowed floor region.
    pos=v(.12,1.05,.42)
    objects.append({'object_id':'box','object_type':'rect_prism_primitive',
      'space_mode_intent':'3d','dimensional_mode':'full_3d',
      'transform':{'position':pos,'rotation':v(0,0,0),'scale':v(1,1,1)},
      'primitive':{'kind':'rect_prism_primitive','width':.48,'height':.48,'depth':.84,
                   'frame':{'origin':pos,'axis_u':v(1,0,0),'axis_v':v(0,1,0),'normal':v(0,0,1)}},
      'flags':{'visible':True,'locked':False,'selectable':True}})
    for name,pos,scale in [('matte_sphere',(-.51,.65,.28),.28),('gloss_sphere',(.52,.44,.23),.23)]:
        objects.append({'object_id':name,'object_type':'mesh_asset_instance','space_mode_intent':'3d',
          'dimensional_mode':'full_3d','transform':{'position':v(*pos),'rotation':v(0,0,0),
          'scale':v(scale,scale,scale)},'geometry_ref':{'kind':'mesh_asset','id':'asset_sphere_64x32','variant':'runtime_default'},
          'extensions':{'line_drawing':{'runtime_mesh_path':'assets/sphere.runtime.json'}},
          'flags':{'visible':True,'locked':False,'selectable':True}})
    orange=[[380,.035],[450,.035],[500,.09],[550,.35],[600,.78],[650,.82],[780,.82]]
    materials=[material('floor',.72),material('back',.72),material('left',orange),
               material('right',.72),material('ceiling',.72),material('box',.65),
               material('matte_sphere',.78),material('gloss_sphere',.75,'ggx',.18)]
    path={'seed':19,'materials':materials,
      'light':{'type':'area','origin_m':[0,.75,1.96],'axis_u':[1,0,0],'axis_v':[0,-1,0],
               'width_m':.65,'height_m':.65,'spectrum':{'continuum':[[380,.018],[780,.018]]}},
      'probes':[{'name':'center','x':48,'y':36}]}
    q={'schema_version':'ray_tracing_agent_render_request_v2','render':{'integrator_3d':'disney_v3_spectral',
        'width':96,'height':72},'inspection':{'camera_position':v(0,-2.7,1.15),
        'camera_look_at':v(0,1,.85),'camera_zoom':1.75},
       'spectral':{'exposure':3,'surface_path_v1':path}}
    return q,scene_root(objects)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description="Reproducible V3 spectral room with per-object reflectance")
    parser.add_argument('--cli',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--samples',type=int,default=512)
    parser.add_argument('--width',type=int,default=256)
    parser.add_argument('--height',type=int,default=192)
    parser.add_argument('--seed',type=int,default=19)
    parser.add_argument('--max-depth',type=int,help='Omission uses the pinned quality profile')
    parser.add_argument('--exposure',type=float,default=3)
    parser.add_argument('--view',choices=('room','shadow'),default='room')
    parser.add_argument('--wall',choices=('orange','neutral','black'),default='orange')
    parser.add_argument('--component',choices=('total','direct','surface_indirect','volume'),default='total')
    parser.add_argument('--fog',action='store_true',help='Add a homogeneous room medium')
    args=parser.parse_args()
    CLI=args.cli.resolve();ROOT=args.output.resolve();ROOT.mkdir(parents=True,exist_ok=True)
    q,sc=room();q['render'].update(width=args.width,height=args.height)
    path=q['spectral']['surface_path_v1']
    path.update(samples=args.samples,seed=args.seed,output_component=args.component,
                probes=[{'name':'center','x':args.width//2,'y':args.height//2}])
    if args.max_depth is not None:path['max_depth']=args.max_depth
    q['spectral']['exposure']=args.exposure
    if args.view=='shadow':
        q['inspection'].update(camera_position=v(-.55,1.8,.9),camera_look_at=v(.1,1.42,0),camera_zoom=1.5)
    if args.wall!='orange':path['materials'][2]=material('left',.72 if args.wall=='neutral' else 0)
    if args.fog:
        path['medium']={'bounds_min_m':[-.99,.01,.001],'bounds_max_m':[.99,1.99,1.99],
            'sigma_a_per_m':[[380,.015],[780,.015]],'sigma_s_per_m':[[380,.25],[780,.25]],'g':.25}
    name='-'.join((args.view,args.wall,args.component))+('-fog' if args.fog else '')
    render(name,q,sc,timeout=3600)
