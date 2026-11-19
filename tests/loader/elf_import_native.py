"""Compare explicitly bound authored guest import with native ELF linking."""
import pathlib,platform,subprocess,sys,tempfile
runner,consumer,provider,cli=sys.argv[1:]
guest=subprocess.run([runner,consumer],capture_output=True,timeout=10)
assert guest.returncode==0,(guest.returncode,guest.stderr)
failed=subprocess.run([cli,'--run-linux-static-pie',consumer,'100'],capture_output=True,timeout=10)
assert failed.returncode!=0 and b'unresolved guest symbol: host_export' in failed.stderr,failed.stderr
# Escape control bytes in displayed guest-provided symbol names.
with tempfile.TemporaryDirectory() as d:
 data=pathlib.Path(consumer).read_bytes().replace(b'host_export\0',b'host\x1bexport\0',1)
 path=pathlib.Path(d)/'mutated.elf';path.write_bytes(data)
 bad=subprocess.run([cli,'--run-linux-static-pie',str(path),'100'],capture_output=True,timeout=10)
 assert bad.returncode!=0 and b'host\\x1bexport' in bad.stderr and b'\x1b' not in bad.stderr,bad.stderr
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
 code='import ctypes,sys; ctypes.CDLL(sys.argv[1],mode=ctypes.RTLD_GLOBAL); f=ctypes.CDLL(sys.argv[2]).compute; f.restype=ctypes.c_int; sys.exit(f())'
 native=subprocess.run([sys.executable,'-c',code,provider,consumer],capture_output=True,timeout=10)
 assert native.returncode==42,(native.returncode,native.stderr)
print('authored guest import execution returned 42'+(' as did native dynamic linker' if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64') else ''))
