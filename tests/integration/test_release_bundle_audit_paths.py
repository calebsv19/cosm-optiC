#!/usr/bin/env python3
"""Exercise the real bundle audit recipe with a contained package fixture."""
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(sys.platform == 'darwin', 'macOS bundle audit')
class BundleAuditPaths(unittest.TestCase):
    def test_audit_and_private_temporary_runtime_stay_contained(self):
        with tempfile.TemporaryDirectory(prefix='ray-bundle-audit-') as raw:
            root = Path(raw).resolve()
            contents = root / 'app/Contents'
            macos = contents / 'MacOS'
            frameworks = contents / 'Frameworks'
            macos.mkdir(parents=True)
            frameworks.mkdir()
            (root / 'home').mkdir()
            (root / 'tmp').mkdir()
            with (contents / 'Info.plist').open('wb') as stream:
                plistlib.dump({'CFBundleIdentifier': 'com.cosm.optic',
                              'CFBundleShortVersionString': '0.19.0',
                              'CFBundleVersion': '0.19.0'}, stream)
            shutil.copyfile('/usr/bin/true', macos / 'raytracing-bin')
            (macos / 'raytracing-bin').chmod(0o755)
            launcher = macos / 'raytracing-launcher'
            launcher.write_text('''#!/bin/sh
set -eu
printf 'RAY_TRACING_RUNTIME_DIR=%s/runtime\\n' "$RAY_TRACING_APP_SUPPORT_DIR"
printf 'RAY_TRACING_RENDER_METRICS_DATASET_PATH=%s/runtime/metrics.json\\n' "$RAY_TRACING_APP_SUPPORT_DIR"
printf 'RAY_TRACING_FFMPEG_BIN=/usr/bin/true\\nVK_ICD_FILENAMES=fixture\\nVK_DRIVER_FILES=fixture\\n'
''')
            launcher.chmod(0o755)
            makefile = root / 'audit.mk'
            makefile.write_text(f'''RELEASE_ROOT := {root}/release
BUILD_DIR_BASE := {root}/build
PACKAGE_CONTENTS_DIR := {contents}
PACKAGE_MACOS_DIR := {macos}
PACKAGE_FRAMEWORKS_DIR := {frameworks}
RELEASE_BUNDLE_ID := com.cosm.optic
RELEASE_VERSION := 0.19.0
include {ROOT}/make/release.mk
.PHONY: package-desktop-self-test
package-desktop-self-test:
\t@:
''')
            result = subprocess.run(['make', '-s', '-f', str(makefile),
                                     'release-bundle-audit'], text=True,
                                    capture_output=True, timeout=30,
                                    env={**os.environ, 'HOME': str(root / 'home'),
                                         'TMPDIR': str(root / 'tmp')})
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            audit = root / 'build/release-audit'
            self.assertTrue((audit / 'print_config.txt').is_file())
            self.assertTrue((audit / 'otool_raytracing_bin.txt').is_file())
            self.assertFalse((root / 'release.audit').exists())
            self.assertFalse((root / 'release').exists())
            self.assertEqual(list((root / 'tmp').glob('raytracing-release-audit.*')), [])


if __name__ == '__main__':
    unittest.main()
