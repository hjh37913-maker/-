"""Cross-check equivalent authored guest logic and native Linux dynamic linking.

The native ELF is a different file/layout; this does not validate SCE format
or any console behavior. Neither fixture contains game or protected data.
"""
import platform,subprocess,sys
runner,provider,consumer=sys.argv[1:]
guest=subprocess.run([runner],capture_output=True,timeout=10)
assert guest.returncode==0,(guest.returncode,guest.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
 code='import ctypes,sys; ctypes.CDLL(sys.argv[1],mode=ctypes.RTLD_GLOBAL); f=ctypes.CDLL(sys.argv[2]).compute; f.restype=ctypes.c_int; sys.exit(f())'
 native=subprocess.run([sys.executable,'-c',code,provider,consumer],capture_output=True,timeout=10)
 assert native.returncode==42,(native.returncode,native.stderr)
print('synthetic SCE-style guest test and equivalent authored native linking case returned 42; formats differ')
