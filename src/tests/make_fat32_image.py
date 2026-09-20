#!/usr/bin/env python3
"""Create a sparse FAT32 test disk, optionally partitioned or with a full FAT."""
import struct,sys
from pathlib import Path
path=Path(sys.argv[1]);base=2048 if 'mbr' in sys.argv[2:] else 0
reserved,fats,fat_sectors,clusters=32,2,513,65536
data_start=reserved+fats*fat_sectors;total=data_start+clusters
with path.open('wb') as f:
    f.truncate((base+total)*512)
    if base:
        m=bytearray(512);m[450]=0x0c;struct.pack_into('<II',m,454,base,total);m[510:]=b'\x55\xaa';f.write(m)
    boot=bytearray(512);boot[:3]=b'\xeb\x58\x90';boot[3:11]=b'PRINCE  '
    struct.pack_into('<HBHBHHBHHHII',boot,11,512,1,reserved,fats,0,0,0xf8,0,63,255,base,total)
    struct.pack_into('<IHHIHH',boot,36,fat_sectors,0,0,2,1,6);boot[66]=0x29;boot[82:90]=b'FAT32   ';boot[510:]=b'\x55\xaa'
    # Valid VBR code bytes at the MBR-table offset must not imply an MBR.
    boot[450]=0x7f
    f.seek(base*512);f.write(boot)
    info=bytearray(512);struct.pack_into('<I',info,0,0x41615252);struct.pack_into('<III',info,484,0x61417272,clusters-1,3);struct.pack_into('<I',info,508,0xaa550000)
    f.seek((base+1)*512);f.write(info)
    fat=bytearray(fat_sectors*512)
    if 'full' in sys.argv[2:]:
        for i in range(clusters+2):struct.pack_into('<I',fat,4*i,0x0fffffff)
    struct.pack_into('<III',fat,0,0x0ffffff8,0xffffffff,0x0fffffff)
    for i in range(fats):f.seek((base+reserved+i*fat_sectors)*512);f.write(fat)
    # Leave exactly one free directory slot: creating Prince must cross a cluster boundary.
    root=bytearray(512)
    for i in range(15):
        root[32*i:32*i+11]=f'FILL{i:04d}BIN'.encode();root[32*i+11]=0x20
    f.seek((base+data_start)*512);f.write(root)
