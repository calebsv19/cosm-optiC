#!/usr/bin/env python3
"""Analyze native matched images and produce static scientific plots/gallery.

Requires matplotlib and Pillow for presentation only. Rendering uses the
separate dependency-free testbench and unmodified optiC CLI.
"""
import argparse
import csv
import html
import json
import math
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image, ImageDraw, ImageFont

from optical_comparison_artifacts import bmp_rgba
import spectral_response_fixture as fixture
from spectral_response_testbench import digest

LABELS = {'white_broad': 'Broad white', 'white_three_peak': 'Three-peak white',
    'warm_2800': 'Warm 2800 K shape', 'green_broad': 'Broad green bias',
    'blue_broad': 'Broad blue bias', 'violet_405': 'Violet 405 nm band',
    'blue_450': 'Blue 450 nm band', 'green_535': 'Green 535 nm band'}
ORDER = ('white_broad', 'white_three_peak', 'warm_2800', 'green_broad',
    'blue_broad', 'violet_405', 'blue_450', 'green_535')
COLORS = ('#444444', '#c63863', '#ce762b', '#4f973c', '#4689bd', '#a04ed4', '#3358cc', '#279a6e')


def summary(root, name):
    return json.loads((root/'final'/name/'summary.json').read_text())


def verify_cases(root, plan):
    cases = {'room-'+n: (n, 'orange', 'room', 'total', 19) for n in ORDER}
    cases.update({'indirect-'+n:(n,'orange','room','surface_indirect',19)
        for n in ('white_broad','white_three_peak','green_broad','blue_broad')})
    cases.update({'seed-'+n:(n,'orange','room','total',104729)
        for n in ('white_broad','white_three_peak','green_broad')})
    cases.update({'shadow-'+n:('white_broad',n,'shadow','total',19)
        for n in ('orange','neutral','green','black','amber')})
    cases.update({'shadow-metamer-'+n:('white_three_peak',n,'shadow','total',19)
        for n in ('neutral','amber')})
    cases['shadow-direct'] = ('white_broad','orange','shadow','direct',19)
    hashes = set(); total_paths = 0
    for name, (source, wall, view, component, seed) in cases.items():
        d = root/'final'/name
        receipt = json.loads((d/'receipt.json').read_text())
        assert receipt['status'] == 'completed'
        for key, file in [('request_sha256','request.json'),('scene_sha256','scene.json'),
                ('summary_sha256','summary.json'),('image_sha256','output/frames/frame_0000.bmp')]:
            assert receipt[key] == digest(d/file), (name, key)
        hashes.add(receipt['runtime_sha256'])
        w,h,samples = (256,192,1536) if view=='shadow' else (320,240,1024)
        expected_request, expected_scene = fixture.fixture(plan['source_curves'][source], wall,
            w,h,samples,seed,component,view)
        q = json.loads((d/'request.json').read_text());q.pop('scene');q.pop('output')
        assert q == expected_request and json.loads((d/'scene.json').read_text()) == expected_scene, name
        s = summary(root,name);r = s['spectral_transport'];f = r['film_components_xyz']
        assert r['status']=='completed' and r['route']=='surface_path_v1' and not r['photon_maps_used']
        assert r['quality']['samples_per_pixel']==samples and r['quality']['max_surface_interactions']==12
        assert r['quality']['exposure']==3 and r['quality']['component']==component
        assert f['committed_pixels']==w*h and f['volume']==[0,0,0]
        assert all(math.isclose(f['direct'][j]+f['surface_indirect'][j], f['total'][j], rel_tol=1e-12) for j in range(3))
        assert r['termination']['camera_paths']==w*h*samples
        total_paths += r['termination']['camera_paths']
        bw,bh,rgba = bmp_rgba(d/'output/frames/frame_0000.bmp')
        png = Image.open(d/'image.png').convert('RGBA')
        assert (bw,bh)==(w,h)==png.size and rgba==png.tobytes()
        if view=='shadow':
            assert s['spectral_surface_path']['probes'][0]['direct_xyz']==[0,0,0]
    assert len(hashes)==1
    pairs = [('room-'+n,'indirect-'+n) for n in ('white_broad','white_three_peak','green_broad','blue_broad')]
    pairs.append(('shadow-orange','shadow-direct'))
    for a,b in pairs:
        x,y = summary(root,a),summary(root,b)
        assert x['spectral_surface_path']==y['spectral_surface_path']
        assert x['spectral_transport']['film_components_xyz']==y['spectral_transport']['film_components_xyz']
        assert x['spectral_transport']['film']==y['spectral_transport']['film']
    return dict(cases=len(cases), same_runtime_sha256=hashes.pop(), camera_paths=total_paths,
        exact_native_png_pixels=True, exact_component_noninterference_pairs=len(pairs),
        all_shadow_centers_direct_zero=True, recipe_and_output_hashes_verified=True)


