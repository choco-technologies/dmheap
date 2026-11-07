"""Regression cases for the empty GOT shipped in memory 0.9 for Xtensa."""
import struct
import unittest

from check_relocations import check


def module():
    data = bytearray(512)
    struct.pack_into("<4I", data, 0, 0x444D4F44, 240, 0x100, 4)
    data[16:30] = b"xtensa-esp32s3\0"
    struct.pack_into("<Q", data, 224, 448)
    struct.pack_into("<II", data, 448 + 6 * 8, 400, 12)
    struct.pack_into("<3I", data, 400, 1, 1, 256)
    return data


class RelocationCheckTest(unittest.TestCase):
    def test_accepts_relocated_package(self):
        self.assertEqual(check(module()), 1)

    def test_rejects_legacy_empty_got(self):
        data = module()
        struct.pack_into("<I", data, 448 + 6 * 8 + 4, 0)
        with self.assertRaisesRegex(ValueError, "missing relocation table"):
            check(data)

    def test_rejects_missing_magic(self):
        data = module()
        struct.pack_into("<I", data, 400, 256)
        with self.assertRaisesRegex(ValueError, "missing nonempty relocation table"):
            check(data)

    def test_rejects_invalid_offset(self):
        data = module()
        struct.pack_into("<I", data, 408, 400)
        with self.assertRaisesRegex(ValueError, "invalid relocation offset"):
            check(data)

    def test_rejects_truncated_module(self):
        with self.assertRaisesRegex(ValueError, "truncated module footer"):
            check(module()[:448])


if __name__ == "__main__":
    unittest.main()
