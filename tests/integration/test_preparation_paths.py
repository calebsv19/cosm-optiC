#!/usr/bin/env python3
"""Containment, cold refusal and exact fresh-tool selection for release gates."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('preparation_paths', ROOT / 'tools/preparation_paths.py')
PATHS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATHS)


class PreparationPaths(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ray-path-contract-')
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name).resolve() / 'source'
        self.repo.mkdir()
        self.build = self.repo / 'build/decision-one-example'

    def admit(self, **changes):
        args = dict(repository=self.repo, build=str(self.build),
                    generated=str(self.build / 'generated'), dist=str(self.build / 'dist'),
                    tests=str(self.build / 'agent_runs/ray_tracing'))
        args.update(changes)
        return PATHS.validate_paths(**args)

    def test_fresh_roots_are_read_only(self):
        roots = self.admit(release=str(self.repo / 'build/release-authenticated/example'))
        self.assertEqual(roots['build'], self.build)
        self.assertFalse(self.build.exists())

    def test_default_build_and_source_ancestors_refuse(self):
        for build in (self.repo / 'build', self.repo, self.repo.parent, self.repo / 'src/output'):
            with self.subTest(build=build), self.assertRaises(ValueError):
                self.admit(build=str(build))

    def test_overlap_escape_and_traversal_refuse(self):
        for change in ({'dist': str(self.build / 'generated')},
                       {'tests': str(self.repo / 'build/agent_runs')},
                       {'generated': str(self.build / 'toolchains/clang')},
                       {'build': str(self.build / '../other')},
                       {'release': str(self.build / 'archive')}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.admit(**change)

    def test_symlink_ancestor_refuses_without_touching_target(self):
        kept = self.repo / 'retained'; kept.mkdir()
        sentinel = kept / 'evidence'; sentinel.write_bytes(b'keep')
        (self.repo / 'alias').symlink_to(kept, target_is_directory=True)
        with self.assertRaises(ValueError):
            self.admit(build=str(self.repo / 'alias/new-build'))
        self.assertEqual(sentinel.read_bytes(), b'keep')

    def executable(self, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#!/bin/sh\nexit 0\n'); path.chmod(0o755)

    def test_exact_tool_no_stale_default_or_legacy_fallback(self):
        old = self.repo / 'build/toolchains/clang/arm64/tools/cli/renderer'
        legacy = self.build / 'arm64/tools/cli/renderer'
        self.executable(old); self.executable(legacy)
        with self.assertRaises(ValueError):
            PATHS.selected_tool(self.repo, str(self.build), 'renderer', 'arm64')
        fresh = self.build / 'toolchains/clang/arm64/tools/cli/renderer'
        self.executable(fresh)
        self.assertEqual(PATHS.selected_tool(self.repo, str(self.build), 'renderer', 'arm64'), fresh)
        fresh.unlink(); fresh.symlink_to(old)
        with self.assertRaises(ValueError):
            PATHS.selected_tool(self.repo, str(self.build), 'renderer', 'arm64')

    def test_fixture_escape_and_occupied_refuse(self):
        tests = str(self.build / 'agent_runs/ray_tracing')
        for label in ('../retained', '/absolute'):
            with self.assertRaises(ValueError):
                PATHS.fixture_root(self.repo, str(self.build), tests, label)
        work = PATHS.fixture_root(self.repo, str(self.build), tests, 'preflight')
        work.mkdir(parents=True); sentinel = work / 'evidence'; sentinel.write_bytes(b'keep')
        # The real shell reset owner refuses occupied contained fixtures.
        command = 'source "$1"; ray_tracing_test_reset_work_root preflight "$2"'
        (self.repo / 'tools').mkdir()
        (self.repo / 'tools/preparation_paths.py').write_bytes((ROOT / 'tools/preparation_paths.py').read_bytes())
        env = dict(os.environ, BUILD_DIR_BASE=str(self.build), RAY_TRACING_TEST_WORK_ROOT=tests)
        result = subprocess.run(['bash', '-c', command, 'bash', str(ROOT / 'tests/integration/lib/common.sh'), str(self.repo)], env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(sentinel.read_bytes(), b'keep')

    def test_make_cold_refuses_default_fixed_gate(self):
        result = subprocess.run(['make', '-s', '-C', str(ROOT), 'run-headless-smoke'], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('require an isolated', result.stderr)
        self.assertNotIn('clang ', result.stdout)


if __name__ == '__main__':
    unittest.main()
