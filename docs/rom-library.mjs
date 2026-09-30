import {GAMES} from './config.mjs';

// Index names only. File bytes are read only for the game being started.
export function indexRomFolder(files) {
 const entries=new Map();let zipCount=0,folder='';
 for(const file of files){
  folder ||= file.webkitRelativePath?.split('/')[0] || '';
  const name=file.name.toLowerCase();if(!name.endsWith('.zip'))continue;
  zipCount++;if(!entries.has(name))entries.set(name,[]);entries.get(name).push(file);
 }
 return {entries,zipCount,folder};
}
export function selectGameRoms(library,system) {
 if(!Object.hasOwn(GAMES,system))throw Error('未対応のゲームです。');
 const files=[],missing=[],duplicates=[];
 for(const rom of GAMES[system].roms){
  const name=rom+'.zip',matches=library.entries.get(name)||[];
  if(matches.length===1)files.push(matches[0]);
  else if(matches.length>1)duplicates.push(name);
  else missing.push(name);
 }
 return {files,missing,duplicates,hasGame:files.some(f=>f.name.toLowerCase()===system+'.zip')};
}
