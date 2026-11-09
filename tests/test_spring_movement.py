"""Spring launches must retain their authored movement speed after migration."""

import unittest

from adversarial_support import ROM_PATH, RuntimeSupport


@unittest.skipUnless(ROM_PATH, "Set SMBMINI_ROM to enable compiled-ROM tests")
class SpringMovementTests(RuntimeSupport, unittest.TestCase):
    def test_world_2_1_spring_keeps_six_pixel_boost(self):
        p = self.select_level(2, 1)
        player = self.address("_actors")
        # Prepare attachment to the real spring before compression. The old
        # full-level route depends on enemy-stomp timing, which this same
        # restored high-speed movement intentionally changes.
        self.prepare_spring(p)
        boosted_updates = 0
        p.button_press("a")
        for _ in range(40):
            before_y = self.word(p, player + 3)
            before_state = p.memory[self.address("_que_state")]
            p.tick(1)
            state = p.memory[self.address("_que_state")]
            velocity = self.word(p, self.address("_pl_vel_y"))
            if velocity >= 32768:
                velocity -= 65536
            if before_state == state == 13 and velocity < -16384:
                movement = (self.word(p, player + 3) - before_y + 32768) % 65536 - 32768
                self.assertEqual(movement, velocity >> 7,
                                 "Spring speed was clipped by a legacy subpixel limit")
                boosted_updates += 1
            self.assertEqual(self.variable(p, "LIVES"), 3)
        self.assertGreater(boosted_updates, 8, "Spring boost was not exercised")
