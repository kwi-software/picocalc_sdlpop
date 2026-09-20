#!/usr/bin/env python3
"""Check linked RAM parking code and explicit persistent-sector reset in UF2."""
import re, struct, subprocess, sys
from pathlib import Path
elf=Path(sys.argv[1]);uf2=elf.with_suffix('.uf2')
nm=subprocess.check_output(['arm-none-eabi-nm','-S',str(elf)],text=True)
address=int(re.search(r'^([0-9a-f]+) [0-9a-f]+ t flash_pause$',nm,re.M)[1],16)
assert 0x20000000<=address<0x20082000, 'Core 1 flash wait must execute from RAM'
code=subprocess.check_output(['arm-none-eabi-objdump','-d','--disassemble=flash_pause',str(elf)],text=True)
assert not re.search(r'\bblx?\s',code), 'Review any out-of-line call from the flash parking routine'
assert '\twfe' in code
end=int(re.search(r'^([0-9a-f]+) [A-Za-z] __flash_binary_end$',nm,re.M)[1],16)
assert end<=0x101fe000, 'Firmware overlaps persistent data'
raw=uf2.read_bytes();assert len(raw)%512==0
reset={}
for pos in range(0,len(raw),512):
    block=raw[pos:pos+512]
    magic0,magic1,flags,target,size,number,total,family=struct.unpack_from('<8I',block)
    assert (magic0,magic1,struct.unpack_from('<I',block,508)[0])==(0x0a324655,0x9e5d5157,0x0ab16f30)
    if 0x101fe000<=target<0x10200000:
        assert size==256 and target%256==0 and block[32:32+size]==b'\xff'*size
        reset[target]=size
assert sorted(reset)==list(range(0x101fe000,0x10200000,256)), 'UF2 must explicitly reset BOTH sectors'
print(f'PASS: flash wait at {address:#x}, no external calls; firmware ends {end:#x}; UF2 contains all 8192 reset bytes')
