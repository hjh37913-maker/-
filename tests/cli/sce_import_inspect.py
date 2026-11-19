"""Synthetic raw candidate-ELF metadata only; no proprietary input."""
import os,pathlib,struct,subprocess,sys,tempfile
cli=sys.argv[1]
def put(buf,pos,value,width):buf[pos:pos+width]=value.to_bytes(width,'little')
def image():
 b=bytearray(4096);b[:4]=b'\x7fELF';b[4:8]=bytes([2,1,1,9]);put(b,16,0xfe10,2);put(b,18,62,2);put(b,20,1,4);put(b,32,64,8);put(b,52,64,2);put(b,54,56,2);put(b,56,2,2)
 put(b,64,2,4);put(b,72,0x200,8);put(b,96,224,8)
 put(b,120,0x61000000,4);put(b,128,0x400,8);put(b,152,0x400,8)
 for i,(tag,val) in enumerate(((0x61000035,0),(0x61000037,64),(0x61000039,0x100),(0x6100003b,24),(0x6100003f,96),(0x6100002f,0x200),(0x61000031,24),(0x61000033,24),(0x61000029,0x218),(0x6100002d,24),(0x6100002b,7),(0x6100000f,(4<<48)|(2<<40)|(3<<32)|20),(0x61000015,(9<<48)|(12<<32)|28),(0,0))):put(b,0x200+16*i,tag,8);put(b,0x208+16*i,val,8)
 b[0x401:0x401+10]=b'fake#J#E\0\0';b[0x40b:0x40b+9]=b'weak\x1bimp\0';b[0x414:0x41c]=b'moduleX\0';b[0x41c:0x421]=b'libY\0'
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
 assert p.returncode==0 and b'NOT established' in p.stdout and b'entries=2' in p.stdout and b'OS-tagged RELA entries=2' in p.stdout and b'OS-tagged module/library declarations=2' in p.stdout and b'import-module id=4 version=2.3 name=moduleX' in p.stdout and b'import-library id=9 version=12.0 name=libY' in p.stdout and b'plt type=7 symbol=2 target=0xbeef addend=0x4' in p.stdout and b'name=fake#J#E library=libY module=moduleX' in p.stdout and b'name=weak\\x1bimp' in p.stdout and b'\x1b' not in p.stdout,(p.stdout,p.stderr)
 # Compare encoded qualifier boundaries against an independent Python encoder.
 ids=(0,63,64,4095,4096,65535)
 def enc(x):
  digits='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-';out=''
  while True:
   x,r=divmod(x,64);out=digits[r]+out
   if not x:return out
 q=bytearray(0x1000);q[:176]=base[:176];put(q,72,0x200,8);put(q,96,(5+len(ids)*2+1)*16,8);put(q,128,0x800,8);put(q,152,0x400,8)
 strings=bytearray(b'\0');symnames=[];tags=[]
 for id in ids:
  module=f'module{id}'.encode();library=f'library{id}'.encode();symbol=f'N#{enc(id)}#{enc(id)}'.encode()
  mo=len(strings);strings+=module+b'\0';li=len(strings);strings+=library+b'\0';symnames.append(len(strings));strings+=symbol+b'\0'
  tags.extend(((0x6100000f,(id<<48)|(1<<40)|(2<<32)|mo),(0x61000015,(id<<48)|(3<<32)|li)))
 assert len(strings)<0x100
 q[0x800:0x800+len(strings)]=strings
 for i,nameoff in enumerate(symnames,1):put(q,0x900+i*24,nameoff,4);q[0x904+i*24]=0x12
 fields=((0x61000035,0),(0x61000037,len(strings)),(0x61000039,0x100),(0x6100003f,(len(ids)+1)*24),(0x6100003b,24),*tags,(0,0))
 for i,(tag,val) in enumerate(fields):put(q,0x200+i*16,tag,8);put(q,0x208+i*16,val,8)
 p=run(q);assert p.returncode==0,(p.returncode,p.stderr)
 for id in ids:assert f'name=N#{enc(id)}#{enc(id)} library=library{id} module=module{id}'.encode() in p.stdout,p.stdout
 # Ambiguous duplicate library IDs must not be guessed.
 dup=image();put(dup,96,240,8);put(dup,0x2d0,0x61000015,8);put(dup,0x2d8,(9<<48)|(1<<32)|28,8);put(dup,0x2e0,0,8);put(dup,0x2e8,0,8)
 p=run(dup);assert p.returncode==0 and b'name=fake#J#E library=<unmatched> module=moduleX' in p.stdout,(p.returncode,p.stdout,p.stderr)
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
 x=image();put(x,0x2b8,(4<<48)|(2<<40)|(3<<32)|1000,8);cases.append((x,b'malformed')) # invalid module name

 x=image();put(x,0x268,25,8);cases.append((x,b'malformed')) # nonmultiple relocation size
 x=image();put(x,0x260,0x6100002f,8);cases.append((x,b'malformed')) # duplicate tag

 x=image();put(x,0x228,0x3ff,8);cases.append((x,b'malformed')) # symbol table out of data
 x=image();put(x,0x518,80,4);cases.append((x,b'malformed')) # invalid name offset
 x=image();x[0x401:0x440]=b'x'*63;cases.append((x,b'malformed'))
 x=image();put(x,0x228,0,8);put(x,0x248,24*4097,8);put(x,152,24*4097+0x100,8);x.extend(bytes(24*4097));cases.append((x,b'resource_limit'))
 for index,(data,error) in enumerate(cases):
  p=run(data);assert p.returncode!=0 and error in p.stderr,(index,error,p.returncode,p.stderr)
 # A huge sparse host file demonstrates bounded reads; never allocate its size.
 if sys.platform.startswith('linux'):
  with path.open('wb') as f:f.write(base);f.truncate(1<<33)
  p=subprocess.run([cli,'--inspect-sce-imports',str(path)],capture_output=True,timeout=10)
  assert p.returncode==0 and b'entries=2' in p.stdout,(p.returncode,p.stderr)
print(f'synthetic SCE-style import metadata, {len(cases)} rejection cases and bounded sparse-file read passed')
