#!/usr/bin/env python3
import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert BMP frame dumps into an MP4 using ffmpeg."
    )
    parser.add_argument(
        "--frames",
        default="data/runtime/frames/default",
        help="Directory containing frame_XXXX.bmp files (default: data/runtime/frames/default)",
    )
    parser.add_argument(
        "--output",
        default="data/runtime/videos/output.mp4",
        help="Output MP4 path (default: data/runtime/videos/output.mp4)",
    )
    parser.add_argument(
        "--fps",
        type=int,
        default=30,
        help="Frames per second for the output video (default: 30)",
    )
    return parser.parse_args()


def ensure_sequential_frames(frame_dir: Path) -> int:
    frames = sorted(frame_dir.glob("frame_*.bmp"))
    if not frames:
        raise RuntimeError(f"No frames found in {frame_dir}")

    temp_names = []
    for idx, frame in enumerate(frames):
        tmp_name = frame_dir / f".tmp_frame_{idx:04d}.bmp"
        frame.rename(tmp_name)
        temp_names.append(tmp_name)

    for idx, tmp in enumerate(temp_names):
        final_name = frame_dir / f"frame_{idx:04d}.bmp"
        tmp.rename(final_name)

    return len(temp_names)


def run_ffmpeg(frame_dir: Path, output_path: Path, fps: int) -> None:
    cmd = [
        "ffmpeg",
        "-y",
        "-start_number",
        "0",
        "-framerate",
        str(fps),
        "-i",
        str(frame_dir / "frame_%04d.bmp"),
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        str(output_path),
    ]
    subprocess.check_call(cmd)


def main() -> int:
    args = parse_args()
    frame_dir = Path(args.frames).resolve()
    output_path = Path(args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    if shutil.which("ffmpeg") is None:
        print("ffmpeg is not installed or not on PATH. Please install ffmpeg to build the video.", file=sys.stderr)
        return 1

    if not frame_dir.exists():
        print(f"Frame directory does not exist: {frame_dir}", file=sys.stderr)
        return 1

    try:
        frame_count = ensure_sequential_frames(frame_dir)
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        return 1

    print(f"Renumbered {frame_count} frames under {frame_dir}")
    print(f"Creating {output_path.name} at {args.fps} fps via ffmpeg…")
    try:
        run_ffmpeg(frame_dir, output_path, args.fps)
    except subprocess.CalledProcessError as exc:
        print(f"ffmpeg failed with code {exc.returncode}", file=sys.stderr)
        return exc.returncode

    print(f"✅ Video ready: {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
