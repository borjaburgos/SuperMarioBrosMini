"""Prevent GBVM writes from overrunning C engine fields."""

import json
import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_PATH = os.environ.get("SMBMINI_BUILD")
WRITES = re.compile(r"\bVM_SET_(?:CONST_)?(U?INT(?:8|16))\s+_(\w+)")
BYTE_FIELDS = {"que_state", "plat_state", "enemy_bounce", "current_vine_tile_x",
               "actor_attached", "dj_val", "nocollide", "plat_hold_jump_max"}


def strings(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for child in value.values():
            yield from strings(child)
    elif isinstance(value, list):
        for child in value:
            yield from strings(child)


class ScriptMemoryWidthTests(unittest.TestCase):
    def test_project_byte_fields_are_not_written_as_words(self):
        checked = 0
        for path in (ROOT / "project").rglob("*.gbsres"):
            for text in strings(json.loads(path.read_text())):
                for kind, field in WRITES.findall(text):
                    if field in BYTE_FIELDS:
                        checked += 1
                        with self.subTest(file=str(path.relative_to(ROOT)), field=field):
                            self.assertFalse(kind.endswith("16"), "Word write to a byte field")
        self.assertGreater(checked, 100, "No enemy-script fixtures found")

    @unittest.skipUnless(BUILD_PATH, "Set SMBMINI_BUILD to audit compiled scripts against C storage")
    def test_compiled_writes_fit_debug_symbol_sizes(self):
        build = Path(BUILD_PATH)
        cdb_files = list((build / "build/rom").glob("*.cdb"))
        self.assertEqual(len(cdb_files), 1, "Expected one linker CDB file")
        sizes = {name: int(size) for name, size in re.findall(
            r"S:G\$(\w+)\$[^\n]*?\(\{(\d+)\}", cdb_files[0].read_text())}
        self.assertEqual(sizes.get("que_state"), 1)
        self.assertEqual(sizes.get("enemy_bounce"), 1)
        self.assertEqual(sizes.get("plat_run_boost"), 2)
        checked = 0
        failures = []
        for path in (build / "src/data").glob("*.s"):
            text = path.read_text()
            writes = WRITES.findall(text)
            writes += re.findall(r"\.R_REF_MEM_SET\s+\.MEM_(I8|U8|I16),\s*_(\w+)", text)
            for kind, field in writes:
                if field in sizes:
                    checked += 1
                    width = 2 if kind.endswith("16") else 1
                    if width > sizes[field]:
                        failures.append(f"{path.name}: {width}-byte write to {field} ({sizes[field]} byte)")
        self.assertGreater(checked, 100, "Compiled script audit did not find expected writes")
        self.assertFalse(failures, "\n".join(failures[:20]))


if __name__ == "__main__":
    unittest.main()
