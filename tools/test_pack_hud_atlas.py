"""Check actual matte removal, not only OZT header construction."""

import unittest

from PIL import Image, ImageDraw

from pack_hud_atlas import extract_cells, unmatte


class HudAtlasTests(unittest.TestCase):
    def test_recovers_edges_without_magenta_fringe(self):
        image = Image.new("RGB", (4, 1))
        image.putdata([(255, 0, 255), (128, 128, 128), (191, 64, 191), (0, 0, 0)])
        result = list(unmatte(image).getdata())
        self.assertEqual(result[0], (0, 0, 0, 0))
        self.assertEqual(result[1], (128, 128, 128, 255))
        self.assertEqual(result[2], (128, 128, 128, 128))
        self.assertEqual(result[3], (0, 0, 0, 255))

    def test_rejects_empty_or_wrong_shape_sheet(self):
        with self.assertRaises(ValueError):
            extract_cells(Image.new("RGB", (400, 400)))
        with self.assertRaises(ValueError):
            extract_cells(Image.new("RGB", (400, 300), (255, 0, 255)))

    def test_sheet_dividers_do_not_change_sprite_bounds(self):
        image = Image.new("RGB", (400, 300), (255, 0, 255))
        draw = ImageDraw.Draw(image)
        for row in range(3):
            for col in range(4):
                x, y = col * 100, row * 100
                draw.rectangle((x + 20, y + 20, x + 79, y + 79), fill=(90, 90, 90))
                draw.line((x, y, x, y + 99), fill=(90, 90, 90))
        cells = extract_cells(image)
        self.assertTrue(all(cell.size == (64, 64) for cell in cells))
        self.assertTrue(all(cell.getpixel((32, 32)) == (90, 90, 90, 255) for cell in cells))


if __name__ == "__main__":
    unittest.main()
