"""Reject oversized metatile resources before they overwrite collisions or WRAM."""

import unittest

from adversarial_support import ROM_PATH, RuntimeSupport


@unittest.skipUnless(ROM_PATH, "Set SMBMINI_ROM to exercise the compiled map loader")
class MapCapacityTests(RuntimeSupport, unittest.TestCase):
    def load_synthetic_map(self, height):
        p = self.boot()
        p.tick(600)
        p.button("start")
        p.tick(300)
        self.variable(p, "SELECTEDWORLD", 16)
        self.variable(p, "SELECTEDLEVEL", 1)
        phase = {"seeded": False, "finished": False}
        record = {}

        def seed(_):
            if phase["seeded"]:
                return
            phase["seeded"] = True
            record["before_map"] = bytes(p.memory[0xA000:0xBF00])
            record["before_collisions"] = bytes(p.memory[0xBF00:0xC000])
            record["before_wram"] = bytes(p.memory[0xC000:0xC080])
            # Model a resized level without patching the loader or ROM on disk.
            p.memory[self.address("_image_tile_width")] = 208
            p.memory[self.address("_image_tile_height")] = height
            p.memory[self.address("_image_bank")] = 127
            self.putword(p, self.address("_image_ptr"), 0x4000)
            p.memory[127, 0x4000:0x6100] = [0xAA] * 0x2100

        def finish(kind):
            def capture(_):
                if not phase["seeded"] or phase["finished"]:
                    return
                phase["finished"] = True
                record["result"] = kind
                record["after_map"] = bytes(p.memory[0xA000:0xBF00])
                record["after_collisions"] = bytes(p.memory[0xBF00:0xC000])
                record["after_wram"] = bytes(p.memory[0xC000:0xC080])
            return capture

        p.hook_register(*self.symbols["_load_meta_tiles"], seed, None)
        p.hook_register(*self.symbols["_metatile_reset"], finish("rejected"), None)
        p.hook_register(*self.symbols["_scroll_reset"], finish("loaded"), None)
        p.button("start")
        p.tick(300)
        self.assertTrue(phase["seeded"], "World 16-1 did not call the actual loader")
        self.assertTrue(phase["finished"], "The loader did not finish")
        return record

    def test_valid_map_near_capacity_still_loads(self):
        result = self.load_synthetic_map(38)
        self.assertEqual(result["result"], "loaded")
        self.assertEqual(result["after_map"][:208 * 38], bytes([0xAA]) * (208 * 38))
        self.assertNotEqual(result["after_collisions"][:176], bytes([0xAA]) * 176)
        self.assertEqual(result["after_wram"], result["before_wram"])

    def test_oversized_maps_leave_map_collisions_and_wram_untouched(self):
        for height in (39, 40):
            with self.subTest(height=height):
                result = self.load_synthetic_map(height)
                self.assertEqual(result["result"], "rejected")
                for region in ("map", "collisions", "wram"):
                    self.assertEqual(result["after_" + region], result["before_" + region],
                                     f"Invalid map changed {region} before returning")


if __name__ == "__main__":
    unittest.main()
