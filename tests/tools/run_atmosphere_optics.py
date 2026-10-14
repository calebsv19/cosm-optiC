#!/usr/bin/env python3
"""Saved physical atmosphere: isolated native UI/MCP/reopen and Direct Light/Disney V2 proof."""
import argparse, copy, hashlib, json, os, shutil, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from scene_workflow_bundle import snapshot_inputs, seal, verify


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',type=Path,required=True)
    parser.add_argument('--cli',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--native-fixture',type=Path,help='Reuse a previously verified saved native fixture for headless-only checks')
    args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    fixture=ROOT/'tests/fixtures/ps4d_runtime_scene_visual_test.json';fixture_digest=hashlib.sha256(fixture.read_bytes()).hexdigest()
    work=out/'native';work.mkdir();shutil.copytree(ROOT/'config',work/'config')
    scene=work/'scene_runtime.json';document=json.loads(fixture.read_text())
    author=document.setdefault('extensions',{}).setdefault('ray_tracing',{}).setdefault('authoring',{})
    author['light_path']=dict(mode='BEZIER_CUBIC',points=[dict(x=-3,y=-4,velocity1=dict(vx=0,vy=0)),dict(x=-3,y=-4,velocity2=dict(vx=0,vy=0))])
    author['light_path_depth']=dict(points=[dict(z=4),dict(z=4)])
    author['light_settings']=dict(intensity=40,radius=0)
    author['environment']=dict(light_mode=2,ambient_strength=.12,environment_preset='Sky',background_brightness_auto=False,background_brightness=.5,background_color=dict(r=.68,g=.8,b=1))
    scene.write_text(json.dumps(document))
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(work),
             RAY_TRACING_ATMOSPHERE_HELPER=str(ROOT/'tools/scene_atmosphere_assets.py'),
             RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'mesh-cache'),
             OPTIC_ATMOSPHERE_MCP_TEST=str(ROOT/'tests/integration/check_atmosphere_optics_mcp.py'))
    runs=[]
    def native(mode):
        log=out/(mode.removeprefix('--')+'.log')
        with log.open('w') as stream:
            result=subprocess.run([str(args.native.resolve()),str(work),str(scene),mode],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=600)
        runs.append(dict(mode=mode,returncode=result.returncode));print(runs[-1],flush=True)
        if result.returncode:raise RuntimeError(log.read_text()[-4000:])
    if args.native_fixture:
        for directory in ('assets','data'):
            if (args.native_fixture/directory).exists():shutil.copytree(args.native_fixture/directory,work/directory)
        shutil.copy2(args.native_fixture/'scene_runtime.json',scene)
    else:
        for mode in ('--atmosphere-optics','--atmosphere-optics-reopen','--atmosphere-mcp','--atmosphere-optics-reopen'):native(mode)
    saved=json.loads(scene.read_text());value=saved['extensions']['ray_tracing']['authoring']['atmosphere_attachment'];optics=value['optics']
    assert optics['profile']=='physical_smoke_v1'
    cli=args.cli.resolve()
    def render(doc,name,inspection=None,volume=None,integrator='direct_light',refusal=None,directory=work):
        source=directory/(name+'.scene.json');source.write_text(json.dumps(doc))
        request=dict(schema_version='ray_tracing_agent_render_request_v1',run_id=name,
            scene=dict(runtime_scene_path=str(source)),volume=volume or dict(enabled=False),
            render=dict(start_frame=0,frame_count=1,width=96,height=64,temporal_frames=1,denoise_enabled=False,integrator_3d=integrator),
            output=dict(root=str(directory/name),overwrite=False))
        request['inspection']=dict(camera_position=dict(x=-3.8,y=-7.2,z=2.2),camera_look_at=dict(x=-.2,y=.8,z=1.2),**(inspection or {}))
        request_path=directory/(name+'.request.json');request_path.write_text(json.dumps(request));summary=directory/(name+'.summary.json');log=directory/(name+'.log')
        with log.open('w') as stream:
            result=subprocess.run([str(cli),'--request',str(request_path),'--render','--summary',str(summary),'--summary-file-only'],cwd=directory,env=dict(env,RAY_TRACING_PROGRAM_ROOT=str(directory)),stdout=stream,stderr=subprocess.STDOUT,timeout=120)
        if refusal:
            diagnostics=log.read_text()+(summary.read_text() if summary.exists() else '')
            assert result.returncode and refusal in diagnostics,diagnostics[-3000:]
            return None,None,request_path
        if result.returncode:raise RuntimeError(log.read_text()[-3000:]+(summary.read_text()[-3000:] if summary.exists() else ''))
        report=json.loads(summary.read_text());image=(directory/name/'frames/frame_0000.bmp').read_bytes()
        assert len(set(image[128:]))>8
        return image,report,request_path
    physical,report,intent=render(saved,'physical_saved')
    assert report['inspection']['saved_atmosphere_optics']==optics
    assert report['inspection']['volume_sampling_layout']=='cell_centered'
    inspection_scene=copy.deepcopy(saved);del inspection_scene['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['optics']
    kernel_optics={k:v for k,v in optics.items() if k not in ('profile','sampling_layout')};kernel_optics['schema']='physical_smoke_v1'
    reference,_,_=render(inspection_scene,'inspection_reference',dict(volume_smoke_optics=kernel_optics),dict(enabled=False,affects_lighting=True,sampling_layout='cell_centered'))
    assert physical==reference,'Saved settings differ from existing physical-kernel inspection path'
    legacy=copy.deepcopy(saved);legacy['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['optics']['profile']='legacy_artistic_v1'
    explicit_legacy,_,_=render(legacy,'legacy_profile')
    absent_legacy,_,_=render(inspection_scene,'legacy_absent')
    assert explicit_legacy==absent_legacy,'Explicit legacy profile changes old scene appearance'
    assert physical!=absent_legacy,'Optical selection has no image effect'
    for field,new in [('mass_extinction_m2_kg',0),('scattering_albedo',0),('anisotropy',-.4),('sky_illumination',True),('sampling_layout','legacy_node')]:
        changed=copy.deepcopy(saved);changed['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['optics'][field]=new
        image,_,_=render(changed,'change_'+field)
        if field not in ('sampling_layout',):assert image!=physical,(field,'no rendered effect')
    disney_inspection=dict(caustic_mode='off',caustic_sidecar_enabled=False,secondary_diffuse_samples_3d=1,transmission_samples_3d=1)
    disney,_,_=render(saved,'disney_saved',disney_inspection,integrator='disney_v2')
    disney_reference,_,_=render(inspection_scene,'disney_inspection_reference',dict(disney_inspection,volume_smoke_optics=kernel_optics),dict(enabled=False,affects_lighting=True,sampling_layout='cell_centered'),integrator='disney_v2')
    assert disney==disney_reference,'Saved Disney V2 differs from inspection physical policy'
    disney_legacy,_,_=render(legacy,'disney_legacy_profile',disney_inspection,integrator='disney_v2')
    disney_absent,_,_=render(inspection_scene,'disney_legacy_absent',disney_inspection,integrator='disney_v2')
    assert disney_legacy==disney_absent,'Explicit legacy profile changes Disney V2 appearance'
    assert disney!=disney_absent,'Physical Disney V2 selection has no rendered effect'
    for field in ('mass_extinction_m2_kg','scattering_albedo'):
        changed=copy.deepcopy(saved);changed['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['optics'][field]=0
        image,_,_=render(changed,'disney_change_'+field,disney_inspection,integrator='disney_v2')
        assert image!=disney,(field,'no Disney V2 rendered effect')
    render(saved,'sidecar_refusal',dict(caustic_mode='off',caustic_sidecar_enabled=True),integrator='disney_v2',refusal='requires direct_light or disney_v2 and caustics off')
    render(saved,'unsupported_integrator',integrator='diffuse_bounce',refusal='requires direct_light or disney_v2 and caustics off')
    render(saved,'override_refusal',dict(volume_density_scale=2),refusal='remove request optical overrides')
    render(saved,'sampling_refusal',volume=dict(enabled=False,sampling_layout='cell_centered'),refusal='remove request optical overrides')
    render(saved,'double_profile_refusal',dict(volume_smoke_optics=kernel_optics),refusal='remove request optical overrides')
    render(saved,'caustic_refusal',dict(caustic_mode='spatial_cache'),refusal='requires direct_light or disney_v2 and caustics off')
    package=out/'portable';snapshot_inputs(scene,intent,package);seal(package,'physical-optics-slice2',cli);assert verify(package)['verified']
    relocated_source=package/'scene_runtime.json'
    if not relocated_source.exists():
        # Snapshot returns its normalized runtime scene beneath inputs.
        manifest=json.loads((package/'render_request.json').read_text());relocated_source=(package/manifest['scene']['runtime_scene_path']).resolve()
    relocated=json.loads(relocated_source.read_text())
    # Render the sealed request directly so rewritten contained asset paths are consumed.
    sealed_request=package/'render_request.json';summary=out/'relocated-summary.json'
    execution=out/'portable-execution';execution.mkdir()
    for directory in ('config','data'):
        if (package/directory).exists():shutil.copytree(package/directory,execution/directory)
    with (out/'relocated.log').open('w') as stream:
        result=subprocess.run([str(cli),'--request',str(sealed_request),'--render','--summary',str(summary),'--summary-file-only'],cwd=execution,env=dict(env,RAY_TRACING_PROGRAM_ROOT=str(execution)),stdout=stream,stderr=subprocess.STDOUT,timeout=120)
    assert result.returncode==0,(out/'relocated.log').read_text()[-3000:]
    assert verify(package)['verified'],'Rendering modified sealed inputs'
    output=json.loads(sealed_request.read_text())['output']['root'];output=Path(output) if Path(output).is_absolute() else (sealed_request.parent/output).resolve()
    assert (output/'frames/frame_0000.bmp').read_bytes()==physical,'Relocation changed physical render'
    assert hashlib.sha256(fixture.read_bytes()).hexdigest()==fixture_digest
    result=dict(passed=True,native_runs=runs,native_fixture_reused=str(args.native_fixture) if args.native_fixture else None,physical_bmp_sha256=hashlib.sha256(physical).hexdigest(),disney_bmp_sha256=hashlib.sha256(disney).hexdigest(),fixture_sha256=fixture_digest,optics=optics,
        checks=([] if args.native_fixture else ['native controls and exact independent Direct Light pixels','UI/MCP shared state and history','fresh-process reopen'])+['existing physical-kernel parity','Disney V2 saved/inspection parity and rendered coefficient effects','legacy exact parity for Direct Light and Disney V2','rendered coefficient effects','unsupported/conflicting request refusals','sealed portable relocation'])
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)

if __name__=='__main__':main()
