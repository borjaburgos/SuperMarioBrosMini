"""Scripted actor moves must finish before unsigned positions can wrap."""

import json
import unittest
from pathlib import Path

from adversarial_support import ACTOR_SIZE, ROM_PATH, RuntimeSupport


@unittest.skipUnless(ROM_PATH, "Set SMBMINI_ROM to enable compiled-ROM tests")
class ScriptedMovementTests(RuntimeSupport, unittest.TestCase):
    def is_dying(self, p):
        return (p.memory[self.address("_music_current_track_bank")],
                self.word(p, self.address("_music_current_track"))) == self.symbols[
                    "_song_super_mario_bros_defea_Dat"]

    def test_real_timeout_on_final_staircase_finishes_death(self):
        # A continuous route from the normal title screen: no scene selection,
        # positions, physics, or countdown values are changed in memory.
        p = self.boot()
        panics = []
        p.hook_register(*self.symbols["___HandleCrash"],
                        lambda _: panics.append(True), None)
        p.tick(600)
        p.button("start")
        p.tick(300)
        p.button("start")
        p.tick(300)
        fixture = json.loads((Path(__file__).parent / "fixtures" /
                              "world_1_1_timeout_staircase.json").read_text())
        held = self.replay(p, fixture["route"])
        player = self.address("_actors")
        self.assertEqual(self.variable(p, "LIVES"), 3)
        # Other engine corrections can move a timer update by one tick while
        # preserving this same controller route onto the staircase. Observe
        # the remaining natural countdown rather than seeding its value.
        for _ in range(64):
            if self.variable(p, "DEATHTIME") <= 1:
                break
            p.tick(1)
        self.assertEqual(self.variable(p, "DEATHTIME"), 1)
        self.assertEqual(self.word(p, player + 3), 56 * 32)
        self.assertGreater(self.word(p, player + 1), 1510 * 32)
        self.assertLess(self.word(p, player + 1), 1536 * 32)
        for button in held:
            p.button_release(button)
        p.button_press("a")
        death_y = None
        for frame in range(300):
            p.tick(1)
            if self.is_dying(p) and death_y is None:
                death_y = self.word(p, player + 3) / 32
            if frame == 60:
                p.button_release("a")
            if self.variable(p, "LIVES") < 3:
                break
        self.assertIsNotNone(death_y, "Jump did not exercise a real timeout")
        self.assertGreater(death_y, 0)
        self.assertLess(death_y, 30)
        self.assertEqual(self.variable(p, "DEATHTYPE"), 1)
        self.assertEqual(self.variable(p, "LIVES"), 2,
                         "Death move wrapped through the map instead of finishing")
        self.assertFalse(panics)

    def test_death_at_unsigned_y_boundaries_finishes(self):
        # Use the real timer callback and MarioDead script. A hook holds each
        # starting height only until the timer begins death, then releases it.
        # Odd positive heights and even wrapped heights failed indefinitely.
        for y in (0, 1, 2, 3, 29, 30, 31, 32, 128, -1, -8, -16):
            with self.subTest(y=y):
                p = self.select_level(1, 1)
                player = self.address("_actors")
                death_seen = []

                def hold(_):
                    if not self.is_dying(p):
                        self.putword(p, player + 1, 48 * 32)
                        self.putword(p, player + 3, y * 32)
                        p.memory[self.address("_que_state")] = 19

                p.hook_register(*self.symbols["_platform_update"], hold, None)
                self.variable(p, "DEATHTIME", 1)
                for _ in range(300):
                    p.tick(1)
                    if self.is_dying(p):
                        death_seen.append(True)
                    if self.variable(p, "LIVES") < 3:
                        break
                self.assertTrue(death_seen, "Timer did not invoke MarioDead")
                self.assertEqual(self.variable(p, "LIVES"), 2)

    def movement_probe(self, function, start, target, speed=64, collision=False):
        """Exercise an actual native VM move through the existing death script.

        The first death-move opcode is changed only in emulator memory to the
        X/Y/XY variant under test. Its ABI and VM argument block are identical.
        The first native call receives controlled actor/target coordinates;
        the next move's initializer captures its completed result. No ROM or
        project file is changed. Offsets follow GB Studio 4.3.2 actor_t/CTX.
        """
        p = self.select_level(1, 1)
        player = self.address("_actors")
        bank, address = self.symbols["_script_9"]
        bytecode = bytes(p.memory[bank, address:address + 512])
        # VM_ACTOR_MOVE_TO_Y .ARG2, .ACTOR_ATTR_RELATIVE_SNAP_PX.
        offset = bytecode.find(bytes([0x91, 0x08, 0xFF, 0xFD]))
        self.assertGreaterEqual(offset, 0, "Death-move VM fixture was not found")
        opcode = {"x": 0x90, "y": 0x91, "xy": 0x92}[function]
        p.memory[bank, address + offset] = opcode
        p.memory[bank, address + offset + 1] = 0x28 if collision else 0x08
        entered, completed, samples = [], [], []
        initializers = []

        def capture(_):
            initializers.append(True)
            if len(initializers) == 2:
                completed.append((self.word(p, player + 1),
                                  self.word(p, player + 3)))

        def seed(_):
            if completed:
                return
            if not entered:
                entered.append(True)
                # OLDCALL banked entry: six return/context bytes, THIS,
                # signed VM reference, then the attributes byte.
                sp = p.register_file.SP
                context = self.word(p, sp + 6)
                reference = self.word(p, sp + 8)
                if reference & 0x8000:
                    reference -= 65536
                params = (self.word(p, context + 8) + 2 * reference
                          if reference < 0 else
                          self.address("_script_memory") + 2 * reference)
                self.assertEqual(self.word(p, params), 0,
                                 "VM probe must move the player actor")
                self.putword(p, params + 2, target[0])
                self.putword(p, params + 4, target[1])
                self.putword(p, player + 1, start[0])
                self.putword(p, player + 3, start[1])
                p.memory[player + 19] = speed
                if collision:
                    # A point-sized player and one collision-enabled actor
                    # cover just the last partial step and its old overshoot.
                    # Test the rollback amount without involving tile walls.
                    other = player + ACTOR_SIZE
                    for at in (player, other):
                        for bound in (6, 8, 10, 12):
                            self.putword(p, at + bound, 0)
                    self.putword(p, player + 54, other)
                    self.putword(p, other + 54, 0)
                    p.memory[other] = 0x08
                    self.putword(p, other + 1, target[0])
                    self.putword(p, other + 3, target[1])
                    for axis, (before, after) in enumerate(zip(start, target)):
                        if before != after:
                            self.putword(p, other + (6 if axis == 0 else 10),
                                         -speed if after < before else 0)
                            self.putword(p, other + (8 if axis == 0 else 12),
                                         speed if after > before else 0)
            samples.append((self.word(p, player + 1),
                            self.word(p, player + 3)))

        p.hook_register(*self.symbols["_vm_actor_move_to_init"], capture, None)
        p.hook_register(*self.symbols["_vm_actor_move_to_" + function], seed, None)
        self.variable(p, "DEATHTIME", 1)
        for _ in range(160):
            p.tick(1)
            if completed:
                break
        self.assertTrue(entered, "Native scripted-movement probe never ran")
        self.assertTrue(completed, "Native scripted move failed to finish")
        samples.append(completed[0])
        return completed[0], samples

    def test_native_axis_moves_bound_the_last_step(self):
        cases = [(32, 0, 64), (64, 0, 64), (65504, 65535, 64),
                 (65472, 65504, 64), (0, 96, 64), (3200, 3296, 64),
                 (3296, 3200, 64), (16, 0, 32), (65280, 65504, 64)]
        for function in ("x", "y"):
            for before, after, speed in cases:
                with self.subTest(axis=function, start=before, target=after, speed=speed):
                    start = (before, 2048) if function == "x" else (2048, before)
                    target = (after, 2048) if function == "x" else (2048, after)
                    end, samples = self.movement_probe(function, start, target, speed)
                    self.assertEqual(end, target)
                    self.assert_bounded_steps(samples, start, target, speed)

    def test_native_diagonal_moves_bound_both_axes(self):
        for start, target in [((32, 65504), (0, 65535)),
                              ((96, 96), (0, 0)),
                              ((3200, 3328), (3296, 3200))]:
            with self.subTest(start=start, target=target):
                end, samples = self.movement_probe("xy", start, target)
                self.assertEqual(end, target)
                self.assert_bounded_steps(samples, start, target, 64)

    def assert_bounded_steps(self, samples, start, target, speed):
        for axis in (0, 1):
            for before, after in zip(samples, samples[1:]):
                self.assertLessEqual(abs(after[axis] - before[axis]), speed)
                self.assertGreaterEqual(after[axis], min(start[axis], target[axis]))
                self.assertLessEqual(after[axis], max(start[axis], target[axis]))

    def test_actor_collision_restores_the_actual_partial_step(self):
        for function, axis in (("x", 0), ("y", 1), ("xy", 0), ("xy", 1)):
            for direction in (-1, 1):
                with self.subTest(function=function, axis=axis, direction=direction):
                    start = (4096, 4096)
                    target = list(start)
                    target[axis] += direction * 32  # 1 px left of a 2 px step.
                    end, _ = self.movement_probe(function, start, target,
                                                 collision=True)
                    self.assertEqual(end, start,
                                     "Collision restored the full speed instead of the step")


if __name__ == "__main__":
    unittest.main()
