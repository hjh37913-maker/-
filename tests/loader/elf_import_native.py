"""Compare explicitly bound authored guest import with native ELF linking."""
import platform,subprocess,sys
runner,consumer,provider=sys.argv[1:]
guest=subprocess.run([runner,consumer],capture_output=True,timeout=10)
assert guest.returncode==0,(guest.returncode,guest.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
 code='import ctypes,sys; ctypes.CDLL(sys.argv[1],mode=ctypes.RTLD_GLOBAL); f=ctypes.CDLL(sys.argv[2]).compute; f.restype=ctypes.c_int; sys.exit(f())'
 native=subprocess.run([sys.executable,'-c',code,provider,consumer],capture_output=True,timeout=10)
 assert native.returncode==42,(native.returncode,native.stderr)
print('authored guest import execution returned 42'+(' as did native dynamic linker' if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64') else ''))
