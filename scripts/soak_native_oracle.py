#!/usr/bin/env python3
"""Repeat a frozen native oracle executable with independently varying seeds.

Only the project's authored seeded oracle binaries are intended for this tool.
Pass the per-batch count printed by that executable and keep it unchanged.
"""
import argparse,datetime,hashlib,json,pathlib,subprocess,time
p=argparse.ArgumentParser()
p.add_argument('--binary',required=True,type=pathlib.Path)
p.add_argument('--seconds',type=int,required=True)
p.add_argument('--comparisons',type=int,required=True)
p.add_argument('--seed',type=int,default=1397411)
p.add_argument('--output',required=True,type=pathlib.Path)
a=p.parse_args()
if not 1<=a.seconds<=3600 or a.comparisons<1:p.error('invalid duration or comparison count')
binary=a.binary.resolve();digest=lambda:hashlib.sha256(binary.read_bytes()).hexdigest();sha=digest();start=time.monotonic();batches=0
while time.monotonic()-start<a.seconds:
 seed=(a.seed+batches*0x9e3779b97f4a7c15)&((1<<64)-1)
 result=subprocess.run([str(binary),str(seed)],capture_output=True,text=True)
 if result.returncode or not result.stdout.startswith(str(a.comparisons)+' '):
  print(result.stdout,result.stderr,flush=True);raise SystemExit(result.returncode or 1)
 batches+=1
 if batches%100==0:print(f'batches={batches} comparisons={batches*a.comparisons} seconds={time.monotonic()-start:.1f}',flush=True)
if digest()!=sha:raise SystemExit('binary changed during soak: results discarded')
report={'binary_sha256':sha,'seed_start':a.seed,'batches':batches,'comparisons_per_batch':a.comparisons,'total_comparisons':batches*a.comparisons,'elapsed_seconds':round(time.monotonic()-start,3),'finished_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'oracle_output':result.stdout.strip()}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
