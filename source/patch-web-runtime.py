"""Patch the pinned Emscripten 6.0.2 browser memory bridges after linking.

Web Crypto rejects views backed by resizable WASM memory. Fill fixed-size
scratch buffers using the same secure API, then copy bytes into the heap.
The generated bridge's WASI success return value (0) is preserved.
Use ordinary WASM buffer views for TextDecoder and other browser APIs;
Emscripten refreshes those views whenever its heap grows.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = 'return view=>(crypto.getRandomValues(view),0)'
FIXED = ('return view=>{const bytes=new Uint8Array(view.buffer,view.byteOffset,view.byteLength);'
         'for(let offset=0;offset<bytes.length;offset+=65536){'
         'const chunk=new Uint8Array(Math.min(65536,bytes.length-offset));'
         'crypto.getRandomValues(chunk);bytes.set(chunk,offset)}return 0}')
RESIZABLE = 'function getMemoryBuffer(){try{var b=wasmMemory.toResizableBuffer();return b}catch{}return wasmMemory.buffer}'
COMPATIBLE = 'function getMemoryBuffer(){return wasmMemory.buffer}'

def patch(path):
    source = path.read_text(encoding='utf-8')
    # Diagnostic links retain whitespace/comments; normalise just the two
    # reviewed bridges so the same guarded patch handles both output styles.
    source = re.sub(r'return view\s*=>\s*\(crypto\.getRandomValues\(view\),\s*0\)', ORIGINAL, source)
    diagnostic_heap = re.search(r'function getMemoryBuffer\(\) \{[\s\S]*?\n\}',source)
    if diagnostic_heap:
        body = diagnostic_heap.group()
        if 'wasmMemory.toResizableBuffer()' not in body or 'return wasmMemory.buffer;' not in body:
            raise RuntimeError('Unexpected diagnostic heap implementation')
        source = source.replace(body,RESIZABLE)
    for original, fixed in [(ORIGINAL,FIXED),(RESIZABLE,COMPATIBLE)]:
        if source.count(fixed) == 1 and original not in source:
            continue
        if source.count(original) != 1 or fixed in source:
            raise RuntimeError('Unexpected Emscripten runtime; review before publishing')
        source = source.replace(original, fixed)
    path.write_text(source, encoding='utf-8', newline='\n')
    print('Patched browser entropy and compatible WASM heap views')

if __name__ == '__main__':
    patch(ROOT / 'docs/core/starblade.js')
