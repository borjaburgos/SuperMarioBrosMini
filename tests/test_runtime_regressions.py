"""ROM-level probes for the GB Studio 4.3.2 port; see tests/README.md."""

import io
import os
import re
import unittest
from pathlib import Path


ROM_PATH = os.environ.get("SMBMINI_ROM")
ACTIVE = 0x20
DISABLED = 0x40
ACTOR_SIZE = 56  # GB Studio 4.3.2 actor_t, including the project plugins.


@unittest.skipUnless(ROM_PATH, "Set SMBMINI_ROM to enable compiled-ROM tests")
class RuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from pyboy import PyBoy
        cls.emulator = PyBoy
        rom = Path(ROM_PATH)
        cls.rom = rom.read_bytes()
        globals_file = rom.parents[2] / "include/data/game_globals.i"
        cls.variables = {name: int(value) for name, value in re.findall(
            r"^(VAR_\w+) = (\d+)", globals_file.read_text(), re.M)}
        cls.symbols = {}
        # .map retains banks above 0x3f, unlike the emitted .sym in some builds.
        for line in rom.with_suffix(".map").read_text().splitlines():
            match = re.match(r"\s+([0-9A-F]{8})\s+(_\w+)\s", line)
            if match:
                value = int(match[1], 16)
                cls.symbols[match[2]] = (value >> 16, value & 0xFFFF)

    def address(self, symbol):
        return self.symbols[symbol][1]

    def variable(self, p, name, value=None):
        address = self.address("_script_memory") + 2 * self.variables["VAR_" + name]
        if value is not None:
            self.putword(p, address, value)
        return self.word(p, address)

    def select_level(self, world, level):
        p = self.boot()
        p.tick(600)
        p.button("start")
        p.tick(300)
        self.variable(p, "SELECTEDWORLD", world)
        self.variable(p, "SELECTEDLEVEL", level)
        p.button("start")
        p.tick(300)
        return p

    def test_tall_world_12_3_starts_at_bottom_and_supports_jumping(self):
        p = self.select_level(12, 3)
        player = self.address("_actors")
        self.assertEqual(p.memory[self.address("_image_tile_height")], 192)
        start_y = self.word(p, player + 3)
        self.assertGreater(start_y, 1400 * 32)
        self.assertLess(start_y, 1536 * 32, "Mario fell through the tall map")
        self.assertGreater(self.word(p, self.address("_draw_scroll_y")), 1300)
        p.button_press("a")
        p.tick(20)
        self.assertLess(self.word(p, player + 3), start_y - 8 * 32)
        p.button_release("a")
        p.tick(100)
        self.assertEqual(self.word(p, player + 3), start_y)
        self.assertEqual(self.variable(p, "LIVES"), 3)

    def test_new_game_resets_powerups_and_demo_counters_in_every_mode(self):
        for mode in (0, 1, 2):
            with self.subTest(mode=mode):
                p = self.boot()
                p.tick(600)
                p.button("start")
                p.tick(300)
                self.variable(p, "MAINMENUSELECTION", mode)
                # State left behind by demo play or a completed/abandoned run.
                for name, value in [("MARIOSTATUS_0", 2), ("HASYOSHI", 1),
                                    ("LIVES", 7), ("COINS", 37),
                                    ("SCORELOWER", 999), ("SCOREUPPER", 1)]:
                    self.variable(p, name, value)
                p.button("start")
                p.tick(300)
                for name in ("MARIOSTATUS_0", "HASYOSHI", "COINS",
                             "SCORELOWER", "SCOREUPPER"):
                    self.assertEqual(self.variable(p, name), 0, name)
                self.assertEqual(self.variable(p, "LIVES"), 3)

    def test_pipe_transition_preserves_current_game_powerups(self):
        p = self.select_level(1, 1)
        self.variable(p, "MARIOSTATUS_0", 2)
        self.variable(p, "HASYOSHI", 1)
        player = self.address("_actors")
        self.putword(p, player + 1, 472 * 32)
        self.putword(p, player + 3, 88 * 32)
        p.button_press("down")
        p.tick(240)
        self.assertEqual(self.variable(p, "CURRENTSUBLEVEL"), 3)
        self.assertEqual(p.memory[self.address("_image_tile_width")], 20)
        self.assertEqual(self.variable(p, "MARIOSTATUS_0"), 2)
        self.assertEqual(self.variable(p, "HASYOSHI"), 1)

    @staticmethod
    def word(p, address):
        return p.memory[address] | p.memory[address + 1] << 8

    @staticmethod
    def putword(p, address, value):
        p.memory[address:address + 2] = [value & 255, (value >> 8) & 255]

    def boot(self, scene=None, x=3, y=15):
        p = self.emulator(io.BytesIO(self.rom), window="null",
                          sound_emulated=False, cgb=True)
        self.addCleanup(p.stop, save=False)
        p.set_emulation_speed(0)
        if scene:
            bank, pointer = self.symbols["_scene_" + scene.replace("-", "_")]
            address = self.address("_start_scene")
            p.memory[0, address:address + 3] = [bank, pointer & 255, pointer >> 8]
            for symbol, value in [("_start_scene_x", x * 256),
                                  ("_start_scene_y", y * 256)]:
                address = self.address(symbol)
                p.memory[0, address:address + 2] = [value & 255, value >> 8]
        return p

    def prepare(self, p):
        # Direct scene starts bypass the title menu's timer initialization.
        variables = self.address("_script_memory")
        self.putword(p, variables, 300)
        self.putword(p, variables + 43 * 2, 1)  # Disable timer for boundary probes.
        self.putword(p, variables + 8 * 2, 16)
        self.putword(p, variables + 9 * 2, 1)

    def test_world_16_map_does_not_overwrite_collision_table(self):
        for scene, width in [("16-1", 208), ("16-2-1", 200), ("16-3", 200)]:
            with self.subTest(scene=scene):
                p = self.boot(scene, y=29)
                p.tick(180)
                self.assertEqual(p.memory[self.address("_image_tile_width")], width)
                self.assertEqual(p.memory[self.address("_image_tile_height")], 32)
                bank = p.memory[self.address("_metatile_collision_bank")]
                pointer = self.word(p, self.address("_metatile_collision_ptr"))
                expected = bytes(p.memory[bank, pointer:pointer + 256])
                actual = bytes(p.memory[0, 0xBF00:0xBFFF]) + bytes([p.memory[0, 0xBFFF]])
                self.assertEqual(actual, expected, "Collision cache was overwritten")
                bank = p.memory[self.address("_image_bank")]
                pointer = self.word(p, self.address("_image_ptr"))
                self.assertEqual(bytes(p.memory[0, 0xA000:0xA000 + width * 32]),
                                 bytes(p.memory[bank, pointer:pointer + width * 32]))

    def test_above_top_jump_keeps_camera_at_top_and_returns(self):
        p = self.boot("16-1", y=29)
        p.tick(180)
        self.assertEqual(self.word(p, self.address("_draw_scroll_y")), 112)
        self.prepare(p)
        player = self.address("_actors")
        self.putword(p, player + 1, 80 * 32)
        self.putword(p, player + 3, -16 * 32)
        p.memory[self.address("_que_state")] = 13  # JUMP_STATE, normal physics.
        self.putword(p, self.address("_pl_vel_y"), -10000)
        p.memory[self.address("_hold_jump_val")] = 0
        above_frames = 0
        returned = False
        for _ in range(120):
            p.tick(1)
            y = self.word(p, player + 3)
            if y & 0x8000:
                above_frames += 1
                self.assertEqual(self.word(p, self.address("_draw_scroll_y")), 0)
            elif above_frames:
                returned = True
                break
        self.assertGreater(above_frames, 10, "Jump fixture never reached offscreen physics")
        self.assertTrue(returned, "Player did not fall back into the scene")

    def test_elevators_remain_active_and_loop_both_directions(self):
        p = self.boot("1-2", x=145, y=12)
        # Keep the camera here while the real elevator behavior runs for >2 loops.
        def hold_player(_):
            self.putword(p, self.address("_actors") + 1, 145 * 256)
            self.putword(p, self.address("_actors") + 3, 96 * 32)
            p.memory[self.address("_que_state")] = 19  # BLANK_STATE
        p.hook_register(*self.symbols["_platform_update"], hold_player, None)
        p.tick(180)
        self.prepare(p)
        elevators = [i for i in range(1, 25)
                     if p.memory[self.address("_actor_behavior_ids") + i] == 16]
        self.assertEqual(len(elevators), 4)
        previous = {}
        wraps = {i: 0 for i in elevators}
        for _ in range(1500):
            p.tick(1)
            for i in elevators:
                actor = self.address("_actors") + ACTOR_SIZE * i
                self.assertTrue(p.memory[actor] & ACTIVE, f"Elevator {i} vanished")
                self.assertEqual(p.memory[self.address("_actor_states") + i], 1)
                y = self.word(p, actor + 3)
                self.assertLess(y, 152 * 32, "Elevator Y wrapped as an unsigned coordinate")
                if i in previous and abs(y - previous[i]) > 100 * 32:
                    wraps[i] += 1
                previous[i] = y
        for i, count in wraps.items():
            self.assertGreaterEqual(count, 2, f"Elevator {i} did not complete two loops")

    def test_wrapping_elevator_detaches_only_its_own_rider(self):
        for velocity in (-8, 8):
            for riding_wrapping_elevator in (True, False):
                with self.subTest(velocity=velocity, own_rider=riding_wrapping_elevator):
                    p = self.boot("1-2", x=145, y=12)
                    seeded = []
                    results = []
                    elevators = []

                    def hold_and_wrap(_):
                        p.memory[self.address("_que_state")] = 19
                        if not elevators or seeded:
                            return
                        seeded.append(True)
                        moving = self.address("_actors") + ACTOR_SIZE * elevators[0]
                        other = self.address("_actors") + ACTOR_SIZE * elevators[1]
                        for i in elevators:
                            self.putword(p, self.address("_actor_vel_y") + 2 * i, 0)
                        self.putword(p, self.address("_actor_vel_y") + 2 * elevators[0], velocity)
                        self.putword(p, moving + 3, 0 if velocity < 0 else 152 * 32 - 1)
                        self.putword(p, self.address("_last_actor"),
                                     moving if riding_wrapping_elevator else other)
                        p.memory[self.address("_actor_attached")] = 1

                    def capture(_):
                        if seeded and not results:
                            moving = self.address("_actors") + ACTOR_SIZE * elevators[0]
                            results.append((p.memory[self.address("_actor_attached")],
                                            self.word(p, moving + 3)))

                    p.hook_register(*self.symbols["_platform_update"], hold_and_wrap, None)
                    p.hook_register(*self.symbols["_camera_update"], capture, None)
                    p.tick(180)
                    elevators.extend(i for i in range(1, 25)
                                     if p.memory[self.address("_actor_behavior_ids") + i] == 16)
                    self.assertEqual(len(elevators), 4)
                    for _ in range(15):
                        p.tick(1)
                        if results:
                            break
                    self.assertTrue(results, "Elevator update never completed")
                    attached, y = results[0]
                    self.assertEqual(attached, int(not riding_wrapping_elevator))
                    self.assertEqual(y, 144 * 32 - 16 if velocity < 0 else 8 * 32 + 15)

    def test_platform_detachment_and_valid_carry(self):
        for state, symbol in [(4, "_ground_state"), (7, "_crouch_state"), (22, "_swim_state")]:
            for flags in (ACTIVE, ACTIVE | DISABLED, 0):
                with self.subTest(state=symbol, flags=flags):
                    p = self.boot("16-1", y=29)
                    p.tick(180)
                    self.prepare(p)
                    player = self.address("_actors")
                    platform = player + ACTOR_SIZE
                    calls = []
                    results = []

                    def seed_attachment(_):
                        if calls:
                            return
                        calls.append(True)
                        self.putword(p, player + 1, 64 * 32)
                        self.putword(p, player + 3, 140 * 32)
                        self.putword(p, platform + 1, 66 * 32)
                        self.putword(p, platform + 3, 150 * 32)
                        for offset, value in [(6, 0), (8, 511), (10, 0), (12, 255)]:
                            self.putword(p, platform + offset, value)
                        p.memory[platform] = flags
                        p.memory[self.address("_actor_behavior_ids") + 1] = 0
                        p.memory[self.address("_actor_attached")] = 1
                        self.putword(p, self.address("_last_actor"), platform)
                        self.putword(p, self.address("_mp_last_x"), 64 * 32)
                        self.putword(p, self.address("_mp_last_y"), 148 * 32)
                        for variable in ("_pl_vel_x", "_pl_vel_y", "_deltaX", "_deltaY"):
                            self.putword(p, self.address(variable), 0)
                        p.memory[self.address("_hold_jump_val")] = 0

                    p.hook_register(*self.symbols[symbol], seed_attachment, None)
                    def select_state(_):
                        if not calls:
                            p.memory[self.address("_que_state")] = state
                    p.hook_register(*self.symbols["_platform_update"], select_state, None)
                    def capture_result(_):
                        if calls and not results:
                            results.append((p.memory[self.address("_actor_attached")],
                                            self.word(p, player + 1), self.word(p, player + 3)))
                    p.hook_register(*self.symbols["_camera_update"], capture_result, None)
                    for _ in range(15):
                        p.tick(1)
                        if results:
                            break
                    self.assertTrue(calls, "Movement state never ran")
                    self.assertTrue(results, "Movement state never returned")
                    attached, x, y = results[0]
                    if flags == ACTIVE:
                        self.assertEqual(attached, 1)
                        self.assertEqual(x, 66 * 32, "Valid platform X movement was lost")
                        self.assertEqual(y, 142 * 32, "Valid platform Y movement was lost")
                    else:
                        self.assertEqual(attached, 0, "Invalid platform still attached")
                        self.assertEqual(x, 64 * 32, "Inherited invalid platform X movement")
                        self.assertLess(y, 141 * 32, "Inherited invalid platform Y movement")

    def test_world_1_enemy_stomp_does_not_corrupt_script_execution(self):
        # Real controller input only: this route crashed at gameplay frame 349.
        # Keep it separate from the short title/movement smoke test.
        p = self.boot()
        panics = []
        updates = []
        frame = -1

        def panic(_):
            panics.append((frame, p.memory[self.address("__current_bank")]))

        def update(_):
            updates.append(frame)

        p.hook_register(*self.symbols["___HandleCrash"], panic, None)
        # Scripts keep updating during the normal death animation, when physics pauses.
        p.hook_register(*self.symbols["_script_runner_update"], update, None)
        p.tick(600)
        p.button("start")
        p.tick(300)
        p.button("start")
        p.tick(300)
        self.assertFalse(panics, "Crashed before gameplay")
        p.button_press("right")
        p.button_press("b")
        stomped = False
        for frame in range(600):
            if frame % 60 == 0:
                p.button_press("a")
            elif frame % 60 == 30:
                p.button_release("a")
            p.tick(1)
            self.assertFalse(panics, f"Kernel panic (gameplay frame, ROM bank): {panics}")
            # Actor 6 is the Goomba whose stomp script corrupted the first VM context.
            stomped |= p.memory[self.address("_actor_states") + 6] == 2
        self.assertTrue(stomped, "Route never exercised the enemy stomp")
        self.assertTrue(updates and updates[-1] >= 590, "Engine stopped updating after the stomp")

    def test_world_16_giant_block_hits_stay_in_the_original_footprint(self):
        # 16-2's opening row: giant brick, coin block, giant brick. Test both
        # halves of the underside. The old compiler folded (v >> 1) << 1 to v,
        # so a brick hit on row 25 created extra tiles on row 26 (and, for a
        # right-half hit, replaced part of the neighboring coin block).
        for x, powered_up in [(14, False), (15, False), (14, True), (15, True),
                              (16, False), (17, False)]:
            with self.subTest(x=x, powered_up=powered_up):
                p = self.boot()
                variables = self.address("_script_memory")
                p.tick(600)
                p.button("start")
                p.tick(300)
                # Select the actual menu's 16-2 destination; allow normal
                # level initialization, including event handlers and graphics.
                self.putword(p, variables + 58 * 2, 16)  # SelectedWorld
                self.putword(p, variables + 60 * 2, 2)   # SelectedLevel
                p.button("start")
                p.tick(300)
                self.assertEqual(p.memory[self.address("_image_tile_width")], 200)
                self.assertEqual(p.memory[self.address("_image_tile_height")], 32)
                self.assertEqual(self.word(p, variables + 8 * 2), 16)
                self.assertEqual(self.word(p, variables + 9 * 2), 2)

                player = self.address("_actors")
                self.putword(p, player + 1, x * 256)
                self.putword(p, player + 3, 29 * 256)
                self.putword(p, variables + 43 * 2, 1)  # DisableTimer
                self.putword(p, variables + 21 * 2, int(powered_up))
                p.tick(8)
                original = bytes(p.memory[0, 0xA000:0xB900])
                left = x & ~1
                footprint = {row * 200 + col for row in (24, 25)
                             for col in (left, left + 1)}
                expected = bytearray(original)
                coin_block = left == 16
                if coin_block:
                    for row, tiles in [(24, (211, 212)), (25, (213, 214))]:
                        for col, tile in zip((left, left + 1), tiles):
                            expected[row * 200 + col] = tile
                elif powered_up:
                    for offset in footprint:
                        expected[offset] = 0

                # Actor 6 is this scene's giant block bump effect.
                effect = player + ACTOR_SIZE * 6
                self.assertEqual(p.memory[self.address("_actor_behavior_ids") + 6], 17)
                touched = set()
                saw_effect = False
                p.button_press("a")
                for frame in range(80):
                    p.tick(1)
                    if frame == 25:
                        p.button_release("a")
                    current = bytes(p.memory[0, 0xA000:0xB900])
                    changed = {i for i, (a, b) in enumerate(zip(original, current)) if a != b}
                    self.assertFalse(changed - footprint,
                                     "Block hit wrote outside its original 2x2 footprint: "
                                     + str([(i % 200, i // 200) for i in sorted(changed - footprint)]))
                    touched.update(changed)
                    if p.memory[effect] & ACTIVE:
                        saw_effect = True
                        self.assertEqual(self.word(p, effect + 1), left * 256)
                        self.assertEqual(self.word(p, effect + 3), 25 * 256)
                self.assertTrue(saw_effect, "Jump never triggered the giant block effect")
                self.assertEqual(touched, footprint, "Not all four block tiles were updated")
                self.assertEqual(current, bytes(expected))
                self.assertFalse(p.memory[effect] & ACTIVE, "Bump effect was not cleaned up")

    def test_title_start_and_player_movement(self):
        p = self.boot()
        p.tick(600)
        variables = self.address("_script_memory")
        self.assertEqual(self.word(p, variables + 46 * 2), 1, "Demo did not start")
        p.button("start")
        p.tick(300)
        p.button("start")  # Select one-player game.
        p.tick(300)
        self.assertEqual(self.word(p, variables + 46 * 2), 0)
        self.assertEqual(self.word(p, variables + 2 * 2), 3)
        player_x = self.address("_actors") + 1
        before = self.word(p, player_x)
        p.button_press("right")
        p.tick(35)
        self.assertGreater(self.word(p, player_x), before + 8 * 32)


if __name__ == "__main__":
    unittest.main()
