#!/usr/bin/env python3
"""Opt-in native E0/E1 acceptance. All writes stay in a new supplied output root."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from smooth_mesh_reflection.prepare_reflection_matrix import build_request


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ppm(path):
    with path.open('rb') as file:
        assert file.readline().strip() == b'P6'
        line = file.readline()
        while line.startswith(b'#'):
            line = file.readline()
        width, height = map(int, line.split())
        assert file.readline().strip() == b'255'
        pixels = file.read()
    assert len(pixels) == width * height * 3
    return width, height, pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-root', type=Path, required=True)
    args = parser.parse_args()
    out = args.output_root.resolve()
    out.mkdir(parents=True, exist_ok=False)
    project = out / 'scene'
    shutil.copytree(ROOT / 'tests/fixtures/mesh_asset_runtime_spheres', project)
    scene = project / 'scene_runtime.json'
    before = json.loads(scene.read_text())
    probe = {'owner': 'acceptance', 'nested': [7, {'keep': 'unchanged'}]}
    before['extensions']['e0_preservation_probe'] = probe
    before['objects'][0].setdefault('flags', {})['u23_unknown'] = {'keep': [1, 2, 3]}
    scene.write_text(json.dumps(before, indent=2))
    runtime = out / 'data/runtime'
    runtime.mkdir(parents=True)
    (runtime / 'animation_config.json').write_text(json.dumps({
        'editorMode': 1, 'spaceMode': 1, 'windowWidth': 1280, 'windowHeight': 800,
        'inputRoot': str(ROOT / 'config'), 'outputRoot': str(runtime),
        'videoOutputRoot': str(out / 'videos')}))
    (runtime / 'scene_config.json').write_text(json.dumps({'window': {'width': 1280, 'height': 800}}))
    binary_root = ROOT / f'build/toolchains/clang/{platform.machine()}'
    binary = binary_root / 'tests/scene_editor_workspace_visual_test'
    source = ROOT / 'third_party/codework_shared/core/core_mesh_compile/tests/fixtures/imports/tetrahedron_ascii.stl'
    source_hash = digest(source)
    env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(ROOT))
    for stage, argument in [('edit', str(source)), ('fresh-reopen', '--reopen')]:
        with (out / f'{stage}.log').open('w') as log:
            subprocess.run([str(binary), str(out), str(scene), argument], env=env,
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    with (out / 'committed-move-reopen.log').open('w') as log:
        subprocess.run([str(binary), str(out), str(scene)+'.committed-move.json', '--move-reopen'],
                       env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    with (out / 'u23-flags-reopen.log').open('w') as log:
        subprocess.run([str(binary),str(out),str(scene)+'.u23-flags.json','--u23-flags'],env=env,
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
    flags_scene=json.loads(Path(str(scene)+'.u23-flags.json').read_text())
    flagged=next(o for o in flags_scene['objects'] if o.get('display_name')=='U2.3 review mesh')
    assert flagged['flags']['visible'] is False and flagged['flags']['locked'] is True
    transform_changes = {}
    for mode in ('rotate', 'scale'):
        for cancelled in (False, True):
            suffix = mode+'-preview' if cancelled else mode
            scene_suffix = f'.{mode}-preview.json' if cancelled else f'.committed-{mode}.json'
            with (out / f'{suffix}-reopen.log').open('w') as log:
                subprocess.run([str(binary),str(out),str(scene)+scene_suffix,'--'+suffix],
                               env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
        tw,th,tbefore=ppm(out / f'workspace_{mode}_before.ppm')
        tw2,th2,tlive=ppm(out / f'workspace_dense_{mode}_active.ppm')
        assert (tw,th)==(tw2,th2)
        pixels=sum(tbefore[i:i+3]!=tlive[i:i+3] for y in range(th//5,th-30)
                   for i in range((y*tw+tw//4)*3,(y*tw+3*tw//4)*3,3))
        assert pixels>2500, (mode,'No substantial live geometry preview',pixels)
        transform_changes[mode]=pixels
    scaled_scene = json.loads(scene.read_text())
    scaled_scene['world_scale'] = 2.0
    scaled_path = Path(str(scene)+'.world-scale.json')
    scaled_path.write_text(json.dumps(scaled_scene))
    with (out / 'world-scale.log').open('w') as log:
        subprocess.run([str(binary),str(out),str(scaled_path),'--world-scale'],env=env,
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
    preview_saved = json.loads(Path(str(scene)+'.preview-saved.json').read_text())
    assert abs(preview_saved['objects'][-1]['transform']['position']['x'] - 0.25) < 1e-9
    w, h, first = ppm(out / 'workspace_move_before.ppm')
    w2, h2, live = ppm(out / 'workspace_move_preview.ppm')
    assert (w,h)==(w2,h2)
    # Selected shaded geometry must move, not merely the small gizmo/outline.
    changed=sum(first[i:i+3]!=live[i:i+3] for y in range(h//5,h-30)
                for i in range((y*w+w//4)*3,(y*w+3*w//4)*3,3))
    assert changed > 2500, ('No substantial live geometry preview',changed)
    pw,ph,primitive_before=ppm(out / 'workspace_primitive_before.ppm')
    pw2,ph2,primitive_live=ppm(out / 'workspace_primitive_preview.ppm')
    assert (pw,ph)==(pw2,ph2)
    primitive_changed=sum(primitive_before[i:i+3]!=primitive_live[i:i+3] for y in range(ph//5,ph-30)
                          for i in range((y*pw+pw//4)*3,(y*pw+3*pw//4)*3,3))
    assert primitive_changed>2500, ('No substantial primitive preview',primitive_changed)

    after = json.loads(scene.read_text())
    assert after['extensions']['e0_preservation_probe'] == probe
    assert len(after['objects']) == len(before['objects']) + 1
    for original, retained in zip(before['objects'], after['objects']):
        assert original == retained, original['object_id']
    imported = after['objects'][-1]
    assert abs(imported['transform']['position']['x'] - 0.25) < 1e-6
    recipe = imported['extensions']['ray_tracing']['managed_mesh']['compiled']['recipe']
    assert recipe['import']['source_to_asset_scale'] == 0.001
    assert recipe['source_sha256'] == source_hash
    assert digest(source) == source_hash
    # The existing header must not be overdrawn by scrolled sidebar labels.
    width, height, initial = ppm(out / 'workspace_library_top.ppm')
    width2, height2, scrolled = ppm(out / 'workspace_library_scrolled.ppm')
    assert (width, height) == (width2, height2)
    header_bytes = width * round(72 * width / 1024) * 3  # Compact header, normal text scale.
    assert initial[:header_bytes] == scrolled[:header_bytes], 'Scrolled pane escaped its clipping boundary'
    request = build_request(project, 'e01', 'tlas_blas_parity')
    request['render'].update(width=320, height=200)
    request_path = project / 'request_e01.json'
    request_path.write_text(json.dumps(request, indent=2))
    with (out / 'render.log').open('w') as log:
        subprocess.run([str(binary_root / 'tools/cli/ray_tracing_render_headless'),
                        '--request', str(request_path), '--render'],
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    frame = project / 'renders/e01_tlas_blas_parity/frames/frame_0000.bmp'
    assert frame.exists() and len(set(frame.read_bytes()[122:])) > 4
    hidden_request=build_request(project,'u23_hidden','tlas_blas_parity')
    hidden_request['render'].update(width=320,height=200)
    hidden_request['scene']['runtime_scene_path']=str(scene)+'.u23-flags.json'
    hidden_request_path=project/'request_u23_hidden.json'
    hidden_request_path.write_text(json.dumps(hidden_request,indent=2))
    with (out/'hidden-render.log').open('w') as log:
        subprocess.run([str(binary_root/'tools/cli/ray_tracing_render_headless'),'--request',str(hidden_request_path),'--render'],
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
    hidden_frame=project/'renders/u23_hidden_tlas_blas_parity/frames/frame_0000.bmp'
    assert hidden_frame.exists() and digest(hidden_frame)!=digest(frame), 'Hidden mesh still renders identically'
    # Document commands must remain visible and stationary in every workspace.
    header_reference = None
    for profile in range(5):
        hw, hh, pixels = ppm(out / f'workspace_profile_{profile}.ppm')
        header = pixels[:hw * round(32 * hw / 1280) * 3]
        assert sum(channel > 90 for channel in header) > 1000, 'Document bar disappeared'
        if header_reference is None:
            header_reference = header
        else:
            assert header == header_reference, 'Workspace changed document bar rendering'
    report = {'status': 'passed', 'viewport_geometry_selection_scene_and_material': True, 'document_bar_stable_all_workspaces': True, 'workspace_menu_dismissal_and_keyboard': True, 'stable_id_selection_and_lock': True, 'visibility_lock_fresh_reopen': True, 'hidden_mesh_render_differs': True, 'rotation_scale_live_pixels': transform_changes,
              'rotation_scale_committed_and_cancelled_fresh_reopen': True,
              'scene_unit_world_scale_conversion': True, 'live_preview_changed_pixels': changed,
              'primitive_preview_changed_pixels': primitive_changed,
              'transactional_move_fresh_reopen': True, 'cancelled_preview_not_serialized': True, 'source_stl_sha256': source_hash,
              'unknown_fields_preserved': True, 'original_objects_preserved': True,
              'source_unit_scale': recipe['import']['source_to_asset_scale'],
              'imported_object_id': imported['object_id'], 'fresh_process_reopen': True,
              'header_clip_pixels_equal': True, 'render_sha256': digest(frame),
              'scene_sha256': digest(scene)}
    (out / 'acceptance.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
