"""Synthetic raw candidate-ELF metadata only; no proprietary input."""
import os,pathlib,struct,subprocess,sys,tempfile
cli=sys.argv[1]
def put(buf,pos,value,width):buf[pos:pos+width]=value.to_bytes(width,'little')
def image():
 b=bytearray(4096);b[:4]=b'\x7fELF';b[4:8]=bytes([2,1,1,9]);put(b,16,0xfe10,2);put(b,18,62,2);put(b,20,1,4);put(b,32,64,8);put(b,52,64,2);put(b,54,56,2);put(b,56,2,2)
 put(b,64,2,4);put(b,72,0x200,8);put(b,96,192,8)
 put(b,120,0x61000000,4);put(b,128,0x400,8);put(b,152,0x400,8)
 for i,(tag,val) in enumerate(((0x61000035,0),(0x61000037,32),(0x61000039,0x100),(0x6100003b,24),(0x6100003f,96),(0x6100002f,0x200),(0x61000031,24),(0x61000033,24),(0x61000029,0x218),(0x6100002d,24),(0x6100002b,7),(0,0))):put(b,0x200+16*i,tag,8);put(b,0x208+16*i,val,8)
 b[0x401:0x401+10]=b'fake#A#B!\0';b[0x40b:0x40b+9]=b'weak\x1bimp\0'
 # null, global undefined, weak undefined with control byte, defined (ignored)
 for i,(name,info,section) in enumerate(((0,0,0),(1,0x12,0),(11,0x20,0),(1,0x12,1))):
  p=0x500+i*24;put(b,p,name,4);b[p+4]=info;put(b,p+6,section,2)
 # Two synthetic relocation records, regular and PLT.
 put(b,0x600,0xdead,8);put(b,0x608,(1<<32)|6,8);put(b,0x610,0,8)
 put(b,0x618,0xbeef,8);put(b,0x620,(2<<32)|7,8);put(b,0x628,4,8)
 return b
def run(b):
 path.write_bytes(b);return subprocess.run([cli,'--inspect-sce-imports',str(path)],capture_output=True,timeout=10)
with tempfile.TemporaryDirectory(prefix='nightfall-sce-meta-') as tmp:
 path=pathlib.Path(tmp)/'authored.elf';base=image();p=run(base)
 assert p.returncode==0 and b'NOT established' in p.stdout and b'entries=2' in p.stdout and b'OS-tagged RELA entries=2' in p.stdout and b'plt type=7 symbol=2 target=0xbeef addend=0x4' in p.stdout and b'name=fake#A#B!' in p.stdout and b'name=weak\\x1bimp' in p.stdout and b'\x1b' not in p.stdout,(p.stdout,p.stderr)
 cases=[]
 cases.append((b'SELF'+bytes(128),b'not_elf')) # no container parsing
 x=image();x[7]=0;cases.append((x,b'unsupported'))
 x=image();put(x,16,2,2);cases.append((x,b'unsupported'))
 x=image();put(x,32,2**64-1,8);cases.append((x,b'malformed'))
 x=image();put(x,56,129,2);cases.append((x,b'resource_limit'))
 x=image();put(x,72,2**64-1,8);cases.append((x,b'malformed'))
 x=image();put(x,120,2,4);cases.append((x,b'malformed'))
 x=image();put(x,0x210,0x61000035,8);cases.append((x,b'malformed'))
 x=image();put(x,0x238,0,8);cases.append((x,b'malformed')) # wrong entry size
 x=image();put(x,0x2a8,2,8);cases.append((x,b'malformed')) # bad PLT format
 x=image();put(x,0x608,(100<<32)|6,8);cases.append((x,b'malformed')) # bad symbol index
 x=image();put(x,0x268,25,8);cases.append((x,b'malformed')) # nonmultiple relocation size
 x=image();put(x,0x260,0x6100002f,8);cases.append((x,b'malformed')) # duplicate tag

 x=image();put(x,0x228,0x3ff,8);cases.append((x,b'malformed')) # symbol table out of data
 x=image();put(x,0x518,40,4);cases.append((x,b'malformed')) # invalid name offset
 x=image();x[0x401:0x420]=b'x'*31;cases.append((x,b'malformed'))
 x=image();put(x,0x228,0,8);put(x,0x248,24*4097,8);put(x,152,24*4097+0x100,8);x.extend(bytes(24*4097));cases.append((x,b'resource_limit'))
 for index,(data,error) in enumerate(cases):
  p=run(data);assert p.returncode!=0 and error in p.stderr,(index,error,p.returncode,p.stderr)
 # A huge sparse host file demonstrates bounded reads; never allocate its size.
 if sys.platform.startswith('linux'):
  with path.open('wb') as f:f.write(base);f.truncate(1<<33)
  p=subprocess.run([cli,'--inspect-sce-imports',str(path)],capture_output=True,timeout=10)
  assert p.returncode==0 and b'entries=2' in p.stdout,(p.returncode,p.stderr)
print(f'synthetic SCE-style import metadata, {len(cases)} rejection cases and bounded sparse-file read passed')
