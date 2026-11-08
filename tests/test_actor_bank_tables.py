"""Behavior checks for const tables used by independently banked actors."""

import unittest

from adversarial_support import ACTIVE, ACTOR_SIZE, ROM_PATH, RuntimeSupport


@unittest.skipUnless(ROM_PATH, "Set SMBMINI_ROM to test banked actor behavior")
class ActorBankTableTests(RuntimeSupport, unittest.TestCase):
    def test_firebar_damage_follows_every_authored_ray(self):
        dx = [0, 3, 6, 7, 8, 7, 6, 3, 0, -3, -6, -7, -8, -7, -6, -3]
        dy = [-8, -7, -6, -3, 0, 3, 6, 7, 8, 7, 6, 3, 0, -3, -6, -7]
        for phase in range(16):
            with self.subTest(phase=phase):
                p = self.select_level(1, 4)
                player = self.address("_actors")
                index = next(i for i in range(1, 25)
                             if p.memory[self.address("_actor_behavior_ids") + i] == 9)
                firebar = player + ACTOR_SIZE * index
                x = self.word(p, firebar + 1) // 32 + 4 + 2 * dx[phase] - 4
                y = self.word(p, firebar + 3) // 32 - 28 + 2 * dy[phase] - 4
                for i in range(1, 25):
                    p.memory[player + ACTOR_SIZE * i] &= ~ACTIVE
                p.memory[firebar] |= ACTIVE
                p.memory[self.address("_actor_states") + index] = 1
                self.putword(p, self.address("_draw_scroll_x"), 160)
                self.putword(p, self.address("_scroll_x"), 160)

                def hold(_):
                    self.putword(p, player + 1, x * 32)
                    self.putword(p, player + 3, y * 32)
                    self.putword(p, self.address("_draw_scroll_x"), 160)
                    self.putword(p, self.address("_scroll_x"), 160)
                    p.memory[firebar] |= ACTIVE
                    p.memory[self.address("_actor_states") + index] = 1
                    p.memory[self.address("_que_state")] = 19
                    p.memory[self.address("_actor_counter_a") + index] = phase
                    p.memory[self.address("_game_time")] = 1

                p.hook_register(*self.symbols["_platform_update"], hold, None)
                try:
                    p.tick(32)
                finally:
                    p.hook_deregister(*self.symbols["_platform_update"])
                self.assertEqual(
                    (p.memory[self.address("_music_current_track_bank")],
                     self.word(p, self.address("_music_current_track"))),
                    self.symbols["_song_super_mario_bros_defea_Dat"],
                    "Mario on a firebar ray was not hit")

    def test_firebar_does_not_damage_the_old_wrong_bank_location(self):
        p = self.select_level(1, 4)
        player = self.address("_actors")
        index = next(i for i in range(1, 25)
                     if p.memory[self.address("_actor_behavior_ids") + i] == 9)
        firebar = player + ACTOR_SIZE * index
        for i in range(1, 25):
            p.memory[player + ACTOR_SIZE * i] &= ~ACTIVE
        p.memory[firebar] |= ACTIVE
        p.memory[self.address("_actor_states") + index] = 1
        self.putword(p, self.address("_draw_scroll_x"), 210)
        self.putword(p, self.address("_scroll_x"), 210)

        def hold(_):
            self.putword(p, player + 1, 290 * 32)
            self.putword(p, player + 3, 51 * 32)
            self.putword(p, self.address("_draw_scroll_x"), 210)
            self.putword(p, self.address("_scroll_x"), 210)
            p.memory[firebar] |= ACTIVE
            p.memory[self.address("_actor_states") + index] = 1
            p.memory[self.address("_que_state")] = 19
            p.memory[self.address("_actor_counter_a") + index] = 0
            p.memory[self.address("_game_time")] = 1

        p.hook_register(*self.symbols["_platform_update"], hold, None)
        try:
            for _ in range(60):
                p.tick(1)
                self.assertNotEqual(
                    (p.memory[self.address("_music_current_track_bank")],
                     self.word(p, self.address("_music_current_track"))),
                    self.symbols["_song_super_mario_bros_defea_Dat"])
        finally:
            p.hook_deregister(*self.symbols["_platform_update"])

    def test_real_spring_compression_keeps_mario_on_the_spring(self):
        p = self.select_level(2, 1)
        player = self.address("_actors")
        index, spring, base = self.prepare_spring(p)
        compressed = set()
        launched = False
        compression_positions = []

        def capture_compression(_):
            if p.memory[self.address("_actor_states") + index] == 2:
                compression_positions.append((
                    p.memory[self.address("_actor_counter_a") + index],
                    self.word(p, player + 3)))

        p.hook_register(*self.symbols["_ground_state"], capture_compression, None)
        self.addCleanup(p.hook_deregister, *self.symbols["_ground_state"])
        p.button_press("a")
        for _ in range(40):
            p.tick(1)
            if p.memory[self.address("_actor_states") + index] == 2:
                phase = p.memory[self.address("_actor_counter_a") + index]
                compressed.add(phase)
                y = self.word(p, player + 3)
                self.assertGreaterEqual(y, base - 8 * 32)
                self.assertLessEqual(y, base)
            elif compressed and p.memory[self.address("_que_state")] == 13:
                launched = True
            self.assertEqual(self.variable(p, "LIVES"), 3)
        self.assertGreaterEqual(len(compressed), 3, "The fixture never compressed the spring")
        self.assertTrue(launched, "The spring never launched Mario after compression")
        self.assertTrue(compression_positions, "Spring compression was not observed before physics")
        for phase, y in compression_positions:
            self.assertEqual(y, base + [-8, -4, 0, -4, -8][phase] * 32)


if __name__ == "__main__":
    unittest.main()
