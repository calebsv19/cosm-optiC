"""Reject nested helper defects locally before notarization."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('signatures', ROOT / 'tools/verify_release_signatures.py')
signatures = importlib.util.module_from_spec(spec)
spec.loader.exec_module(signatures)
IDENTITY = 'Developer ID Application: Fixture (TEAMID)'
DETAILS = f'Authority={IDENTITY}\nAuthority=Developer ID Certification Authority\nTimestamp=Oct 4, 2026\nCodeDirectory v=20500 size=42 flags=0x10000(runtime) hashes=2+7 location=embedded\n'


class SignatureRequirements(unittest.TestCase):
    def test_nested_executable_requires_all_three_properties(self):
        signatures.check_signature(DETAILS, identity=IDENTITY, executable=True)
        for original, replacement in [(f'Authority={IDENTITY}', 'Signature=adhoc'),
                                      ('Timestamp=Oct 4, 2026', ''),
                                      ('flags=0x10000(runtime)', 'flags=0x0(none)')]:
            with self.subTest(original=original), self.assertRaises(ValueError):
                signatures.check_signature(DETAILS.replace(original, replacement),
                                           identity=IDENTITY, executable=True)

    def test_library_requires_identity_timestamp_but_not_runtime(self):
        signatures.check_signature(DETAILS.replace('(runtime)', '(none)'),
                                   identity=IDENTITY, executable=False)
        with self.assertRaises(ValueError):
            signatures.check_signature(DETAILS, identity=IDENTITY + 'wrong', executable=False)


if __name__ == '__main__':
    unittest.main()
