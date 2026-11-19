#!/usr/bin/env python3
"""Run reproducible, independent-seed native shift differential batches.

The C++ executable compares interpreter results and defined flags to equivalent
native register operations. It does not independently validate guest address
calculation, memory permissions, or the original immediate opcode encoding.
"""
import argparse
import json
import hashlib
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--until', type=float, required=True, help='Unix time at which to stop starting batches')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--iterations', type=int, default=65536)
args = parser.parse_args()
if not 1 <= args.iterations <= 10000000:
    parser.error('iterations must be between 1 and 10000000')
start = time.time()
seed = 0x849cdef039217aad
batches = cases = 0
success = True
error = ''
while time.time() < args.until:
    seed = (seed + 0x9e3779b97f4a7c15) & ((1 << 64)-1)
    run = subprocess.run([str(args.binary.resolve()), str(args.iterations), str(seed)], text=True, capture_output=True)
    if run.returncode:
        success = False
        error = f'seed={seed} exit={run.returncode}: {run.stderr} {run.stdout}'
        break
    expected = args.iterations * 36
    if not run.stdout.startswith(f'{expected} shifts '):
        success = False
        error = f'unexpected output with seed={seed}: {run.stdout}'
        break
    batches += 1
    cases += expected
    if batches % 50 == 0:
        print(f'{batches} batches, {cases} comparisons, {time.time()-start:.1f}s', flush=True)
end = time.time()
report = {'success':success,'started_utc':datetime.fromtimestamp(start,timezone.utc).isoformat(),
          'finished_utc':datetime.fromtimestamp(end,timezone.utc).isoformat(), 'elapsed_seconds':round(end-start,3),
          'batches':batches,'comparisons':cases,'iterations_per_form':args.iterations,
          'binary_sha256':hashlib.sha256(args.binary.read_bytes()).hexdigest(),
          'first_seed_formula':'0x849cdef039217aad + batch * 0x9e3779b97f4a7c15 mod 2^64',
          'oracle':'native register shifts with equivalent counts, not identical immediate encoding or guest addressing',
          'error':error}
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report),flush=True)
sys.exit(0 if success else 1)
