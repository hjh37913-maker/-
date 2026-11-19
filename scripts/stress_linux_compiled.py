#!/usr/bin/env python3
"""Reproducible independent checks for original Linux test ELFs, not games.

Optional Linux native comparison launches only repository-authored fixtures;
guest paths, descriptors and pointers are never passed to the OS as syscalls.
"""
import argparse
import platform
import random
import subprocess
import sys
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--cli', required=True, type=Path)
p.add_argument('--cases', type=int, default=512)
p.add_argument('--seed', type=int, default=441147)
a = p.parse_args()
if not 1 <= a.cases <= 10000:
    p.error('cases must be 1..10000')
root = Path(__file__).resolve().parents[1]
cli = a.cli.resolve()
binaries = [root/'examples'/x for x in ('linux-hash.elf', 'linux-allocate.elf', 'linux-vector.elf')]
native_ok = sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64', 'amd64')
rng = random.Random(a.seed)
checks = 0
for case in range(a.cases):
    size = rng.randrange(97)
    sample = bytes(rng.getrandbits(8) for _ in range(size))
    value = 2166136261
    for byte in sample[:64]:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    hash_result = f'{value:08x}\n'.encode()
    vector_result = b''
    if size >= 32:
        for lane in range(4):
            left = int.from_bytes(sample[4*lane:4*lane+4], 'little')
            right = int.from_bytes(sample[16+4*lane:20+4*lane], 'little')
            vector_result += (((left + right) & 0xffffffff) ^ right).to_bytes(4, 'little')
    for index, binary in enumerate(binaries):
        expected = (1, b'') if index == 2 and size < 32 else (0, vector_result if index == 2 else hash_result)
        result = subprocess.run([str(cli), '--run-linux-elf-stdin', str(binary), '2000'], input=sample, capture_output=True, timeout=10)
        if (result.returncode, result.stdout) != expected:
            raise SystemExit(f'guest mismatch case={case} size={size} fixture={binary.name} stderr={result.stderr!r}')
        if native_ok:
            host = subprocess.run([str(binary)], input=sample, capture_output=True, timeout=10)
            if (host.returncode, host.stdout) != expected:
                raise SystemExit(f'native mismatch case={case} size={size} fixture={binary.name}')
        checks += 1
print(f'{checks} compiled-fixture executions matched independent reference' + (' and native execution' if native_ok else '; native Linux comparison skipped'))
