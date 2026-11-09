"""Keep controller-route results independent of previous emulator sessions."""

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

import replay_levels


class ReplayHarnessTests(unittest.TestCase):
    def test_main_uses_and_closes_a_fresh_emulator_for_each_route(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rom = root / "game.gb"
            rom.write_bytes(b"test ROM; PyBoy is mocked")
            fixture = root / "routes.json"
            records = [{"world": 1, "level": level, "route": []} for level in (1, 2)]
            fixture.write_text(json.dumps({"rom_sha256": "different", "levels": records}))
            argv = ["replay_levels.py", str(rom), str(fixture), "--allow-different-rom"]

            first, second = Mock(), Mock()
            first.run.return_value = {"passed": True}
            second.run.return_value = {"passed": True}
            with patch.object(sys, "argv", argv), patch.object(
                    replay_levels, "Replay", side_effect=[first, second]) as constructor:
                self.assertEqual(replay_levels.main(), 0)
            self.assertEqual(constructor.call_count, 2)
            first.run.assert_called_once_with(records[0])
            second.run.assert_called_once_with(records[1])
            first.close.assert_called_once_with()
            second.close.assert_called_once_with()

            # Exceptions must also close that record's emulator, and must not
            # leave a partially used instance available to a later record.
            failing = Mock()
            failing.run.side_effect = ValueError("invalid route")
            with patch.object(sys, "argv", argv), patch.object(
                    replay_levels, "Replay", return_value=failing) as constructor:
                with self.assertRaisesRegex(ValueError, "invalid route"):
                    replay_levels.main()
            constructor.assert_called_once_with(rom, False)
            failing.close.assert_called_once_with()

    def test_a_replay_instance_rejects_reuse_before_touching_emulator_state(self):
        emulator = object.__new__(replay_levels.Replay)
        emulator._has_run = True
        with self.assertRaisesRegex(RuntimeError, "single-use"):
            emulator.run({})


if __name__ == "__main__":
    unittest.main()
