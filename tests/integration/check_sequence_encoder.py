"""Real encoder readiness and fail-before-render delivery admission."""
from pathlib import Path
import json
import argparse
import os
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from sequence_encoder import preflight_encoder, encoding_args
from sequence_multiview import deliver

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--require-encoder',action='append',choices=('libx264','mpeg4'),default=[])
a=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='optic-encoder-tests-') as root:
    root=Path(root)
    for encoder in sorted(set(['mpeg4',*a.require_encoder])):
        proof=preflight_encoder(encoder=encoder)
        assert proof['ok'] and proof['decoded'] and proof['all_pts_exact'] and not proof['fallback']
    missing=root/'ffmpeg-without-required-encoder'
    missing.write_text('#!/bin/sh\necho "Unknown encoder libx264" >&2\nexit 8\n');missing.chmod(0o700)
    scene=root/'scene.json';scene.write_text(json.dumps(dict(extensions=dict(ray_tracing=dict(authoring=dict(sequence={},camera_rig=dict(cameras=[dict(id='camera/A')])))))))
    request=root/'request.json';request.write_text(json.dumps(dict(scene=dict(runtime_scene_path=str(scene)))))
    output=root/'must-not-exist'
    cli=root/'must-not-run';marker=root/'renderer-was-invoked'
    cli.write_text('#!/bin/sh\ntouch "'+str(marker)+'"\n');cli.chmod(0o700)
    try:deliver(request,['camera/A'],output,cli,ffmpeg=str(missing))
    except ValueError as exc:assert 'Encoder preflight failed for libx264' in str(exc)
    else:raise AssertionError('missing encoder admitted')
    assert not output.exists() and not marker.exists()
    try:encoding_args('automatic')
    except ValueError:pass
    else:raise AssertionError('implicit codec fallback admitted')
    print('Encoder preflight: required real codecs, exact PTS/decode, missing encoder before output/render, no fallback PASS')
