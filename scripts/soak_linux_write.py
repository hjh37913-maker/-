#!/usr/bin/env python3
"""Stress-test only the limited write ABI against safe native private pipes.

Native test writes deterministic synthetic bytes to a pipe it creates; no guest
file path, descriptor, or untrusted host pointer is forwarded. This is not a
PS5 or complete Linux compatibility test.
"""
import argparse
import hashlib
import json
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--binary',type=Path,required=True)
p.add_argument('--until',type=float,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--iterations',type=int,default=65536)
a=p.parse_args()
if not 1<=a.iterations<=1000000: p.error('iterations must be 1..1000000')
binary=a.binary.resolve()
binary_hash=hashlib.sha256(binary.read_bytes()).hexdigest()
start=time.time(); seed=0x19aacc7733dd11ee; batches=cases=0; error=''
while time.time()<a.until:
    seed=(seed+0x9e3779b97f4a7c15)&((1<<64)-1)
    try: result=subprocess.run([str(binary),str(a.iterations),str(seed)],capture_output=True,text=True,timeout=30)
    except subprocess.TimeoutExpired: error=f'batch timeout seed={seed}'; break
    if result.returncode or not result.stdout.startswith(f'{a.iterations} private-pipe native write comparisons passed'):
        error=f'seed={seed} exit={result.returncode}: {result.stdout} {result.stderr}'; break
    batches+=1; cases+=a.iterations
    if batches%500==0: print(f'{batches} batches, {cases} comparisons, {time.time()-start:.1f}s',flush=True)
end=time.time()
report={'success':not bool(error),'started_utc':datetime.fromtimestamp(start,timezone.utc).isoformat(),
        'finished_utc':datetime.fromtimestamp(end,timezone.utc).isoformat(),'elapsed_seconds':round(end-start,3),
        'binary_sha256':binary_hash,'batches':batches,'comparisons':cases,'iterations_per_batch':a.iterations,
        'seed_formula':'0x19aacc7733dd11ee + batch * 0x9e3779b97f4a7c15 mod 2^64',
        'oracle':'native Linux write into a private pipe, synthetic inputs; project output limits tested separately',
        'scope':'test ABI write only; not PS5, full Linux, file access, networking, or a sandbox', 'error':error}
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report),flush=True)
sys.exit(1 if error else 0)
