"""Original shared-object fixture: native dynamic loader vs guest self-linking."""
import platform, subprocess,sys
cli,fixture=sys.argv[1:]
guest=subprocess.run([cli,'--run-linux-static-pie',fixture,'100'],capture_output=True,timeout=10)
assert guest.returncode==42 and guest.stdout==b'',(guest.returncode,guest.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
 # Isolated child loads only our authored library and calls its compute function.
 code='import ctypes,sys; f=ctypes.CDLL(sys.argv[1]).compute; f.restype=ctypes.c_int; sys.exit(f())'
 host=subprocess.run([sys.executable,'-c',code,fixture],capture_output=True,timeout=10)
 assert (host.returncode,host.stdout)==(guest.returncode,guest.stdout),(host.returncode,host.stderr)
print('self-contained symbol relocations matched original fixture expectation and native dlopen where available')
