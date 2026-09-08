"""Check the unified-diff metadata GB Studio uses when loading engine plugins."""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")


class EnginePatchTests(unittest.TestCase):
    def test_hunk_line_counts(self):
        patches = list((ROOT / "plugins").rglob("*.patch"))
        self.assertTrue(patches, "No engine patches found")
        for path in patches:
            lines = path.read_text().splitlines()
            for index, line in enumerate(lines):
                match = HUNK.match(line)
                if not match:
                    continue
                with self.subTest(patch=str(path.relative_to(ROOT)), line=index + 1):
                    old_count = new_count = 0
                    for body in lines[index + 1:]:
                        if HUNK.match(body) or body.startswith(("diff ", "Index: ")):
                            break
                        if body.startswith("\\"):
                            continue  # "No newline at end of file"
                        self.assertTrue(body.startswith((" ", "+", "-")), body)
                        old_count += body.startswith((" ", "-"))
                        new_count += body.startswith((" ", "+"))
                    self.assertEqual(old_count, int(match[2] or 1), "Old line count")
                    self.assertEqual(new_count, int(match[4] or 1), "New line count")


if __name__ == "__main__":
    unittest.main()
