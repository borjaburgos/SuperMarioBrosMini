"""Elevator wrap checks must distinguish tall-map coordinates from negative Y."""

import unittest

from adversarial_support import ACTIVE, ACTOR_SIZE, ROM_PATH, RuntimeSupport


@unittest.skipUnless(ROM_PATH, "Set SMBMINI_ROM to enable compiled-ROM tests")
class TallElevatorTests(RuntimeSupport, unittest.TestCase):
    def _step_elevator(self, y, velocity, own_rider=True):
        p = self.select_level(12, 3)
        self.assertEqual(p.memory[self.address("_image_tile_height")], 192)
        actor = self.address("_actors") + ACTOR_SIZE
        # Run the real elevator behavior in this real tall map. Freeze player
        # movement so only this platform's wrap is allowed to change attachment.
        p.memory[actor] = ACTIVE | 0x10
        self.putword(p, actor + 1, 80 * 32)
        self.putword(p, actor + 3, int(y * 32))
        p.memory[self.address("_actor_behavior_ids") + 1] = 16
        p.memory[self.address("_actor_states") + 1] = 1
        self.putword(p, self.address("_actor_vel_y") + 2, velocity)
        p.memory[self.address("_que_state")] = 19
        p.memory[self.address("_actor_attached")] = 1
        self.putword(p, self.address("_last_actor"),
                     actor if own_rider else actor + ACTOR_SIZE)
        start = self.word(p, actor + 3)
        for _ in range(12):
            p.tick(1)
            if self.word(p, actor + 3) != start:
                break
        self.assertNotEqual(self.word(p, actor + 3), start,
                            "Elevator behavior did not run")
        return self.word(p, actor + 3), p.memory[self.address("_actor_attached")]

    def test_upward_elevators_cross_1024_pixels_without_wrapping_or_detaching(self):
        for y in (1000, 1024.25, 1100, 1400):
            with self.subTest(y=y):
                position, attached = self._step_elevator(y, -8)
                self.assertEqual(position, int((y - 0.5) * 32))
                self.assertEqual(attached, 1)

    def test_real_top_and_bottom_wrap_detach_only_the_wrapping_platforms_rider(self):
        # Retain the authored bottom wrap at the first tile beyond the bottom.
        for y, velocity, expected in ((0.25, -8, 1535.75), (1543.75, 8, 8.25)):
            for own_rider in (False, True):
                with self.subTest(y=y, own_rider=own_rider):
                    position, attached = self._step_elevator(y, velocity, own_rider)
                    self.assertEqual(position, int(expected * 32))
                    self.assertEqual(attached, 0 if own_rider else 1)

    def test_downward_elevator_beyond_1024_pixels_still_carries_its_rider(self):
        position, attached = self._step_elevator(1400, 8)
        self.assertEqual(position, 1400 * 32 + 16)
        self.assertEqual(attached, 1)
