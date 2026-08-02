from __future__ import annotations

from copy import deepcopy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

import sys

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import compare_render_artifacts as comparison


def write_bmp(path: Path, pixels: list[tuple[int, int, int]]) -> None:
    width = len(pixels)
    height = 1
    row_stride = ((width * 3 + 3) // 4) * 4
    row = bytearray(row_stride)
    for index, (red, green, blue) in enumerate(pixels):
        row[index * 3:index * 3 + 3] = bytes((blue, green, red))
    header = bytearray(b"BM")
    header.extend(struct.pack("<IHHI", 54 + row_stride, 0, 0, 54))
    header.extend(
        struct.pack(
            "<IiiHHIIiiII",
            40,
            width,
            height,
            1,
            24,
            0,
            row_stride,
            2835,
            2835,
            0,
            0,
        )
    )
    path.write_bytes(bytes(header) + bytes(row))


def write_bgra_bitfields_bmp(
    path: Path, pixels: list[tuple[int, int, int]]
) -> None:
    width = len(pixels)
    pixel_offset = 122
    row = bytearray(width * 4)
    for index, (red, green, blue) in enumerate(pixels):
        row[index * 4:index * 4 + 4] = bytes((blue, green, red, 255))
    header = bytearray(pixel_offset)
    header[:2] = b"BM"
    struct.pack_into("<IHHI", header, 2, pixel_offset + len(row), 0, 0, pixel_offset)
    struct.pack_into(
        "<IiiHHIIiiII",
        header,
        14,
        108,
        width,
        1,
        1,
        32,
        3,
        len(row),
        2835,
        2835,
        0,
        0,
    )
    struct.pack_into("<III", header, 54, 0x00FF0000, 0x0000FF00, 0x000000FF)
    path.write_bytes(bytes(header) + bytes(row))


def digest(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def summary() -> dict:
    return {
        "schema_version": comparison.SUMMARY_SCHEMA,
        "route_family": "native_3d",
        "route_native_3d": True,
        "rendered_frames": True,
        "frames_rendered": 1,
        "volume_source_kind": "none",
        "volume_visible": True,
        "integrator_3d": "direct_light",
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": 2,
            "height": 1,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "denoise_enabled": False,
        },
        "inspection": {
            "preset": "none",
            "trace_route": "tlas_blas",
            "caustic_mode": "off",
            "caustic_product_mode": "reference",
        },
    }


class RenderArtifactComparisonTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.source = self.root / "source"
        self.target = self.root / "target"
        self.source.mkdir()
        self.target.mkdir()
        for root in (self.source, self.target):
            (root / "summary.json").write_text(
                json.dumps(summary(), sort_keys=True) + "\n",
                encoding="utf-8",
            )
            write_bmp(root / "frame.bmp", [(10, 20, 30), (40, 50, 60)])
        self.request_path = self.root / "request.json"
        self.request = self._request()
        self._write_request()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _side(self, root: Path, version: str) -> dict:
        return {
            "lifecycle_id": f"lifecycle.ray.{version}",
            "lifecycle_result_digest": "sha256:" + version[-1] * 64,
            "program_release_version": version,
            "worker_package_version": f"0.{version}",
            "artifact_root": str(root),
            "summary_path": "summary.json",
            "summary_digest": digest(root / "summary.json"),
            "frame_path": "frame.bmp",
            "frame_digest": digest(root / "frame.bmp"),
        }

    def _request(self) -> dict:
        return {
            "schema_family": comparison.REQUEST_FAMILY,
            "schema_variant": comparison.REQUEST_VARIANT,
            "comparison_id": "comparison.ray.10.2-to-10.3.001",
            "pair_id": "pair.ray.10.2-and-10.3.001",
            "pair_result_digest": "sha256:" + "a" * 64,
            "source": self._side(self.source, "10.2"),
            "target": self._side(self.target, "10.3"),
            "policy": {
                "max_mean_absolute_channel_delta_ppm": 0,
                "max_changed_pixel_ratio_ppm": 0,
                "max_absolute_channel_delta": 0,
            },
            "operations": {
                "semantic_compare": True,
                "visual_acceptance": False,
                "promotion": False,
                "current_pointer_read": False,
                "current_pointer_mutation": False,
                "network_contact": False,
                "remote_contact": False,
                "vps_contact": False,
                "custom_os_boot": False,
            },
        }

    def _write_request(self) -> None:
        self.request_path.write_text(
            json.dumps(self.request, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    def test_exact_pair_is_semantic_match(self) -> None:
        result = comparison.compare(self.request_path)
        self.assertEqual(result["state"], "semantic_match")
        self.assertTrue(
            result["summary_comparison"]["compatible"]
        )
        self.assertTrue(result["image_comparison"]["exact_pixel_match"])
        self.assertEqual(
            result["image_comparison"][
                "mean_absolute_channel_delta_ppm"
            ],
            0,
        )
        self.assertFalse(result["visual_acceptance_performed"])
        self.assertFalse(result["promotion_recommended"])

    def test_bounded_pixel_change_has_deterministic_disposition(self) -> None:
        write_bmp(
            self.target / "frame.bmp",
            [(11, 20, 30), (40, 50, 60)],
        )
        self.request["target"]["frame_digest"] = digest(
            self.target / "frame.bmp"
        )
        self._write_request()
        strict = comparison.compare(self.request_path)
        self.assertEqual(strict["state"], "semantic_difference")
        self.assertEqual(
            strict["image_comparison"]["changed_pixel_count"], 1
        )
        self.request["policy"].update(
            {
                "max_mean_absolute_channel_delta_ppm": 654,
                "max_changed_pixel_ratio_ppm": 500_000,
                "max_absolute_channel_delta": 1,
            }
        )
        self._write_request()
        bounded = comparison.compare(self.request_path)
        self.assertEqual(bounded["state"], "semantic_match")

    def test_summary_semantic_drift_is_incompatible(self) -> None:
        target_summary = summary()
        target_summary["integrator_3d"] = "disney"
        (self.target / "summary.json").write_text(
            json.dumps(target_summary, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        self.request["target"]["summary_digest"] = digest(
            self.target / "summary.json"
        )
        self._write_request()
        result = comparison.compare(self.request_path)
        self.assertEqual(result["state"], "incompatible_artifacts")
        self.assertEqual(
            result["summary_comparison"]["differences"][0]["field"],
            "integrator_3d",
        )

    def test_digest_drift_fails_before_comparison(self) -> None:
        self.request["source"]["frame_digest"] = "sha256:" + "f" * 64
        self._write_request()
        with self.assertRaisesRegex(
            comparison.ComparisonError, "frame digest drifted"
        ):
            comparison.compare(self.request_path)

    def test_create_only_output_is_idempotent_and_rejects_drift(self) -> None:
        payload = comparison.canonical_bytes(
            comparison.compare(self.request_path)
        )
        output = self.root / "result.json"
        comparison.write_create_only(output, payload)
        comparison.write_create_only(output, payload)
        output.write_bytes(b"drift")
        with self.assertRaisesRegex(
            comparison.ComparisonError, "exists with drift"
        ):
            comparison.write_create_only(output, payload)

    def test_cli_writes_and_replays_canonical_result(self) -> None:
        output = self.root / "cli-result.json"
        command = [
            sys.executable,
            str(REPOSITORY_ROOT / "tools" / "compare_render_artifacts.py"),
            "--request",
            str(self.request_path),
            "--output",
            str(output),
        ]
        first = subprocess.run(command, check=True, capture_output=True)
        second = subprocess.run(command, check=True, capture_output=True)
        self.assertEqual(first.stdout, second.stdout)
        document = comparison.parse_json(output)
        self.assertEqual(document["state"], "semantic_match")
        self.assertEqual(
            output.read_bytes(), comparison.canonical_bytes(document)
        )

    def test_bgra_bitfields_matches_native_output_layout(self) -> None:
        frame = self.root / "bitfields.bmp"
        pixels = [(1, 2, 3), (250, 251, 252)]
        write_bgra_bitfields_bmp(frame, pixels)
        width, height, observed = comparison.read_bmp(frame)
        self.assertEqual((width, height), (2, 1))
        self.assertEqual(observed, pixels)

    def test_unknown_authority_field_fails_closed(self) -> None:
        drifted = deepcopy(self.request)
        drifted["operations"]["remote_contact"] = True
        self.request = drifted
        self._write_request()
        with self.assertRaisesRegex(
            comparison.ComparisonError, "operations drifted"
        ):
            comparison.compare(self.request_path)


if __name__ == "__main__":
    unittest.main()
