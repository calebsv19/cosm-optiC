"""Independent water-only sequence receipt oracle; no renderer or simulation."""
from fractions import Fraction
import hashlib
import json
from pathlib import Path

def _read(p):return json.loads(p.read_text())
def _sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def _path(parent,relative,root):
    if not isinstance(relative,str) or Path(relative).is_absolute():raise ValueError('water path must be project relative')
    p=parent/relative
    if not p.resolve().is_relative_to(root):raise ValueError('water path escapes project')
    if any(x.is_symlink() for x in [p,*p.parents] if x.is_relative_to(root)):raise ValueError('water path is a symlink')
    if not p.is_file():raise ValueError('water source is missing')
    return p

def _select(manifest,requested,missing):
    frames=manifest.get('frames',[]);ids=[f['frame_index'] for f in frames]
    if not ids or any(type(x) is not int or x<0 for x in ids) or ids!=sorted(set(ids)):raise ValueError('water frame IDs must be sorted and unique')
    if missing=='error' and requested not in ids:raise ValueError('water frame outside coverage')
    if missing not in ('hold','error'):raise ValueError('unsupported water missing policy')
    selected=max((i for i in ids if i<=requested),default=ids[0])
    return next(f for f in frames if f['frame_index']==selected)

def expected_cache(scene,scene_path,source_tick):
    author=scene['extensions']['ray_tracing']['authoring'];attachment=author['atmosphere_attachment'];playback=attachment['playback'];seq=author['sequence']
    if playback.get('clock')!='sequence_source' or attachment['source_kind']!='scene_bundle':raise ValueError('not a sequence-source water bundle')
    if attachment.get('visible',True) or attachment.get('affects_lighting',True) or attachment.get('appearance',{}).get('density_scale',1)!=0 or attachment.get('appearance',{}).get('opacity',1)!=0:raise ValueError('sequence water volume must be hidden and zero')
    root=Path(scene_path).resolve().parent;bundle_path=_path(root,attachment['source_path'],root);bundle=_read(bundle_path)
    rate=Fraction(playback.get('rate_numerator',24),playback.get('rate_denominator',1));source_rate=Fraction(seq['source']['rate']['numerator'],seq['source']['rate']['denominator']);requested=Fraction(playback.get('source_start',0))
    if playback.get('animated',False):requested+=(source_tick-playback.get('timeline_start',0))*rate/source_rate
    requested=requested.numerator//requested.denominator;policy=playback.get('missing','hold')
    water=_path(bundle_path.parent,bundle['water_source']['path'],root);water_frame=_select(_read(water),requested,policy);water_file=_path(water.parent,water_frame['path'],root)
    volume=_path(bundle_path.parent,bundle['fluid_source']['path'],root);volume_frame=_select(_read(volume),requested,policy);volume_file=_path(volume.parent,volume_frame['path'],root)
    combined=hashlib.sha256((_sha(volume_file)+_sha(water_file)).encode()).hexdigest()
    return dict(id=attachment['id'],tick=water_frame['frame_index'],content_digest=combined,clock='sequence_source',rate=[rate.numerator,rate.denominator])