def patch_regions(root, case):
    scene = json.loads((root/'final'/case/'scene.json').read_text())
    film = summary(root, case)['spectral_transport']['film']
    origin = np.array(film['camera_origin_m'])
    forward, right, up = (np.array(film[k]) for k in ('camera_forward', 'camera_right', 'camera_up'))
    result = {}
    for obj in scene['objects']:
        if not obj['object_id'].startswith('swatch_'):
            continue
        p = obj['primitive'];frame = p['frame']
        vector = lambda v: np.array([v[k] for k in ('x','y','z')])
        at, u, v = (vector(frame[k]) for k in ('origin','axis_u','axis_v'))
        corners = []
        for a in (-.5, .5):
            for b in (-.5, .5):
                d = at + a*p['width']*u + b*p['height']*v-origin
                t = np.dot(d, forward)
                corners.append(((1+np.dot(d,right)/(t*film['tan_half_fov_x']))*film['width']/2-.5,
                    (1-np.dot(d,up)/(t*film['tan_half_fov_y']))*film['height']/2-.5))
        # Inset the projected quadrilateral to exclude silhouettes/antialiasing.
        x0 = math.ceil(min(p[0] for p in corners))+2
        x1 = math.floor(max(p[0] for p in corners))-2
        y0 = math.ceil(min(p[1] for p in corners))+2
        y1 = math.floor(max(p[1] for p in corners))-2
        assert 0 <= x0 < x1 < film['width'] and 0 <= y0 < y1 < film['height']
        result[obj['object_id'][7:]] = [x0,y0,x1,y1]
    return result


