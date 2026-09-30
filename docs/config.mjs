export const WIDTH = 1920;
export const HEIGHT = 1080;
export const DISPLAY_MODES = {Original:1440,FHD:1920,UltraWide:2580};
export const displayWidth = mode => Object.hasOwn(DISPLAY_MODES,mode) ? DISPLAY_MODES[mode] : 1920;
export const GAMES = {
 starblad:{title:'STAR BLADE · World',family:'starblade',roms:['starblad','namcoc67','namcoc68']},
 starbladj:{title:'STAR BLADE · Japan',family:'starblade',roms:['starbladj','starblad','namcoc67','namcoc68']},
 solvalou:{title:'SOLVALOU',family:'starblade',roms:['solvalou','namcoc67','namcoc68']},
 vr:{title:'Virtua Racing',family:'model1',roms:['vr','model1io','m1comm']},
 srallyc:{title:'SEGA Rally Championship',family:'model2',roms:['srallyc','segabill']},
 daytona:{title:'DAYTONA USA',family:'model2',roms:['daytona','model1io']},
 ridgerac:{title:'RIDGE RACER',family:'system22',roms:['ridgerac','namcoc71','namcoc74']},
 ridgera2:{title:'RIDGE RACER 2',family:'system22',roms:['ridgera2','namcoc71','namcoc74']},
 raverace:{title:'RAVE RACER',family:'system22',roms:['raverace','namcoc71','namcoc74']}
};
export const ROM_NAMES=new Set(Object.values(GAMES).flatMap(game=>game.roms.map(name=>`${name}.zip`)));
export const racing=system=>GAMES[system]?.family!=='starblade';
export function validateRoms(files, system) {
  if (!Object.hasOwn(GAMES,system)) throw new Error('未対応のバージョンです。');
  const names = new Set();
  for (const file of files) {
    const name = file.name.toLowerCase();
    if (!ROM_NAMES.has(name)) throw new Error(`未対応のファイル: ${file.name}`);
    if (names.has(name)) throw new Error(`ROMが重複しています: ${name}`);
    if (file.size === 0 || file.size > 128 * 1024 * 1024) throw new Error(`ROMのサイズを確認してください: ${name}`);
    names.add(name);
  }
  if (!names.has(`${system}.zip`)) throw new Error(`${system}.zip を選択してください。`);
}
export function mameArguments(system, aim = 'Keyboard', display = 'FHD') {
  if (!Object.hasOwn(GAMES,system)) throw new Error('Invalid system');
  return [system, '-rompath', '/roms', '-cfg_directory', '/cfg', '-nvram_directory', '/nvram',
    '-video', 'soft', '-sound', 'js', '-window', '-resolution', `${displayWidth(display)}x${HEIGHT}`,
    // -aspect describes the physical monitor, not the game's aspect ratio.
    // The driver sets 16:9; auto keeps browser pixels square on any display.
    '-aspect', 'auto', '-keepaspect', '-unevenstretch',
    ...(aim === 'Mouse' && ['starblad','starbladj'].includes(system) ? ['-mouse','-adstick_device','mouse'] : ['-nomouse']),
    '-skip_gameinfo', '-noreadconfig', '-nowriteconfig', '-samplerate', '48000'];
}
export function inputConfiguration(system, aim = 'Keyboard') {
 if(!Object.hasOwn(GAMES,system))throw Error('Invalid system');
 const ports=[];
 if(aim==='Mouse' && ['starblad','starbladj'].includes(system)){
  for(const [axis,dec,inc] of [['X','LEFT','RIGHT'],['Y','UP','DOWN']])ports.push(`<port type="P1_AD_STICK_${axis}"><newseq type="standard">MOUSECODE_1_${axis}AXIS</newseq><newseq type="decrement">KEYCODE_${dec}</newseq><newseq type="increment">KEYCODE_${inc}</newseq></port>`);
  ports.push('<port type="P1_BUTTON1"><newseq type="standard">KEYCODE_LCONTROL OR MOUSECODE_1_BUTTON1</newseq></port>');
 }
 const port=(type,seqs)=>ports.push(`<port type="${type}">${Object.entries(seqs).map(([type,key])=>`<newseq type="${type}">KEYCODE_${key}</newseq>`).join('')}</port>`);
 if(racing(system)){
  port('P1_PADDLE',{decrement:'LEFT',increment:'RIGHT'});port('P1_PEDAL',{increment:'UP'});port('P1_PEDAL2',{increment:'DOWN'});port('P1_PEDAL3',{increment:'SPACE'});
  ['A','S','D','F','G','H','Z','X','J'].forEach((key,i)=>port(`P1_BUTTON${i+1}`,{standard:key}));
  port('P1_JOYSTICK_UP',{standard:'Z'});port('P1_JOYSTICK_DOWN',{standard:'X'});port('P1_JOYSTICK_LEFT',{standard:'A'});port('P1_JOYSTICK_RIGHT',{standard:'S'});
 }
 return `<?xml version="1.0"?><mameconfig version="10"><system name="default"><input>${ports.join('')}</input></system></mameconfig>`;
}
