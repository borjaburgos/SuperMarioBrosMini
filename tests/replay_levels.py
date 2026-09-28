"""Replay recorded controller-only level routes with a compiled GB Studio ROM.

Level selection is the sole memory write: it changes the menu's selected world
and level before pressing Start. Gameplay uses only emulated button input.
"""

import argparse
import hashlib
import io
import json
import re
from pathlib import Path


class Replay:
    def __init__(self, rom, visible=False):
        from pyboy import PyBoy

        self.symbols = {}
        for line in rom.with_suffix(".map").read_text().splitlines():
            match = re.match(r"\s+([0-9A-F]{8})\s+(_\w+)\s", line)
            if match:
                value = int(match[1], 16)
                self.symbols[match[2]] = (value >> 16, value & 65535)
        globals_file = rom.parents[2] / "include/data/game_globals.i"
        self.variables = {name: int(value) for name, value in re.findall(
            r"^(VAR_\w+) = (\d+)", globals_file.read_text(), re.M)}
        self.p = PyBoy(str(rom), window="SDL2" if visible else "null",
                       sound_emulated=False, cgb=True)
        self.p.set_emulation_speed(0)
        self.panics = []
        self.p.hook_register(*self.symbols["___HandleCrash"],
                             lambda _: self.panics.append(self.p.frame_count), None)
        self.p.tick(600)
        self.p.button("start")
        self.p.tick(300)
        saved = io.BytesIO()
        self.p.save_state(saved)
        self.menu = saved.getvalue()

    def variable(self, name, value=None):
        address = self.symbols["_script_memory"][1] + 2 * self.variables["VAR_" + name]
        if value is not None:
            self.p.memory[address:address + 2] = [value & 255, value >> 8]
        return self.p.memory[address] | self.p.memory[address + 1] << 8

    def run(self, record):
        world, level = record["world"], record["level"]
        self.p.load_state(io.BytesIO(self.menu))
        self.panics.clear()
        self.variable("SELECTEDWORLD", world)
        self.variable("SELECTEDLEVEL", level)
        self.p.button("start")
        self.p.tick(300)
        held = set()
        lives = self.variable("LIVES")
        lost_life = completed = False
        frames = 0
        for buttons, duration in record["route"]:
            keys = set(buttons.split())
            if not keys <= {"a", "b", "up", "down", "left", "right", "start", "select"}:
                raise ValueError(f"Unknown buttons: {buttons}")
            if not isinstance(duration, int) or duration <= 0:
                raise ValueError(f"Invalid frame duration: {duration}")
            for button in held - keys:
                self.p.button_release(button)
            for button in keys - held:
                self.p.button_press(button)
            held = keys
            for frame in range(0, duration, 4):
                count = min(4, duration - frame)
                self.p.tick(count)
                frames += count
                current_lives = self.variable("LIVES")
                lost_life |= current_lives < lives
                lives = current_lives
                completed |= bool(self.variable("LEVELCOMPLETE"))
        destination = [self.variable("CURRENTWORLD"), self.variable("CURRENTLEVEL")]
        # A warp-zone transition alone is not a level clear.
        passed = (completed and destination != [world, level]
                  and not lost_life and not self.panics)
        return {"world": world, "level": level, "passed": passed,
                "completion_observed": completed, "destination": destination,
                "lost_life": lost_life, "panics": list(self.panics), "frames": frames}

    def close(self):
        self.p.stop(save=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("routes", type=Path, help="Recorded route JSON")
    parser.add_argument("--level", help="Replay only this WORLD-LEVEL, e.g. 9-1")
    parser.add_argument("--visible", action="store_true")
    parser.add_argument("--allow-different-rom", action="store_true")
    parser.add_argument("--output", type=Path, help="Optional JSON results file")
    args = parser.parse_args()
    records = json.loads(args.routes.read_text())
    digest = hashlib.sha256(args.rom.read_bytes()).hexdigest()
    if digest != records["rom_sha256"] and not args.allow_different_rom:
        parser.error("ROM differs from recorded build; use --allow-different-rom to test a newer build")
    routes = [r for r in records["levels"] if not args.level
              or f'{r["world"]}-{r["level"]}' == args.level]
    if not routes:
        parser.error("No matching recorded levels")
    emulator = Replay(args.rom, args.visible)
    results = []
    try:
        for record in routes:
            result = emulator.run(record)
            results.append(result)
            print(json.dumps(result), flush=True)
    finally:
        emulator.close()
    if args.output:
        args.output.write_text(json.dumps({"rom_sha256": digest, "results": results}, indent=2) + "\n")
    return int(any(not result["passed"] for result in results))


if __name__ == "__main__":
    raise SystemExit(main())
