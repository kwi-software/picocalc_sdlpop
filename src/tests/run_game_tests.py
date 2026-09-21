#!/usr/bin/env python3
"""Build the actual game with sanitizers; exercise startup, input and level transitions."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[2]
env=dict(os.environ,GAME_SANITIZE='1',ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
subprocess.run(['sh','src/game/build_host.sh'],cwd=root,env=env,check=True)
out=root/'build-host-game/captures';out.mkdir(exist_ok=True)
store=out/'flash.bin'
store.unlink(missing_ok=True)
cases=[
 ('title',[],{'POP_FRAMES':'400'}),
 ('start',[],{'POP_SCENARIO':'start','POP_LEVEL_TEST':'1','POP_FRAMES':'100'}),
 ('movement',['megahit','1'],{'POP_INPUT':'1','POP_FRAMES':'150'}),
 ('aspect',[],{'POP_SCENARIO':'aspect','POP_FRAMES':'400'}),
 ('filter',[],{'POP_SCENARIO':'filter','POP_FRAMES':'400'}),
 ('health',['megahit','1'],{'POP_SCENARIO':'health','POP_FRAMES':'100'}),
 ('restart',['megahit','1'],{'POP_SCENARIO':'restart','POP_FRAMES':'300'}),
 ('transitions',['megahit','1'],{'POP_SCENARIO':'levels','POP_LEVEL_TEST':'14','POP_FRAMES':'80'}),
 ('cheats',[],{'POP_SCENARIO':'cheats','POP_LEVEL_TEST':'1','POP_FRAMES':'100'}),
 ('save',['megahit','1'],{'POP_SCENARIO':'save','POP_FRAMES':'100','PRINCE_STORE_PATH':str(store)}),
 ('load',[],{'POP_SCENARIO':'load','POP_LEVEL_TEST':'6','POP_FRAMES':'100','PRINCE_STORE_PATH':str(store)}),
 ('attract',[],{'POP_FRAMES':'8000'}),
]
for name,args,opts in cases:
    run_env=dict(env,**opts,POP_CAPTURE=str(out/(name+'.ppm')))
    p=subprocess.run(['./build-host-game/prince',*args],cwd=root,env=run_env,capture_output=True,text=True,timeout=60)
    log=p.stdout+p.stderr;(out/(name+'.log')).write_text(log)
    if p.returncode:
        print(log[-3000:]);raise SystemExit(f'{name} failed ({p.returncode})')
    if name=='transitions':
        for level in range(1,15):assert f'VISITED LEVEL {level}\n' in log
    assert 'audio_errors=0' in log and 'runtime error' not in log
    print(name+': '+log.strip().splitlines()[-1],flush=True)
