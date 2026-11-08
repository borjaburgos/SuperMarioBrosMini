"""Exercise the real palette-copy native function on CGB and DMG hardware modes."""

import io
import re
import unittest
from pathlib import Path

from adversarial_support import ROM_PATH, RuntimeSupport


@unittest.skipUnless(ROM_PATH, "Set SMBMINI_ROM to check native palette commits")
class PaletteCopyTests(RuntimeSupport, unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        text = Path(ROM_PATH).with_suffix(".map").read_text()
        match = re.search(r"^\s+([0-9A-F]{8})\s+XG\$copy_scene_palette_colors\$0\$0\s", text, re.M)
        if not match:
            raise AssertionError("Palette fixture requires GB Studio's debug function-end symbol")
        value = int(match[1], 16)
        cls.function_end = (value >> 16, value & 65535)

    def copy_palette(self, cgb, commit):
        from pyboy.core import bootrom
        # PyBoy auto-selects a CGB boot ROM from this cartridge's header; that
        # boot ROM overrides cgb=False. Select both boot ROMs explicitly so the
        # DMG case really exercises the DMG branch rather than CGB hardware.
        boot = Path(bootrom.__file__).with_name("bootrom_cgb.bin" if cgb else "bootrom_dmg.bin")
        p = self.emulator(io.BytesIO(self.rom), window="null", sound_emulated=False,
                          cgb=cgb, bootrom=str(boot))
        self.addCleanup(p.stop, save=False)
        p.set_emulation_speed(0)
        p.tick(600)
        p.button("start")
        p.tick(300)
        self.assertEqual(bool(p.memory[self.address("__is_CGB")]), cgb,
                         "The requested hardware boot mode was not entered")

        script_bank, script_ptr = self.symbols["_script_6"]  # Shared InitLevel.
        old_bank, old_ptr = self.symbols["_vm_load_meta_tiles"]
        bank, pointer = self.symbols["_copy_scene_palette_colors"]
        # OP_VM_CALL_NATIVE stores its pointer big-endian, followed by its bank.
        old_call = bytes([0x2D, old_ptr >> 8, old_ptr & 255, old_bank])
        script = self.rom[script_bank * 0x4000:(script_bank + 1) * 0x4000]
        offset = script.find(old_call, script_ptr - 0x4000)
        self.assertGreaterEqual(offset, script_ptr - 0x4000)
        self.assertLess(offset, script_ptr - 0x4000 + 2048)
        # Only this emulator's VM call operand changes. The compiled native
        # function and its normal banked-call/THIS calling convention stay real.
        p.memory[script_bank, 0x4000 + offset:0x4000 + offset + 4] = [
            0x2D, pointer >> 8, pointer & 255, bank]
        record = {}

        def hardware_palette():
            if not cgb:
                return p.memory[0xFF47]  # BGP
            colors = []
            for index in range(64):
                p.memory[0xFF68] = index
                colors.append(p.memory[0xFF69])
            return bytes(colors)

        def enter(_):
            if record:
                return
            # An LCD-off fixture permits deterministic palette reads/writes
            # without mode-3 access restrictions; game logic is not exercised.
            p.memory[0xFF40] &= 0x7F
            p.memory[0xFF47] = 0x1B
            p.memory[self.address("_DMG_palette")] = 0x1B
            if cgb:
                p.memory[0xFF68] = 0x80
                for _ in range(64):
                    p.memory[0xFF69] = 0
            record["before"] = hardware_palette()
            # GB Studio 4.3.2 OLDCALL banked entry: saved context is six bytes,
            # followed by THIS. SCRIPT_CTX.stack_ptr is at byte offset eight.
            this = self.word(p, p.register_file.SP + 6)
            stack = self.word(p, this + 8)
            source_bank, source = self.symbols["_scene_12_3"]
            self.putword(p, stack - 2, source_bank)
            self.putword(p, stack - 4, source)
            self.putword(p, stack - 6, int(commit))

        def capture(_):
            if "before" not in record or "after" in record:
                return
            record["after"] = hardware_palette()
            start = self.address("_BkgPalette")
            record["staged"] = bytes(p.memory[start:start + 64]) if cgb else p.memory[self.address("_DMG_palette")]

        p.hook_register(bank, pointer, enter, None)
        p.hook_register(*self.function_end, capture, None)
        self.variable(p, "SELECTEDWORLD", 1)
        self.variable(p, "SELECTEDLEVEL", 1)
        p.button("start")
        p.tick(300)
        self.assertIn("after", record, "The real palette-copy function did not finish")
        self.assertNotEqual(record["staged"], record["before"], "Source palette did not differ from the fixture")
        return record

    def test_cgb_commit_applies_and_unchecked_only_stages(self):
        for commit in (False, True):
            with self.subTest(commit=commit):
                result = self.copy_palette(True, commit)
                self.assertEqual(result["after"], result["staged"] if commit else result["before"])

    def test_dmg_commit_applies_and_unchecked_only_stages(self):
        for commit in (False, True):
            with self.subTest(commit=commit):
                result = self.copy_palette(False, commit)
                self.assertEqual(result["after"], result["staged"] if commit else result["before"])


if __name__ == "__main__":
    unittest.main()
