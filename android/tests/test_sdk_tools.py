"""SDK evidence and strict-link regressions; fixtures contain no game bytes."""
from pathlib import Path
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import sdk_audit
import sdk_evidence
import strict_link


class InventoryTests(unittest.TestCase):
    def test_real_baseline_preserves_duplicate_and_nonzero_slots(self):
        report = sdk_audit.inventory()
        self.assertEqual((report['baseline_count'], report['table_count'], report['slot_count']), (102, 15, 103))
        rows = {row['symbol']: row for row in report['contracts']}
        self.assertEqual([s['offset'] for s in rows['func_002A0C10']['slots']], [0, 24])
        self.assertEqual(rows['func_00100000']['slots'][0]['offset'], 20)
        self.assertTrue(all(not r['c_candidates'] for r in rows.values()))
        self.assertEqual(report['tables']['D_002EB190']['incoming'][0]['table'], 'D_002EB330')

    def test_addended_data_reference_is_retained(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'shape.s'
            path.write_text('.section .data\n.p2align 4\nA:\n.space 12\n.long B + 4\n.zero 8\nB:\n.long 0\n')
            table = sdk_audit.tables(directory)
            self.assertEqual(table['A']['size'], 24)
            self.assertEqual(table['A']['slots'][0]['offset'], 12)
            self.assertEqual(table['A']['slots'][0]['addend'], 4)

    def test_compare_reports_names_even_when_count_is_unchanged(self):
        original = [r['symbol'] for r in sdk_audit.baseline()]
        result = sdk_audit.compare(original[1:] + ['unexpected_dependency'])
        self.assertEqual(result['resolved'], original[:1])
        self.assertEqual(result['new'], ['unexpected_dependency'])


class EvidenceTests(unittest.TestCase):
    def test_checks_every_slot_and_rejects_mismatched_address_map(self):
        audit = sdk_audit.inventory()
        flat = bytearray(0x2ff180 - sdk_evidence.BASE)
        for row in audit['contracts']:
            for slot in row['slots']:
                offset = int(slot['table'][2:], 16) + slot['offset'] - sdk_evidence.BASE
                struct.pack_into('<I', flat, offset, int(row['ps2_address'], 16))
        self.assertEqual(len(sdk_evidence.verify_slots(flat, audit)), 103)
        flat[0x2d3ff8 - sdk_evidence.BASE] ^= 1
        with self.assertRaisesRegex(ValueError, 'Address map mismatch'):
            sdk_evidence.verify_slots(flat, audit)

    def test_unknown_program_is_rejected_before_disassembly(self):
        with patch.object(sdk_evidence.bt4_disc, 'flat_image', return_value=b'synthetic only'):
            with self.assertRaisesRegex(ValueError, 'Unknown executable checksum'):
                sdk_evidence.identify_elf(b'')

    @unittest.skipUnless(importlib.util.find_spec('spimdisasm'), 'install android/requirements-evidence.txt')
    def test_r5900_lq_and_return_delay_slot_disassemble(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            flat = bytearray(0x2ff180 - sdk_evidence.BASE)
            # Synthetic R5900 lq $2, 0($4); jr $31; nop. This is not game code.
            struct.pack_into('<3I', flat, sdk_evidence.START - sdk_evidence.BASE, 0x78820000, 0x03e00008, 0)
            image, symbols = out / 'synthetic.bin', out / 'symbols.txt'
            image.write_bytes(flat)
            symbols.write_text(f'SyntheticOnly = {sdk_evidence.START:#x}; // type:func\n')
            sdk_evidence.disassemble(image, out / 'asm', symbols)
            assembly = '\n'.join(p.read_text() for p in (out / 'asm').rglob('*.s'))
            self.assertIn('glabel SyntheticOnly', assembly)
            self.assertRegex(assembly, r'lq\s+\$2, 0x0\(\$4\)')
            self.assertRegex(assembly, r'jr\s+\$31')
            self.assertIn('endlabel SyntheticOnly', assembly)


class LinkTests(unittest.TestCase):
    def test_missing_strict_flag_clears_stale_library_and_does_not_claim_resolution(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / 'libtenkaichi_mobile.so').write_bytes(b'stale')
            (out / 'link.rsp').write_text('-shared')
            report = dict(requested=116, compiled=116, failed=[], linked=True)
            code = strict_link.link(out, out, report)
            self.assertNotEqual(code, 0)
            self.assertFalse((out / 'libtenkaichi_mobile.so').exists())
            self.assertFalse(report['linked'])
            self.assertFalse(report['sdk_comparison']['available'])
            self.assertEqual(report['sdk_comparison']['resolved'], [])
            self.assertIn('--no-undefined', report['link_driver_error'])


if __name__ == '__main__':
    unittest.main()
