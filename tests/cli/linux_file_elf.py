"""Opt-in synthetic file fixtures; native oracle uses a private temporary file."""
import pathlib, platform, subprocess, sys, tempfile
cli, elf_asm, elf_c = sys.argv[1:]
sample=b'Nightfall synthetic data.\n'
fixtures=((elf_asm,b'Nightfallsynthetic'),(elf_c,sample))
native_ok=sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64')
with tempfile.TemporaryDirectory(prefix='nightfall-file-') as directory:
    file=pathlib.Path(directory)/'fixture'
    file.write_bytes(sample)
    for elf,expected in fixtures:
        guest=subprocess.run([cli,'--run-linux-elf-fixture',elf,'500','/test/input'],capture_output=True)
        assert (guest.returncode,guest.stdout)==(0,expected), (elf,guest.returncode,guest.stdout,guest.stderr)
        no_fixture=subprocess.run([cli,'--run-linux-elf',elf,'500','/test/input'],capture_output=True)
        assert no_fixture.returncode!=0 and no_fixture.stdout==b''
        if native_ok:
            native=subprocess.run([elf,str(file)],capture_output=True)
            assert (native.returncode,native.stdout)==(guest.returncode,guest.stdout)
print('original file ELFs matched expected output' + (' and native temporary-file oracle' if native_ok else ''))
