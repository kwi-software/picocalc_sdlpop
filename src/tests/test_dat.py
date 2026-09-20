#!/usr/bin/env python3
"""DAT bounds/codec tests; optional independent comparison with SDLPoP PNGs."""
import importlib.util, struct, sys, unittest
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('pack_dat',root/'src/tools/pack_dat.py')
pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)
class DATTests(unittest.TestCase):
    def test_container(self):
        blob=struct.pack('<IH',10,10)+b'\x00abc'+struct.pack('<HHIH',1,42,6,3)
        self.assertEqual(pack.resources(blob),{42:b'abc'})
        for bad in (b'',blob[:8],blob[:-1],struct.pack('<IH',100,10)+blob[6:]):
            with self.assertRaises(ValueError):pack.resources(bad)
    def test_raw_packed_and_odd_width(self):
        raw=struct.pack('<HHH',2,3,0x3000)+bytes([0x12,0x30,0x45,0x60])
        self.assertEqual(pack.decode(raw),(3,2,bytes([1,2,3,4,5,6])))
        with self.assertRaises(ValueError):pack.decode(raw[:-1])
    def test_rle_axes(self):
        for method,pixels in ((1,[1,2,3,4]),(2,[1,3,2,4])):
            raw=struct.pack('<HHH',2,2,0x7000|(method<<8))+bytes([3,1,2,3,4])
            self.assertEqual(pack.decode(raw),(2,2,bytes(pixels)))
        raw=struct.pack('<HHH',2,2,0x7100)+bytes([252,7])
        self.assertEqual(pack.decode(raw),(2,2,b'\x07'*4))
    def test_lzg_reference(self):
        # One literal followed by an overlapping 5-byte window copy.
        raw=struct.pack('<HHH',2,3,0x7300)+bytes([1,7,0x0b,0xbe])
        self.assertEqual(pack.decode(raw),(3,2,b'\x07'*6))
        with self.assertRaises(ValueError):pack.decode(raw[:-1])
    def test_originals(self):
        for group in pack.GROUPS:
            self.assertTrue(pack.resources((root/'PrinceFiles'/f'{group}.DAT').read_bytes()))
if __name__=='__main__':
    unittest.main()
