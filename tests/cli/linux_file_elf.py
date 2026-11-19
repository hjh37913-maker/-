"""Opt-in synthetic file fixture; native oracle uses a private temporary file."""
import pathlib, platform, subprocess, sys, tempfile
cli, elf = sys.argv[1:]
expected=b'Nightfallsynthetic'
guest=subprocess.run([cli,'--run-linux-elf-fixture',elf,'500','/test/input'],capture_output=True)
assert (guest.returncode,guest.stdout)==(0,expected), (guest.returncode,guest.stdout,guest.stderr)
no_fixture=subprocess.run([cli,'--run-linux-elf',elf,'500','/test/input'],capture_output=True)
assert no_fixture.returncode==1 and no_fixture.stdout==b''
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    with tempfile.TemporaryDirectory(prefix='nightfall-file-') as directory:
        file=pathlib.Path(directory)/'fixture'
        file.write_bytes(b'Nightfall synthetic data.\n')
        native=subprocess.run([elf,str(file)],capture_output=True)
        assert (native.returncode,native.stdout)==(guest.returncode,guest.stdout)
print('original file ELF matched expected output' + (' and native temporary-file oracle' if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64') else ''))
