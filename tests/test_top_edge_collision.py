"""Partial visibility at the top must preserve terrain collisions."""

import unittest

from adversarial_support import ACTOR_SIZE, ROM_PATH, RuntimeSupport


@unittest.skipUnless(ROM_PATH, "Set SMBMINI_ROM to enable compiled-ROM tests")
class TopEdgeCollisionTests(RuntimeSupport, unittest.TestCase):
    def _player(self, p, x, y, powered=True, state=1):
        actor = self.address("_actors")
        self.variable(p, "MARIOSTATUS_0", int(powered))
        self.putword(p, actor + 1, int(x * 32))
        self.putword(p, actor + 3, int(y * 32))
        for offset, value in [(6, 0), (8, 7 * 32 - 1),
                              (10, (-7 if powered else 1) * 32), (12, 8 * 32 - 1)]:
            self.putword(p, actor + offset, value)
        p.memory[self.address("_que_state")] = state
        p.memory[self.address("_hold_jump_val")] = 0
        p.memory[self.address("_actor_attached")] = 0
        self.putword(p, self.address("_pl_vel_x"), 0)
        self.putword(p, self.address("_pl_vel_y"), 512)
        return actor

    def test_small_and_big_mario_land_on_world_12_3_top_row_from_both_air_states(self):
        for powered in (False, True):
            for state in (1, 13):
                with self.subTest(powered=powered, state=state):
                    p = self.select_level(12, 3)
                    actor = self._player(p, 80, -20, powered, state)
                    self.assertEqual(p.memory[0, 0xA000 + 10], 255,
                                     "Fixture must retain the real top-row floor")
                    p.tick(64)
                    self.assertEqual(self.word(p, actor + 3), (-8 * 32) & 65535,
                                     "Mario passed through the first-row floor")
                    self.assertEqual(p.memory[self.address("_que_state")], 4)
                    self.assertEqual(self.variable(p, "LIVES"), 3)

    def test_big_mario_lands_on_top_of_a_ceiling_instead_of_inside_it(self):
        p = self.select_level(14, 4)
        actor = self._player(p, 64, -20)
        p.tick(64)
        self.assertEqual(self.word(p, actor + 3), (-8 * 32) & 65535)
        self.assertEqual(self.variable(p, "LIVES"), 3)

    def test_visible_lower_body_still_hits_a_wall_while_head_is_above_map(self):
        # The real row-zero ceiling has a gap from x=640 through x=671.
        # Big Mario's head is at -3 px, but his visible body must respect
        # both walls from both air states (jump has two separate sweeps).
        for state in (1, 13):
            for direction, x, velocity in (("left", 640, -4096),
                                            ("right", 665, 4096)):
                with self.subTest(state=state, direction=direction):
                    p = self.select_level(14, 4)
                    actor = self._player(p, x, 4, state=state)
                    self.putword(p, self.address("_pl_vel_x"), velocity)
                    self.putword(p, self.address("_pl_vel_y"),
                                 -128 if state == 13 else 128)
                    self.putword(p, self.address("_plat_grav"), 0)
                    self.putword(p, self.address("_plat_hold_grav"), 0)
                    p.button_press(direction)
                    for _ in range(6):
                        p.tick(1)
                        current_x = self.word(p, actor + 1)
                        if direction == "left":
                            self.assertGreaterEqual(current_x, x * 32,
                                                    "Visible body crossed the left wall")
                        else:
                            self.assertLessEqual(current_x, x * 32,
                                                 "Visible body crossed the right wall")

    def test_fully_above_screen_player_does_not_hit_an_overlapping_offscreen_enemy(self):
        p = self.select_level(1, 1)
        # Bring the real first Goomba into the active linked list normally.
        p.button_press("right")
        p.tick(100)
        p.button_release("right")
        enemy = self.address("_actors") + ACTOR_SIZE * 4
        self.assertTrue(p.memory[enemy] & 0x20, "Fixture Goomba did not activate")
        actor = self._player(p, 128, -24)
        self.putword(p, enemy + 1, 128 * 32)
        self.putword(p, enemy + 3, -24 * 32)
        p.memory[enemy] |= 0x10  # Keep the controlled enemy from being culled.
        p.memory[self.address("_actor_behavior_ids") + 4] = 0
        self.putword(p, self.address("_pl_vel_y"), 0)
        self.putword(p, self.address("_plat_grav"), 0)
        for _ in range(30):
            p.tick(1)
            self.assertEqual(self.variable(p, "LIVES"), 3)
            self.assertNotEqual(
                (p.memory[self.address("_music_current_track_bank")],
                 self.word(p, self.address("_music_current_track"))),
                self.symbols["_song_super_mario_bros_defea_Dat"])