def sheet(root, cells, name, columns=4, subtitle='Same camera, exposure and scene; native pixels, no denoising'):
    images = [Image.open(root/'final'/cell/'image.png').convert('RGB') for cell, _ in cells]
    width, height = images[0].size
    assert all(im.size == (width, height) for im in images)
    font_path = '/System/Library/Fonts/Supplemental/Arial.ttf'
    font = ImageFont.truetype(font_path, 17) if Path(font_path).exists() else ImageFont.load_default()
    small = ImageFont.truetype(font_path, 13) if Path(font_path).exists() else font
    gap, title_h, header = 14, 49, 44
    rows = math.ceil(len(cells)/columns)
    out = Image.new('RGB', (columns*(width+gap)+gap, rows*(height+title_h+gap)+header), '#141820')
    draw = ImageDraw.Draw(out)
    draw.text((gap, 12), subtitle, font=small, fill='#f2f4f7')
    for i, ((case, label), im) in enumerate(zip(cells, images)):
        x, y = gap+(i % columns)*(width+gap), header+(i//columns)*(height+title_h+gap)
        draw.text((x, y), label, font=font, fill='#f2f4f7')
        draw.text((x, y+23), case, font=small, fill='#aab5c5')
        out.paste(im, (x, y+title_h))
    out.save(root/name)


def spectral_plots(root, plan):
    plt.rcParams.update({'font.size': 10, 'axes.spines.top': False, 'axes.spines.right': False})
    fig, axes = plt.subplots(2, 1, figsize=(12, 7), constrained_layout=True)
    for source_name, color in zip(ORDER, COLORS):
        curve = plan['source_curves'][source_name]
        nm, values = np.array(curve).T
        axes[0].plot(nm, values/max(values), label=LABELS[source_name], color=color)
    axes[0].set(xlim=(380, 780), ylabel='SPD / own peak', title='Declared source shapes — all rendered at equal incident CIE Y')
    fig.legend(*axes[0].get_legend_handles_labels(), loc='outside upper center', ncol=4, fontsize=9)
    for name, curve in plan['reflectances'].items():
        if name == 'black':
            continue
        nm, values = np.array(curve).T
        color = {'neutral':'#767676', 'orange':'#cb7930', 'green':'#58a14a',
            'amber':'#c9a12c', 'cyan':'#3d9ca2', 'blue':'#4666bb', 'violet':'#995bc0'}[name]
        axes[1].plot(nm, values, label=name, color=color)
    axes[1].set(xlim=(380, 780), ylim=(0, 1), xlabel='Vacuum wavelength (nm)', ylabel='Reflectance',
        title='Synthetic passive reflectance curves — elastic reflection, no wavelength conversion')
    axes[1].legend(ncol=7, fontsize=8)
    fig.savefig(root/'spectra-and-reflectance.png', dpi=160)
    fig.savefig(root/'spectra-and-reflectance.pdf')
    plt.close(fig)
    broad, peaks = (plan['source_curves'][n] for n in ('white_broad', 'white_three_peak'))
    fig, axes = plt.subplots(1, 2, figsize=(12, 4), constrained_layout=True)
    for curve, label in ((broad, 'Broad white'), (peaks, 'Three-peak white')):
        nm, values = np.array(curve).T
        axes[0].plot(nm, values, label=label)
    axes[0].set(xlim=(380, 780), xlabel='Vacuum wavelength (nm)', ylabel='Spectral radiance (W / m² sr nm)',
        title='Same incident XYZ; different spectral composition')
    axes[0].legend()
    names = [n for n in plan['reflectances'] if n != 'black']
    ratios = [fixture.xyz(peaks, plan['reflectances'][n])[1]/
              fixture.xyz(broad, plan['reflectances'][n])[1] for n in names]
    axes[1].bar(names, ratios, color=['#767676', '#cb7930', '#58a14a', '#c9a12c', '#3d9ca2', '#4666bb', '#995bc0'])
    axes[1].axhline(1, color='black', linewidth=.7)
    axes[1].tick_params(axis='x', rotation=30)
    axes[1].set(ylabel='Three-peak / broad returned Y', title='Independent source × reflectance product integral')
    fig.savefig(root/'metamer-response.png', dpi=160)
    fig.savefig(root/'metamer-response.pdf')
    plt.close(fig)


def shadow_plot(root):
    rows = json.loads((root/'shadow-probe-results.json').read_text())['results']
    lookup = {(r['source'],r['wall']):r for r in rows}
    names = ('black', 'neutral', 'orange', 'green', 'amber')
    colors = ('#333333', '#888888', '#cb7930', '#58a14a', '#c9a12c')
    fig, axes = plt.subplots(1, 3, figsize=(14, 4), constrained_layout=True)
    means = [lookup['white_broad',n]['mean_xyz'][1] for n in names]
    error = [lookup['white_broad',n]['seed_mean_standard_error'][1] for n in names]
    axes[0].bar(names, means, yerr=error, color=colors, capsize=3)
    axes[0].tick_params(axis='x', rotation=30)
    axes[0].set(ylabel='Unexposed Y (mean ± seed SE)', title='Directly shadowed receiver: direct = 0')
    black = np.array(lookup['white_broad','black']['mean_xyz'])
    for name, color in zip(names[1:], colors[1:]):
        increment = np.array(lookup['white_broad',name]['mean_xyz'])-black
        xy = increment[:2]/sum(increment)
        axes[1].scatter(*xy, color=color, s=65)
        axes[1].annotate(name, xy, xytext=(5,5), textcoords='offset points')
    axes[1].set(xlim=(.2,.65), ylim=(.25,.68), xlabel='x = X/(X+Y+Z)', ylabel='y = Y/(X+Y+Z)',
        title='Color of added indirect light\n(linear black-wall subtraction)')
    positions = np.arange(2)
    for shift, source, color in ((-.18,'white_broad','#888888'),(.18,'white_three_peak','#b38d42')):
        values = [lookup[source,n]['mean_xyz'][1] for n in ('neutral','amber')]
        errors = [lookup[source,n]['seed_mean_standard_error'][1] for n in ('neutral','amber')]
        axes[2].bar(positions+shift, values, .36, yerr=errors, capsize=3, color=color, label=LABELS[source])
    axes[2].set(xticks=positions, xticklabels=['neutral wall','amber wall'], ylabel='Unexposed shadow Y',
        title='Incident white XYZ matched')
    axes[2].legend(fontsize=8)
    fig.savefig(root/'shadow-transfer.png', dpi=160)
    fig.savefig(root/'shadow-transfer.pdf')
    plt.close(fig)


def metamer_detail(root):
    regions = patch_regions(root, 'room-white_broad')
    x0 = min(v[0] for v in regions.values())-2
    x1 = max(v[2] for v in regions.values())+3
    y0 = min(v[1] for v in regions.values())-2
    y1 = max(v[3] for v in regions.values())+3
    scale, margin = 4, 180
    width, height = (x1-x0)*scale, (y1-y0)*scale
    image = Image.new('RGB', (margin+width+20, 64+2*(height+32)), '#141820')
    draw = ImageDraw.Draw(image)
    font_path = Path('/System/Library/Fonts/Supplemental/Arial.ttf')
    font = ImageFont.truetype(str(font_path), 15) if font_path.exists() else ImageFont.load_default()
    draw.text((16,12), 'Same-white source pair: native patch pixels, 4× nearest-neighbor view', font=font, fill='white')
    for name, bounds in regions.items():
        x = margin+((bounds[0]+bounds[2])/2-x0)*scale
        draw.text((x,40),name,font=font,anchor='mm',fill='white')
    for i, case in enumerate(('room-white_broad','room-white_three_peak')):
        y = 64+i*(height+32)
        draw.text((16,y+height//2), LABELS[case[5:]], font=font, fill='white')
        source = Image.open(root/'final'/case/'image.png').convert('RGB')
        crop = source.crop((x0,y0,x1,y1)).resize((width,height),Image.Resampling.NEAREST)
        image.paste(crop,(margin,y))
    image.save(root/'metamer-material-detail.png')


def analyze(root):
    plan = json.loads((root/'plan.json').read_text())
    references = json.loads((root/'reference-results.json').read_text())
    verification = verify_cases(root, plan)
    spectral_plots(root, plan)
    shadow_plot(root)
    metamer_detail(root)
    rows = []
    for name, curve in plan['source_curves'].items():
        xyz = fixture.xyz(curve)
        rows.append(dict(source=name, X=xyz[0], Y=xyz[1], Z=xyz[2],
            integrated_radiance=fixture.power(curve),
            emitted_power_W=fixture.power(curve)*.65**2*math.pi))
    with (root/'source-inputs.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    seed_results = []
    patch_results = []
    for name in ('white_broad', 'white_three_peak', 'green_broad'):
        a = bmp_rgba(root/'final'/('room-'+name)/'output/frames/frame_0000.bmp')
        b = bmp_rgba(root/'final'/('seed-'+name)/'output/frames/frame_0000.bmp')
        pa = np.array(list(a[2]), dtype=float).reshape(a[1], a[0], 4)[:, :, :3]
        pb = np.array(list(b[2]), dtype=float).reshape(b[1], b[0], 4)[:, :, :3]
        # Predeclared full native film; no title or source overlay is included.
        diff = pa-pb
        seed_results.append(dict(source=name, display_RMS_255=float(np.sqrt(np.mean(diff*diff))),
            maximum_channel_difference=float(np.max(np.abs(diff))),
            mean_absolute_difference=float(np.mean(np.abs(diff)))))
        for material_name, (x0,y0,x1,y1) in patch_regions(root, 'room-'+name).items():
            a_roi, b_roi = pa[y0:y1+1,x0:x1+1], pb[y0:y1+1,x0:x1+1]
            patch_results.append(dict(source=name, material=material_name,
                bounds_xy_inclusive=[x0,y0,x1,y1], mean_rgb_255=a_roi.mean(axis=(0,1)).tolist(),
                second_seed_mean_rgb_255=b_roi.mean(axis=(0,1)).tolist(),
                seed_RMS_255=float(np.sqrt(np.mean((a_roi-b_roi)**2))),
                measure='display_RGB_only_not_linear_XYZ'))
    components = []
    for folder in sorted((root/'final').iterdir()):
        if not (folder/'summary.json').is_file():
            continue
        s = json.loads((folder/'summary.json').read_text())
        p = s['spectral_surface_path']['probes'][0]
        r = s['spectral_transport']
        components.append(dict(case=folder.name, total_xyz=p['total_xyz'], direct_xyz=p['direct_xyz'],
            indirect_xyz=p['surface_indirect_xyz'], volume_xyz=p['volume_xyz'],
            sample_standard_error=p['luminance_standard_error'],
            indirect_Y_fraction=p['surface_indirect_xyz'][1]/p['total_xyz'][1] if p['total_xyz'][1] else 0,
            film_components_xyz=r['film_components_xyz'], camera=r['film']))
    result = dict(seed_comparisons=seed_results, seed_RMS_limit=plan['seed_display_RMS_limit_255'],
        seed_gate_passed=all(x['display_RMS_255'] <= plan['seed_display_RMS_limit_255'] for x in seed_results),
        components=components, reference_max_relative_L2=max(x['relative_L2_error'] for x in references['results']),
        matched_source_inputs=rows, patch_display_measurements=patch_results,
        shadow_point_controls=json.loads((root/'shadow-probe-results.json').read_text()),
        metamer_precision=json.loads((root/'precision-results.json').read_text()), verification=verification)
    (root/'measurements.json').write_text(json.dumps(result, indent=2)+'\n')
    sheet(root, [('room-'+n, LABELS[n]) for n in ORDER], 'source-comparison.png')
    sheet(root, [('indirect-'+n, LABELS[n]+' — indirect only') for n in
        ('white_broad', 'white_three_peak', 'green_broad', 'blue_broad')], 'indirect-comparison.png')
    sheet(root, [(n, label) for n, label in (
        ('shadow-direct', 'Direct only — orange wall'), ('shadow-black', 'Black wall control'),
        ('shadow-neutral', 'Neutral wall'), ('shadow-orange', 'Orange wall'),
        ('shadow-green', 'Green wall'), ('shadow-amber', 'Selective amber wall'))],
        'shadow-controls.png', columns=3)
    sheet(root, [('shadow-neutral', 'Broad white / neutral wall'),
        ('shadow-metamer-neutral', 'Three-peak white / neutral wall'),
        ('shadow-amber', 'Broad white / amber wall'),
        ('shadow-metamer-amber', 'Three-peak white / amber wall')],
        'metamer-shadow-comparison.png', columns=2,
        subtitle='Incident lamp XYZ matched; fixed camera/exposure; no image normalization')
    sections = [('source-comparison.png', 'Matched source spectra'),
        ('indirect-comparison.png', 'The surface-indirect contribution, same exposure'),
        ('shadow-controls.png', 'Colored reflectors and direct-shadow controls'),
        ('metamer-shadow-comparison.png', 'Same-white source pair with selective reflectors'),
        ('metamer-material-detail.png', 'Native selective-patch detail under the same-white source pair'),
        ('shadow-transfer.png', 'Measured indirect light at the directly shadowed target'),
        ('spectra-and-reflectance.png', 'Declared source shapes and reflectance curves'),
        ('metamer-response.png', 'Independent spectral response prediction')]
    page = ['<!doctype html><meta charset="utf-8"><title>optiC visible spectral response</title>',
        '<style>body{background:#141820;color:#e9edf4;font:16px system-ui;max-width:1400px;margin:32px auto;padding:0 20px}img{max-width:100%;height:auto}a{color:#9bc5ff}p{line-height:1.6}</style>',
        '<h1>Visible spectra through ordinary V3</h1>',
        '<p>Saved scene optical profiles; identical geometry, camera and exposure3. '
        'Sources have equal incident CIE Y, not equal radiant power. No white balance, denoising or reconstruction change. '
        'Synthetic controlled spectra and reflectances; no UV below400nm or fluorescence.</p>',
        '<p>Back-wall swatches, image left to right: neutral, orange, amber, green, cyan, blue, violet. '
        'Saturated violet/blue are limited by the RGB display gamut. Fixed budgets do not certify convergence.</p>']
    for image, title in sections:
        page += ['<h2>'+html.escape(title)+'</h2>', f'<a href="{image}"><img src="{image}" alt="{html.escape(title)}"></a>']
    page += ['<h2>Individual room frames</h2><ul>']
    for name in ORDER:
        base = 'final/room-'+name
        page += [f'<li><a href="{base}/image.png">{html.escape(LABELS[name])}</a> · '
            f'<a href="{base}/scene.json">saved scene</a> · <a href="{base}/summary.json">summary</a></li>']
    page += ['</ul>']
    page += ['<h2>Raw evidence</h2><p><a href="measurements.json">Measurements</a> · '
        '<a href="reference-results.json">Independent transfer reference</a> · '
        '<a href="source-inputs.csv">Source normalization and radiometric power</a></p>',
        '<p>Each final case retains its saved scene, ordinary request, native BMP, lossless PNG, '
        'summary, receipt and hashes. Seed-gate result: '+str(result['seed_gate_passed'])+'.</p>']
    (root/'index.html').write_text('\n'.join(page))
    print(json.dumps(dict(seed_comparisons=seed_results, seed_gate_passed=result['seed_gate_passed'])))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', required=True, type=Path)
    args = parser.parse_args()
    analyze(args.root.resolve())
