"""Focused tests for the offline texture packer; run with python3 -m unittest."""

import struct
import unittest

from PIL import Image

from png_to_ozt import encode


class OztTests(unittest.TestCase):
    def test_bottom_up_bgra_preserves_straight_alpha(self):
        original = Image.new("RGBA", (2, 2))
        original.putdata([(255, 0, 0, 128), (0, 255, 0, 0), (0, 0, 255, 255), (10, 20, 30, 64)])
        payload = encode(original)
        self.assertEqual(struct.unpack_from("<HHBB", payload, 16), (2, 2, 32, 8))
        self.assertEqual(payload[22:26], bytes([255, 0, 0, 255]))
        decoded = Image.frombytes("RGBA", (2, 2), payload[22:], "raw", "BGRA", 0, -1)
        self.assertEqual(decoded.tobytes(), original.tobytes())
        self.assertEqual(len(payload), 22 + 2 * 2 * 4)

    def test_rejects_dimensions_outside_loader_contract(self):
        for size in [(0, 1), (1, 0), (1025, 1), (1, 1025)]:
            with self.subTest(size=size), self.assertRaises(ValueError):
                encode(Image.new("RGBA", size))


if __name__ == "__main__":
    unittest.main()
