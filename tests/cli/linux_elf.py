"""Compare original, native-safe Linux ELF fixtures with the limited test runtime."""
import pathlib
import platform
import subprocess
import sys

cli, hello, args, synthetic, echo, heap, writev, readv, multiply, divide, divide_fault, carry, conditions, strings, compiled_hash, compiled_vector, mmap_elf, compiled_alloc = sys.argv[1:]
def guest(file, budget='100', *argv):
    return subprocess.run([cli, '--run-linux-elf', file, budget, *argv], capture_output=True)
q=guest(hello)
assert q.returncode==0 and q.stdout==b'Hello from an original test program!\n', (q.returncode,q.stdout,q.stderr)
assert b'PS5 software and GTA VI are not supported' in q.stderr
q=guest(args,'100','hello')
assert q.returncode==7 and q.stdout==b'hello', (q.returncode,q.stdout,q.stderr)
q=guest(args)
assert q.returncode==9 and not q.stdout, (q.returncode,q.stdout,q.stderr)
# stdin is opted in explicitly, bounded and captured before guest execution.
for sample in (b'',b'Input is real.\n',bytes(range(64)),b'\xff\x00\r\n'):
    interpreted=subprocess.run([cli,'--run-linux-elf-stdin',echo,'100'],input=sample,capture_output=True)
    assert interpreted.returncode==0 and interpreted.stdout==sample[:32], (interpreted.returncode,interpreted.stdout,interpreted.stderr)
    if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
        native=subprocess.run([echo],input=sample,capture_output=True)
        assert (interpreted.returncode,interpreted.stdout)==(native.returncode,native.stdout)
oversized=subprocess.run([cli,'--run-linux-elf-stdin',echo,'100'],input=b'x'*65537,capture_output=True)
assert oversized.returncode==1 and b'exceeds 65536 bytes' in oversized.stderr and not oversized.stdout
q=guest(writev)
assert q.returncode==0 and q.stdout==b'Hello, vector!\n', (q.returncode,q.stdout,q.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    native=subprocess.run([writev],capture_output=True)
    assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
for sample in (b'',b'abcde',b'xy',bytes(range(50))):
    interpreted=subprocess.run([cli,'--run-linux-elf-stdin',readv,'100'],input=sample,capture_output=True)
    expected=(sample[:5]+b'\x00'*5)[:5]
    assert interpreted.returncode==0 and interpreted.stdout==expected, (interpreted.returncode,interpreted.stdout,interpreted.stderr)
    if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
        native=subprocess.run([readv],input=sample,capture_output=True)
        assert (interpreted.returncode,interpreted.stdout)==(native.returncode,native.stdout)
q=guest(heap)
assert q.returncode==0 and q.stdout==b'OK\n', (q.returncode,q.stdout,q.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    native=subprocess.run([heap],capture_output=True)
    assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
q=guest(multiply)
assert q.returncode==0 and q.stdout==b'Multiply OK\n', (q.returncode,q.stdout,q.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    native=subprocess.run([multiply],capture_output=True)
    assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
q=guest(mmap_elf)
assert q.returncode==0 and q.stdout==b'M', (q.returncode,q.stdout,q.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    native=subprocess.run([mmap_elf],capture_output=True)
    assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
# GCC-compiled SSE2 integer vector fixture; short input exits without output.
for size in range(65):
    sample=bytes((i*43+size*7)&255 for i in range(size))
    q=subprocess.run([cli,'--run-linux-elf-stdin',compiled_vector,'1000'],input=sample,capture_output=True)
    if size<32: expected_code,expected=1,b''
    else:
        expected_code=0; expected=b''
        for lane in range(4):
            a=int.from_bytes(sample[lane*4:lane*4+4],'little')
            b=int.from_bytes(sample[16+lane*4:20+lane*4],'little')
            expected+=(((a+b)&0xffffffff)^b).to_bytes(4,'little')
    assert (q.returncode,q.stdout)==(expected_code,expected), (size,q.returncode,q.stdout,q.stderr)
    if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
        native=subprocess.run([compiled_vector],input=sample,capture_output=True)
        assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
# A compiler-generated, freestanding C ELF combines read, byte loads,
# arithmetic, IMUL, shifts, indexed lookup, stores, and write.
for size in range(71):
    sample=bytes((i*73+size*11)&255 for i in range(size))
    h=2166136261
    for byte in sample[:64]: h=((h^byte)*16777619)&0xffffffff
    expected=f'{h:08x}\n'.encode()
    for binary in (compiled_hash,compiled_alloc):
        q=subprocess.run([cli,'--run-linux-elf-stdin',binary,'2000'],input=sample,capture_output=True)
        assert q.returncode==0 and q.stdout==expected, (binary,size,q.returncode,q.stdout,q.stderr)
        if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
            native=subprocess.run([binary],input=sample,capture_output=True)
            assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
q=guest(strings)
assert q.returncode==0 and q.stdout==b'String OK\n', (q.returncode,q.stdout,q.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    native=subprocess.run([strings],capture_output=True)
    assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
# A small budget may stop a repeated instruction before it finishes.
q=guest(strings,'8')
assert q.returncode==1 and b'budget_exhausted' in q.stderr and not q.stdout
q=guest(conditions)
assert q.returncode==0 and q.stdout==b'Conditions OK\n', (q.returncode,q.stdout,q.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    native=subprocess.run([conditions],capture_output=True)
    assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
q=guest(carry)
assert q.returncode==0 and q.stdout==b'Carry OK\n', (q.returncode,q.stdout,q.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    native=subprocess.run([carry],capture_output=True)
    assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
q=guest(divide)
assert q.returncode==0 and q.stdout==b'Divide OK\n', (q.returncode,q.stdout,q.stderr)
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    native=subprocess.run([divide],capture_output=True)
    assert (q.returncode,q.stdout)==(native.returncode,native.stdout)
q=guest(divide_fault)
assert q.returncode==1 and b'did not exit' in q.stderr and b'divide_error' in q.stderr and not q.stdout
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    import resource, signal
    def no_core(): resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    native=subprocess.run([divide_fault],capture_output=True,preexec_fn=no_core)
    assert native.returncode==-signal.SIGFPE and not native.stdout
q=guest(hello,'1')
assert q.returncode==1 and b'did not exit' in q.stderr
q=guest(synthetic)
assert q.returncode==1 and b'did not exit' in q.stderr # synthetic HLT isn't process exit
q=guest(hello,'0')
assert q.returncode==2 and b'Budget' in q.stderr
q=guest(str(pathlib.Path(hello).with_name('nightfall-missing-test.elf')))
assert q.returncode==1
if sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64','amd64'):
    for path,argv in [(hello,[]),(args,['hello']),(args,[])]:
        native=subprocess.run([path,*argv],capture_output=True)
        interpreted=guest(path,'100',*argv)
        assert (interpreted.returncode,interpreted.stdout)==(native.returncode,native.stdout)
    print('guest stdout and exit matched native execution of original Linux test fixtures')
else:
    print('guest test expectations passed; native Linux comparison skipped on this host')
