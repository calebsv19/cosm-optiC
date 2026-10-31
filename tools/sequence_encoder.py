"""Explicit delivery profiles and a real encode/probe/decode readiness check."""
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from sequence_temporal_review import verify_video

PROFILES = {'libx264': ('h264', ['-c:v', 'libx264', '-crf', '18']),
            'mpeg4': ('mpeg4', ['-c:v', 'mpeg4', '-q:v', '2'])}


def encoding_args(encoder):
    if encoder not in PROFILES:
        raise ValueError('Select an explicit supported encoder: libx264 or mpeg4')
    return list(PROFILES[encoder][1])


def preflight_encoder(ffmpeg='ffmpeg', ffprobe='ffprobe', encoder='libx264'):
    """No fallback. Fail before frame rendering; temporary probe is always retired."""
    args = encoding_args(encoder)
    executables = [shutil.which(str(p)) for p in (ffmpeg, ffprobe)]
    if not all(executables):
        raise ValueError('Encoder preflight requires executable ffmpeg and ffprobe')
    ffmpeg, ffprobe = map(str, executables)
    try:
        with tempfile.TemporaryDirectory(prefix='optic-encoder-preflight-') as root:
            video = Path(root) / 'probe.mp4'
            command = [ffmpeg, '-v', 'error', '-nostdin', '-n', '-f', 'rawvideo',
                       '-pixel_format', 'rgb24', '-video_size', '16x16',
                       '-framerate', '24', '-i', 'pipe:0', '-frames:v', '2',
                       *args, '-pix_fmt', 'yuv420p', '-video_track_timescale', '24',
                       '-movie_timescale', '24', str(video)]
            subprocess.run(command, input=bytes(16*16*3*2), capture_output=True,
                           check=True, timeout=20)
            stream = json.loads(subprocess.check_output(
                [ffprobe, '-v', 'error', '-select_streams', 'v:0', '-show_streams',
                 '-of', 'json', str(video)], timeout=10))['streams'][0]
            if (stream['codec_name'], stream['width'], stream['height']) != (PROFILES[encoder][0], 16, 16):
                raise ValueError('Encoder probe produced an unexpected codec or dimensions')
            verify_video(video, Fraction(24), 2, ffmpeg, ffprobe, timeout=10)
    except (subprocess.SubprocessError, OSError, ValueError, KeyError, IndexError) as exc:
        detail = getattr(exc, 'stderr', b'') or str(exc)
        if isinstance(detail, bytes): detail = detail.decode(errors='replace')
        raise ValueError(f'Encoder preflight failed for {encoder}: {detail[-1500:]}') from exc
    return dict(schema='ray_sequence_encoder_readiness_v1', ok=True,
                encoder=encoder, codec=PROFILES[encoder][0],
                ffmpeg=ffmpeg, ffprobe=ffprobe,
                ffmpeg_sha256=hashlib.sha256(Path(ffmpeg).read_bytes()).hexdigest(),
                ffprobe_sha256=hashlib.sha256(Path(ffprobe).read_bytes()).hexdigest(),
                frame_count=2, rate='24', duration='1/12',
                all_pts_exact=True, decoded=True, fallback=False)
