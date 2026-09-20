#!/usr/bin/env python3
"""Original PoP 1.0/1.1/1.3/1.4 DOS DATs -> immutable C assets. Standard Python only.
The graphics decoder follows SDLPoP's RLE/LZG and packed-pixel format.
"""
import argparse, hashlib, json, struct
from pathlib import Path
GROUPS={'PRINCE':[150,700],'KID':[400],'VDUNGEON':[200,360],
        'VPALACE':[200,360],'GUARD':[750],'GUARD1':[],'GUARD2':[],
        'FAT':[750],'SKEL':[750],'VIZIER':[750],'SHADOW':[750],
        'PV':[800,850,900,950,980],'TITLE':[40,50], 'LEVELS':[],
        'MIDISND1':[],'MIDISND2':[],'DIGISND1':[],'DIGISND2':[],
        'DIGISND3':[],'IBM_SND1':[],'IBM_SND2':[]}
def resources(b):
    if len(b)<8:raise ValueError('DAT header truncated')
    off,size=struct.unpack_from('<IH',b)
    if off<6 or off+size>len(b) or size<2:raise ValueError('DAT table out of bounds')
    count=struct.unpack_from('<H',b,off)[0]
    if 2+8*count>size:raise ValueError('DAT table truncated')
    out={}
    for i in range(count):
        ident,pos,n=struct.unpack_from('<HIH',b,off+2+8*i)
        if pos<6 or pos+1+n>off or ident in out:raise ValueError('Invalid DAT entry')
        out[ident]=b[pos+1:pos+1+n]
    return out

def decode(b):
    if len(b)<6:raise ValueError('Image header truncated')
    h,w,flags=struct.unpack_from('<HHH',b)
    depth=((flags>>12)&7)+1;method=(flags>>8)&15
    if not(0<w<=320 and 0<h<=200) or depth not in (1,2,4,8) or method>4:
        raise ValueError(f'Unsupported image {w}x{h}, depth={depth}, codec={method}')
    stride=(w*depth+7)//8;n=h*stride;pos=6;out=bytearray()
    def read():
        nonlocal pos
        if pos>=len(b):raise ValueError('Truncated image stream')
        v=b[pos];pos+=1;return v
    if method==0:
        if len(b)<6+n:raise ValueError('Truncated raw image')
        out.extend(b[6:6+n])
    elif method in (1,2):
        while len(out)<n:
            c=read()
            if c<128:
                for _ in range(min(c+1,n-len(out))):out.append(read())
            else:
                value=read();out.extend(bytes([value])*min(256-c,n-len(out)))
    else:
        window=bytearray(1024);wp=1024-66;mask=0
        while len(out)<n:
            mask>>=1
            if not(mask&0xff00):mask=read()|0xff00
            if mask&1:
                v=read();window[wp]=v;wp=(wp+1)&1023;out.append(v)
            else:
                info=(read()<<8)|read();src=info&1023;length=(info>>10)+3
                for _ in range(min(length,n-len(out))):
                    v=window[src];src=(src+1)&1023
                    window[wp]=v;wp=(wp+1)&1023;out.append(v)
    if method in (2,4):out=bytearray(out[x*h+y] for y in range(h) for x in range(stride))
    mask=(1<<depth)-1
    pixels=bytes((out[y*stride+x*depth//8]>>(8-depth-(x*depth%8)))&mask for y in range(h) for x in range(w))
    return w,h,pixels

def generate(root,out):
    data={g:(root/(g+'.DAT')).read_bytes() for g in GROUPS}
    parsed={g:resources(b) for g,b in data.items()};entries={}
    for g,headers in GROUPS.items():
        entries[g+'.DAT']=data[g]
        for ident in headers:
            header=parsed['GUARD1' if g=='GUARD' else g][ident]
            if len(header)!=100 or header[3]!=16:raise ValueError(f'{g}/{ident}: unsupported palette')
            pal=[]
            for i in range(16):
                red,green,blue=header[4+3*i:7+3*i]
                if max(red,green,blue)>63:raise ValueError('Invalid VGA palette')
                pal.append(((red<<2>>3)<<11)|((green<<2>>2)<<5)|(blue<<2>>3))
            pal[0]=0
            for image_id in list(range(ident+1,ident+header[0]+1)) + (sorted(k for k in parsed[g] if k>=1200) if ident==200 else []):
                resource=parsed[g].get(image_id)
                if resource is None or resource==b'\0\0':continue # optional missing images supported by SDLPoP
                w,h,pixels=decode(resource)
                if max(pixels,default=0)>=16:raise ValueError('Non-VGA sprite not supported')
                entries[f'{g}/res{image_id}.png']=(w,h,pixels,pal)
    out.mkdir(parents=True,exist_ok=True)
    header=['/* Generated from PrinceFiles; do not edit. */','#pragma once','#include "pop_assets.h"',
            'extern const POP_Asset pop_assets[];','extern const size_t pop_asset_count;',
            '#ifdef PRINCE_ASSETS_IMPLEMENTATION']
    rows=[];payload=0
    def array(typ,name,values):
        header.append(f'static const {typ} {name}[] = {{')
        header.extend('  '+','.join(map(str,values[i:i+24]))+',' for i in range(0,len(values),24))
        header.append('};')
    for i,(name,b) in enumerate(sorted(entries.items())):
        sym=f'asset_{i}'
        if isinstance(b,bytes):
            array('uint8_t',sym,b);payload+=len(b)
            rows.append('{'+json.dumps(name)+',{'+sym+',sizeof '+sym+'},NULL}')
        else:
            w,h,pixels,pal=b;array('uint8_t',sym,pixels);array('uint16_t',sym+'_pal',pal);payload+=len(pixels)+32
            header.append(f'static const PC_Surface {sym}_surface = '+'{'+f'{w},{h},{w},PC_INDEX8,{sym},sizeof {sym},{sym}_pal,16,0,false,'+'{'+f'0,0,{w},{h}'+'}};')
            rows.append('{'+json.dumps(name)+',{NULL,0},&'+sym+'_surface}')
    header.extend(['const POP_Asset pop_assets[]={',',\n'.join(rows),'};',
                   'const size_t pop_asset_count=sizeof pop_assets/sizeof pop_assets[0];','#endif'])
    (out/'game_assets.h').write_text('\n'.join(header)+'\n')
    (out/'game_assets.c').write_text('#define PRINCE_ASSETS_IMPLEMENTATION\n#include "game_assets.h"\n')
    report={'resources':len(entries),'payload_bytes':payload,'inputs':{g+'.DAT':hashlib.sha256(b).hexdigest()for g,b in data.items()}}
    (out/'game_assets.json').write_text(json.dumps(report,indent=2)+'\n');print(f'{len(entries)} resources, {payload} payload bytes')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('data',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
    try:generate(a.data,a.output)
    except (ValueError,KeyError,OSError,struct.error) as e:p.exit(1,f'DAT conversion failed: {e}\n')
