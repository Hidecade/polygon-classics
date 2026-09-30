// Hosted assets stay below the provider's per-file limit. Modern browsers use
// the compressed core; older browsers can assemble the same bytes from chunks.
export async function loadHostedCore(progress=()=>{}) {
  const manifestResponse=await fetch('core/manifest.json');
  if(!manifestResponse.ok)throw Error('実行コアの情報を読み込めません');
  const manifest=await manifestResponse.json();
  if(!Number.isSafeInteger(manifest.size)||manifest.size<8||manifest.size>128*1024*1024||!Array.isArray(manifest.parts)||!/^[a-f0-9]{64}$/.test(manifest.sha256))throw Error('実行コアの情報が不正です');
  const url=name=>{
    if(typeof name!=='string'||!/^starblade\.[a-z0-9.-]+$/.test(name))throw Error('実行コアのファイル名が不正です');
    return `core/${name}`;
  };
  const binary=new Uint8Array(manifest.size);let offset=0;
  if(typeof DecompressionStream==='function') {
    const response=await fetch(url(manifest.gzip));
    if(!response.ok||!response.body)throw Error('実行コアをダウンロードできません');
    const reader=response.body.pipeThrough(new DecompressionStream('gzip')).getReader();
    try {
      for(;;){const {done,value}=await reader.read();if(done)break;if(offset+value.length>binary.length)throw Error('実行コアのサイズが不正です');binary.set(value,offset);offset+=value.length;progress(offset,binary.length);}
    } finally {await reader.cancel().catch(()=>{});}
  } else {
    for(const part of manifest.parts){
      const response=await fetch(url(part));if(!response.ok)throw Error('実行コアをダウンロードできません');
      const bytes=new Uint8Array(await response.arrayBuffer());if(offset+bytes.length>binary.length)throw Error('実行コアのサイズが不正です');binary.set(bytes,offset);offset+=bytes.length;progress(offset,binary.length);
    }
  }
  if(offset!==binary.length)throw Error('実行コアのダウンロードが完了しませんでした');
  const digest=await crypto.subtle.digest('SHA-256',binary);
  const hash=Array.from(new Uint8Array(digest),byte=>byte.toString(16).padStart(2,'0')).join('');
  if(hash!==manifest.sha256)throw Error('実行コアの検証に失敗しました。再読み込みしてください');
  return binary;
}
