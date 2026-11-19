#!/usr/bin/env python3
"""Repeat independent original-file fixture tests with deterministic seeds.

Use a frozen runner and don't change examples/linux-file-c.elf while this runs.
The optional JSON report records input identities, elapsed time, and scope.
"""
import argparse,datetime,hashlib,json,pathlib,subprocess,sys,time
p=argparse.ArgumentParser()
p.add_argument('--runner',type=pathlib.Path,required=True)
p.add_argument('--seconds',type=int,default=60)
p.add_argument('--batch',type=int,default=5000)
p.add_argument('--seed',type=int,default=381000)
p.add_argument('--output',type=pathlib.Path,required=True)
a=p.parse_args()
if not 1<=a.seconds<=3600 or not 1<=a.batch<=10000:p.error('seconds 1..3600; batch 1..10000')
root=pathlib.Path(__file__).resolve().parents[1]
runner=a.runner.resolve();elf=root/'examples/linux-file-c.elf';driver=root/'scripts/stress_linux_files.py'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
identities={'runner_sha256':digest(runner),'fixture_sha256':digest(elf),'driver_sha256':digest(driver)}
start=time.monotonic();batches=0
while time.monotonic()-start<a.seconds:
 result=subprocess.run([sys.executable,str(driver),'--runner',str(runner),'--cases',str(a.batch),'--seed',str(a.seed+batches)],capture_output=True,text=True)
 if result.returncode:print(result.stdout,result.stderr,flush=True);raise SystemExit(result.returncode)
 batches+=1
 print(f'batches={batches} cases={batches*a.batch} seconds={time.monotonic()-start:.1f}',flush=True)
if identities!={'runner_sha256':digest(runner),'fixture_sha256':digest(elf),'driver_sha256':digest(driver)}:raise SystemExit('Input changed during run: results discarded')
report={**identities,'seed_start':a.seed,'batches':batches,'cases_per_batch':a.batch,'guest_executions':batches*a.batch,'elapsed_seconds':round(time.monotonic()-start,3),'finished_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Original compiled file fixture vs Python and, on Linux x86-64, native private temporary files. No games or PS5 software.'}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report),flush=True)
