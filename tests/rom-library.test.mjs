import {test} from 'node:test';import assert from 'node:assert/strict';
import {indexRomFolder,selectGameRoms} from '../docs/rom-library.mjs';
const file=(name,path='roms/'+name)=>({name,webkitRelativePath:path,size:12,arrayBuffer(){throw Error('Indexing must not read file contents');}});
test('folder scan finds nested, case-insensitive game and BIOS ZIPs without reading data',()=>{
 const lib=indexRomFolder([file('RAVERACE.ZIP'),file('NAMCOC71.ZIP','roms/bios/NAMCOC71.ZIP'),file('namcoc74.zip'),file('photo.png'),file('other.zip')]);
 const s=selectGameRoms(lib,'raverace');assert.equal(lib.folder,'roms');assert.equal(lib.zipCount,4);assert.equal(s.hasGame,true);assert.equal(s.files.length,3);assert.deepEqual(s.missing,[]);assert.deepEqual(s.duplicates,[]);
});
test('changing game reuses the folder but never loads unrelated games',()=>{
 const lib=indexRomFolder(['vr.zip','model1io.zip','m1comm.zip','raverace.zip','namcoc71.zip','namcoc74.zip'].map(n=>file(n)));
 assert.deepEqual(selectGameRoms(lib,'vr').files.map(f=>f.name),['vr.zip','model1io.zip','m1comm.zip']);assert.equal(selectGameRoms(lib,'raverace').files.length,3);
});
test('missing game and duplicated ZIP names are reported explicitly',()=>{
 const lib=indexRomFolder([file('raverace.zip'),file('namcoc71.zip'),file('NAMCOC71.ZIP','roms/old/NAMCOC71.ZIP')]);const s=selectGameRoms(lib,'raverace');assert.deepEqual(s.duplicates,['namcoc71.zip']);assert.deepEqual(s.missing,['namcoc74.zip']);assert.equal(selectGameRoms(lib,'vr').hasGame,false);
});
