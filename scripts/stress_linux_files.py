#!/usr/bin/env python3
"""Compare our authored C ELF with Python and native private test files."""
import argparse, pathlib, platform, random, subprocess, sys, tempfile
p=argparse.ArgumentParser()
p.add_argument('--runner',type=pathlib.Path,required=True)
p.add_argument('--cases',type=int,default=128)
p.add_argument('--seed',type=int,default=732105)
a=p.parse_args()
if not 1<=a.cases<=10000:p.error('cases must be 1..10000')
root=pathlib.Path(__file__).resolve().parents[1]
elf=root/'examples/linux-file-c.elf';runner=a.runner.resolve()
native=sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64')
rng=random.Random(a.seed)
with tempfile.TemporaryDirectory(prefix='nightfall-c-file-') as temp:
 path=pathlib.Path(temp)/'original-data'
 for i in range(a.cases):
  size=i%97;data=bytes(rng.getrandbits(8) for _ in range(size))
  expected=(0,data) if size<=64 else (4,b'')
  guest=subprocess.run([str(runner),str(elf),data.hex()],capture_output=True,timeout=10)
  if (guest.returncode,guest.stdout)!=expected:raise SystemExit(f'guest failed case={i} size={size} result={guest.returncode} stderr={guest.stderr!r}')
  if native:
   path.write_bytes(data);host=subprocess.run([str(elf),str(path)],capture_output=True,timeout=10)
   if (host.returncode,host.stdout)!=expected:raise SystemExit(f'native failed case={i} size={size} result={host.returncode} stderr={host.stderr!r}')
print(f'{a.cases} compiled synthetic-file executions matched Python'+(' and native private-file execution' if native else '; native execution skipped'))
