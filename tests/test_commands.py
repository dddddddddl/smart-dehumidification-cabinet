"""Exercise supplied mock command semantics without opening a serial port."""
import csv
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mock_serial', ROOT / 'tools/mock_serial.py')
mock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mock)


class CommandTests(unittest.TestCase):
    def test_explicit_power_and_parameter_wakeup(self):
        state = {'status': 'running', 'auto': 'off'}
        mock.apply_esp_command({'power': 'standby'}, state)
        self.assertEqual(state['status'], 'standby')
        mock.apply_esp_command({'uv': 'on'}, state)
        self.assertEqual(state['status'], 'running')
        self.assertEqual(state['uv'], 'on')

    def test_manual_power_is_ignored_in_automatic_mode(self):
        state = {'status': 'running', 'auto': 'on', 'p_lvl': 40}
        mock.apply_esp_command({'p_lvl': 100}, state)
        self.assertEqual(state['p_lvl'], 40)
        mock.apply_esp_command({'auto': 'off', 'p_lvl': 70}, state)
        self.assertEqual(state['p_lvl'], 70)

    def test_target_bounds_and_legacy_alias(self):
        state = {'status': 'running', 'auto': 'off', 'target_h': 65}
        mock.apply_esp_command({'target_h': 120}, state)
        self.assertEqual(state['target_h'], 100)
        mock.apply_esp_command({'target_humidity': -10}, state)
        self.assertEqual(state['target_h'], 0)
        self.assertEqual(mock.clamp_percent('invalid', 65), 65)

    def test_voice_reference_keeps_literal_protocol_frames(self):
        with (ROOT / 'voice/commands_english.csv').open() as f:
            rows = list(csv.DictReader(f))
        self.assertEqual(len(rows), 17)
        indexed = {r['command_phrase']: r for r in rows}
        self.assertEqual(indexed['CHANGE-MATERIAL']['send_frame'], 'AA 55 00 0E FB')
        self.assertEqual(indexed['CHANGE-SILICA']['send_frame'], 'AA 55 00 0F FB')
        self.assertEqual(indexed['HEY-SMART-DRYER']['type'], 'wake word')


if __name__ == '__main__':
    unittest.main()
