#!/usr/bin/env python3
"""Run production generation retention with Linux PATH_MAX and an 8 MiB stack."""
import argparse
import os
from pathlib import Path
import re
import resource
import sys
import subprocess
import tempfile


def block(source, pattern):
    match = re.search(pattern, source)
    if not match:
        raise AssertionError(f"production block missing: {pattern}")
    start = source.index("{", match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if source[match.start():].startswith("typedef"):
        end = source.index(";", end) + 1
    return source[match.start():end]


def limits():
    # Darwin does not implement Linux address-space/nice enforcement here.
    # This local, allocation-bounded fixture is not native package qualification.
    if sys.platform.startswith("linux"):
        resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3,) * 2)
        os.nice(19)
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_FSIZE, (64 * 1024**2,) * 2)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("src/app/ray_tracing_temporal_checkpoint.c"))
    args = parser.parse_args()
    source = args.source.read_text()
    production = [block(source, r"typedef struct RayTracingCheckpointGenerationCandidate\s*\{")]
    production += [block(source, rf"static [^\n]*\b{name}\s*\(") for name in
                   ("regular_file", "compare_generation_descending", "scan_generations", "retain_two_generations")]
    call = ("retain_two_generations(&session, candidates, 1024);" if
            "candidate_capacity" in production[-1] else "retain_two_generations(&session);")
    prefix = """#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#undef PATH_MAX
#define PATH_MAX 4096
typedef struct { char root[PATH_MAX]; struct { int frameIndex; } identity; } RayTracingTemporalCheckpointSession;
static char test_root[PATH_MAX];
"""
    driver = """
static void* worker(void* unused) {
    (void)unused;
    RayTracingTemporalCheckpointSession session = {0};
    RayTracingCheckpointGenerationCandidate candidates[1024];
    snprintf(session.root, sizeof(session.root), "%s", test_root);
    if (scan_generations(session.root, 0, candidates, 1024) != 3) return (void*)1;
    CALL
    if (scan_generations(session.root, 0, candidates, 1024) != 2) return (void*)2;
    return NULL;
}
int main(int argc, char** argv) {
    pthread_attr_t attr; pthread_t thread; void* result = NULL;
    if (argc != 2) return 3;
    snprintf(test_root, sizeof(test_root), "%s", argv[1]);
    if (pthread_attr_init(&attr) || pthread_attr_setstacksize(&attr, 8*1024*1024) ||
        pthread_create(&thread, &attr, worker, NULL) || pthread_join(thread, &result)) return 4;
    return result ? 5 : 0;
}
""".replace("CALL", call)
    # The fixed Linux package container deliberately mounts /tmp noexec.
    # Keep invocation-owned executable fixtures in the build mount instead.
    fixture_parent = Path("build/checkpoint-stack-fixtures")
    fixture_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ray-checkpoint-stack-", dir=fixture_parent) as temporary:
        root = Path(temporary)
        frames = root / "frames/frame_0000"
        frames.mkdir(parents=True)
        for generation in range(1, 4):
            (frames / f"generation_{generation:020d}.rtck").write_bytes(b"retention fixture")
        code = root / "test.c"
        binary = root / "test"
        code.write_text(prefix + "\n".join(production) + driver)
        subprocess.run(["cc", "-O0", "-fno-inline", "-pthread", str(code), "-o", str(binary)], check=True)
        result = subprocess.run([str(binary), str(root / "frames")], preexec_fn=limits, timeout=10)
        if result.returncode:
            raise AssertionError(f"production retention failed on 8 MiB stack: exit {result.returncode}")
        assert [p.name for p in sorted(frames.iterdir())] == [
            f"generation_{generation:020d}.rtck" for generation in (2, 3)]
    print("production retention preserves newest two generations on Linux-sized 8 MiB stack")


if __name__ == "__main__":
    main()
