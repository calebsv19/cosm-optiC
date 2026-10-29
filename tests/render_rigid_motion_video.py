#!/usr/bin/env python3
"""Render32 exact16fps samples, verify an MP4, then retire its raw frames.

Presentation clone only. The immutable source result is referenced read-only.
No simulation runs, interpolation, or result promotion takes place here.
"""
import argparse,hashlib,json,os,subprocess,shutil
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for n in ['scene','renderer','out']:ap.add_argument('--'+n,type=Path,required=True)
    ap.add_argument('--camera-position',nargs=3,type=float,default=(2.6,3.2,2))
    ap.add_argument('--camera-look-at',nargs=3,type=float,default=(.25,0,.5))
    a=ap.parse_args();root=a.out.resolve();root.mkdir(parents=True,exist_ok=False)
    source=a.scene.resolve();parent=source.parent
    original={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source,parent/'result/poses.pack']}
    scene=json.loads(source.read_text());scene['extensions']['ray_tracing']['authoring']['scene_timeline']={
        'version':1,'rate':{'numerator':16,'denominator':1},'range':{'start_frame':0,'frame_count':32},'tracks':[]}
    # Native bundle admission requires contained regular files, not a symlink.
    shutil.copytree(parent/'result',root/'result')
    (root/'scene.json').write_text(json.dumps(scene,indent=2)+'\n')
    req={'schema_version':'ray_tracing_agent_render_request_v1','run_id':'rigid_video16',
         'scene':{'runtime_scene_path':str(root/'scene.json')},'volume':{'enabled':False},
         'render':{'start_frame':0,'frame_count':32,'width':512,'height':384,'integrator_3d':'direct_light','temporal_frames':1,'normalized_t':0},
         'resources':{'cpu_percent':40,'max_workers':2,'reserve_cpu_count':1},
         'inspection':{'camera_position':dict(zip('xyz',a.camera_position)),'camera_look_at':dict(zip('xyz',a.camera_look_at)),'environment_light_mode':'ambient','ambient_strength':.3},
         'output':{'root':str(root/'render'),'overwrite':False},'progress':{'summary_path':str(root/'summary.json'),'progress_path':str(root/'progress.json')}}
    request=root/'request.json';request.write_text(json.dumps(req,indent=2)+'\n')
    env=dict(os.environ,XDG_CONFIG_HOME=str(root/'config'),RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled')
    def call(cmd):return subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=600)
    # Verify physical-time sampling before spending the sequence render.
    audits=[]
    for frame in [0,5,16,31]:
        q=json.loads(json.dumps(req));q['render'].update(start_frame=frame,frame_count=1);q['progress']['summary_path']=str(root/f'audit-{frame}.json');p=root/f'audit-{frame}-request.json';p.write_text(json.dumps(q))
        r=call([a.renderer,'--request',p,'--preflight']);assert r.returncode==0,r.stderr+r.stdout
        j=json.loads((root/f'audit-{frame}.json').read_text());poses=j['evaluated_objects'];assert len(poses)>=8
        dynamic=[o for o in poses if o.get('source_tick') is not None];assert dynamic and all(o['source_tick']==frame*15 for o in dynamic),(frame,dynamic)
        audits.append({'frame':frame,'source_tick':frame*15,'result_revisions':sorted(set(o['result_revision'] for o in dynamic))})
    with (root/'renderer.log').open('w') as log:
        r=subprocess.run([str(a.renderer),'--request',str(request),'--render'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
    assert r.returncode==0
    summary=json.loads((root/'summary.json').read_text());assert summary['frames_rendered']==32
    frames=sorted((root/'render/frames').glob('frame_*.bmp'));assert len(frames)==32
    hashes=[hashlib.sha256(p.read_bytes()).hexdigest() for p in frames];assert len(set(hashes))>8
    video=root/'motion-16fps.mp4'
    r=call(['ffmpeg','-v','error','-nostdin','-framerate','16','-i',root/'render/frames/frame_%04d.bmp','-frames:v','32','-c:v','libx264','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',video]);assert r.returncode==0,r.stderr
    r=call(['ffprobe','-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,avg_frame_rate,nb_read_frames,duration','-of','json',video]);assert r.returncode==0
    meta=json.loads(r.stdout)['streams'][0];assert int(meta['nb_read_frames'])==32 and meta['avg_frame_rate']=='16/1' and abs(float(meta['duration'])-2)<1e-6
    r=call(['ffmpeg','-v','error','-nostdin','-i',video,'-f','null','-']);assert r.returncode==0,r.stderr
    # Keep a poster; the raw frame sequence is disposable once video decodes.
    r=call(['ffmpeg','-v','error','-nostdin','-ss','0.875','-i',video,'-frames:v','1',root/'poster.png']);assert r.returncode==0,r.stderr
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in original.items())
    removed=sum(p.stat().st_size for p in frames);shutil.rmtree(root/'render/frames')
    (root/'verification.json').write_text(json.dumps({'ok':True,'frame_count':32,'fps':16,'duration_s':2,'physical_sample_times_s':[0,31/16],'source_tick_step':15,'selected_audits':audits,'decoded_stream':meta,'unique_raw_frame_hashes':len(set(hashes)),'frame_sha256':hashes,'mp4_sha256':hashlib.sha256(video.read_bytes()).hexdigest(),'raw_frames_deleted':True,'removed_raw_frame_bytes':removed,'parent_hashes_unchanged':original},indent=2)+'\n')
    print(video,flush=True)
if __name__=='__main__':main()
