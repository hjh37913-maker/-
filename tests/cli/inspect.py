"""Bounded ELF metadata inspection: synthetic files, no guest execution."""
import pathlib
import struct
import subprocess
import sys
import tempfile

cli = sys.argv[1]
def run(*args):
    return subprocess.run([cli, '--inspect-elf', *map(str,args)], capture_output=True, text=True)
def fixture():
    data=bytearray(0x120)
    data[:7]=b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHIQQQIHHHHHH',data,16,2,62,1,0x1000,64,0,0,64,56,1,0,0,0)
    struct.pack_into('<IIQQQQQQ',data,64,1,5,0x100,0x1000,0,0x20,0x30,0x100)
    data[0x100]=0xf4
    return data
with tempfile.TemporaryDirectory() as tmp:
    path=pathlib.Path(tmp)/'test.elf'
    def check(data,status=0,message=''):
        path.write_bytes(data); before=path.read_bytes(); p=run(path)
        assert p.returncode==status, (p.returncode,p.stdout,p.stderr)
        assert message in (p.stdout if status==0 else p.stderr), (message,p.stdout,p.stderr)
        assert path.read_bytes()==before, 'inspection modified source file'
        return p
    p=check(fixture(),message='type=2 machine=62 OSABI=0 ABI version=0 entry=0x1000')
    assert 'type=1 flags=5 offset=256 vaddr=0x1000 filesz=32 memsz=48 align=256' in p.stdout
    assert 'no execution or decryption' in p.stdout and 'not a compatibility result' in p.stdout
    assert 'halted' not in p.stdout
    for n in range(64):
        data=fixture()[:n]
        status='not_elf' if n<4 else 'malformed'
        check(data,1,status)
    check(b'notelf',1,'not_elf')
    for pos,val in [(4,1),(5,2),(6,2)]:
        data=fixture(); data[pos]=val; check(data,1,'unsupported_format')
    for pos,fmt,value in [(20,'<I',2),(52,'<H',32),(54,'<H',0),(32,'<Q',2**64-1),
                          (96,'<Q',49),(72,'<Q',2**64-1),(112,'<Q',3)]:
        data=fixture(); struct.pack_into(fmt,data,pos,value); check(data,1,'malformed')
    for count in (129,65535):
        data=fixture(); struct.pack_into('<H',data,56,count); check(data,1,'unsupported_format')
    data=fixture(); struct.pack_into('<HH',data,16,3,183); struct.pack_into('<I',data,64,3)
    check(data,message='type=3 machine=183') # metadata only, not a compatibility verdict
    data=fixture(); struct.pack_into('<H',data,56,0); check(data,message='no execution or decryption')
    bad=run(path,'extra'); assert bad.returncode==2
    missing=run(pathlib.Path(tmp)/'missing'); assert missing.returncode==1 and 'read_error' in missing.stderr
    # Huge sparse synthetic file: bounded inspector reads only header/table, not payload.
    path.write_bytes(fixture())
    with path.open('r+b') as f: f.truncate(16*1024*1024)
    p=run(path); assert p.returncode==0 and 'File size=16777216' in p.stdout
print('ELF inspection limits, metadata, rejections, non-execution and source immutability passed')
