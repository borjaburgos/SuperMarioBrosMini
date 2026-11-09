"""Shared ROM fixtures for the adversarial-review regression tests."""

import io
import os
import re
from pathlib import Path


ROM_PATH = os.environ.get("SMBMINI_ROM")
ACTIVE = 0x20
ACTOR_SIZE = 56


class RuntimeSupport:
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
        for line in rom.with_suffix(".map").read_text().splitlines():
            match = re.match(r"\s+([0-9A-F]{8})\s+(_\w+)\s", line)
            if match:
                value = int(match[1], 16)
                cls.symbols[match[2]] = (value >> 16, value & 65535)

    def address(self, name):
        return self.symbols[name][1]

    @staticmethod
    def word(p, address):
        return p.memory[address] | p.memory[address + 1] << 8

    @staticmethod
    def putword(p, address, value):
        p.memory[address:address + 2] = [value & 255, (value >> 8) & 255]

    def variable(self, p, name, value=None):
        address = self.address("_script_memory") + 2 * self.variables["VAR_" + name]
        if value is not None:
            self.putword(p, address, value)
        return self.word(p, address)

    def boot(self):
        p = self.emulator(io.BytesIO(self.rom), window="null",
                          sound_emulated=False, cgb=True)
        self.addCleanup(p.stop, save=False)
        p.set_emulation_speed(0)
        return p

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

    def prepare_spring(self, p):
        """Attach Mario to a real spring once; let its normal behavior run next."""
        player = self.address("_actors")
        index = next(i for i in range(1, 25)
                     if p.memory[self.address("_actor_behavior_ids") + i] == 14)
        spring = player + ACTOR_SIZE * index
        top = self.word(p, spring + 10)
        top = top - 65536 if top >= 32768 else top
        base = self.word(p, spring + 3) + top
        x = self.word(p, spring + 1)
        scroll_x = max(0, x // 32 - 80)
        p.memory[spring] = (p.memory[spring] | ACTIVE | 0x10) & ~0x40
        p.memory[self.address("_actor_states") + index] = 2
        p.memory[self.address("_actor_counter_a") + index] = 0
        self.putword(p, player + 1, x)
        self.putword(p, player + 3, base - 8 * 32)
        self.putword(p, self.address("_scroll_x"), scroll_x)
        self.putword(p, self.address("_draw_scroll_x"), scroll_x)
        p.memory[self.address("_actor_attached")] = 1
        self.putword(p, self.address("_last_actor"), spring)
        self.putword(p, self.address("_mp_last_x"), x)
        self.putword(p, self.address("_mp_last_y"), self.word(p, spring + 3))
        self.putword(p, self.address("_pl_vel_x"), 0)
        self.putword(p, self.address("_pl_vel_y"), 0)
        p.memory[self.address("_hold_jump_val")] = 0
        p.memory[self.address("_que_state")] = 4
        return index, spring, base

    @staticmethod
    def replay(p, route):
        held = set()
        for buttons, duration in route:
            keys = set(buttons.split())
            for button in held - keys:
                p.button_release(button)
            for button in keys - held:
                p.button_press(button)
            p.tick(duration)
            held = keys
        return held
