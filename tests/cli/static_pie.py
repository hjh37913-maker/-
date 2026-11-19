"""Execute only independently authored PIE fixtures; not a PS5 executable."""
import platform,subprocess,sys
cli,guest,native=sys.argv[1:]
p=subprocess.run([cli,'--run-linux-static-pie',guest,'100'],capture_output=True,timeout=10)
assert p.returncode==42 and p.stdout==b'',(p.returncode,p.stdout,p.stderr)
assert b'PS5 software and GTA VI are not supported' in p.stderr
p2=subprocess.run([cli,'--run-linux-elf',guest,'100'],capture_output=True,timeout=10)
assert p2.returncode!=42
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
 # This separate authored binary requests the native Linux dynamic linker.
 # Never directly execute the no-interpreter guest fixture on the host.
 host=subprocess.run([native],capture_output=True,timeout=10)
 assert (host.returncode,host.stdout)==(p.returncode,p.stdout),(host.returncode,host.stderr)
print('original PIE fixture exit matched expected'+(' and native dynamic-linker fixture' if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64') else ''))
