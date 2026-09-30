"""Package a locally rebuilt core into the static site's existing layout."""
from pathlib import Path
import gzip,hashlib,json,shutil,subprocess,sys
root=Path(__file__).resolve().parents[1]
mame=Path(__file__).resolve().parent/'mame'
core=(mame/'starblade.wasm').read_bytes()
assert core[:4]==b'\0asm'
out=root/'docs/core';out.mkdir(parents=True,exist_ok=True)
for p in out.glob('starblade.wasm-part-*.bin'):p.unlink()
parts=[]
for index,offset in enumerate(range(0,len(core),8*1024*1024)):
 name=f'starblade.wasm-part-{index}.bin';(out/name).write_bytes(core[offset:offset+8*1024*1024]);parts.append(name)
name='starblade.wasm-gzip.bin';(out/name).write_bytes(gzip.compress(core,compresslevel=9,mtime=0))
(out/'manifest.json').write_text(json.dumps({'size':len(core),'sha256':hashlib.sha256(core).hexdigest(),'gzip':name,'parts':parts},indent=2)+'\n')
shutil.copyfile(mame/'starblade.js',out/'starblade.js')
subprocess.run([sys.executable,str(Path(__file__).with_name('patch-web-runtime.py'))],check=True)
print('Updated docs/core. Test before committing and publishing.')
