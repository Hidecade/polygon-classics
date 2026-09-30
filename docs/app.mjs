import {indexRomFolder,selectGameRoms,availableGames} from './rom-library.mjs';
import {setupMobileInput} from './mobile-input.mjs';
import {validateRoms,GAMES,racing,displayWidth,DISPLAY_MODES} from './config.mjs';
import {HOSTED} from './deployment.mjs';
import {setupRanking} from './ranking.mjs';
setupRanking();
const $ = id => document.getElementById(id);
const mobile = matchMedia('(pointer:coarse)').matches || /iPhone|iPod/.test(navigator.userAgent);
const defaults = {display:'FHD',fullscreen:false,fps:false,renderer:'GPU',renderStyle:'Solid',glow:'Weak',aim:'Mouse',touch:mobile,tilt:false,level:true,haptics:true};
let settings = {...defaults};
try { settings = {...defaults,...JSON.parse(localStorage.getItem('starblade.menu') || '{}')}; } catch {}
if(!Object.hasOwn(DISPLAY_MODES,settings.display))settings.display='FHD';
if(!['Solid','Wireframe'].includes(settings.renderStyle))settings.renderStyle=defaults.renderStyle;
let coreReady=false, playerReady=false, busy=false, boardLoading=false, localReady=false, generation=0;
let paused=false;
let romLibrary=indexRomFolder([]);
let runtime={native:false,platform:'web',running:false};
const log=message=>{$('log').textContent=($('log').textContent+message+'\n').slice(-24000);};
const status=message=>{$('status').textContent=message;};
new ResizeObserver(([entry])=>{const {width,height}=entry.contentRect;$('screen').style.setProperty('--game-scale',Math.min(width/displayWidth(settings.display),height/1080));}).observe($('screen'));
function updateGameList(){
 const previous=$('system').value,systems=availableGames(romLibrary);
 $('system').replaceChildren(...systems.map(system=>new Option(GAMES[system].title,system)));
 if(systems.includes(previous))$('system').value=previous;
 $('game-choice').hidden=!systems.length;
}
updateGameList();
// Keep overlays inside the fullscreen element.
$('pause-panel').append($('game-tools'));
$('screen').append($('touch-controls'));
function refresh(){
 const width=displayWidth(settings.display);
 $('screen').style.aspectRatio=`${width}/1080`;$('game').style.width=`${width}px`;
 $('setting-display').value=settings.display;$('setting-display').disabled=busy;
 $('display-resolution').textContent=`${width} × 1080 / ${{Original:"4:3",FHD:"16:9",UltraWide:"43:18"}[settings.display]}`;
 $('display-warning').hidden=settings.display!=="UltraWide";
 const selected=$('system').value,game=GAMES[selected],race=!!game&&racing(selected);
 const choice=game?selectGameRoms(romLibrary,selected):{files:[],hasGame:false,duplicates:[]},selectedReady=choice.hasGame&&!choice.duplicates.length;
 $('rom-hint').textContent=game?'関連ZIP: '+game.roms.map(n=>n+'.zip').join(' / '):'';
 $('folder-summary').textContent=romLibrary.zipCount?`${romLibrary.folder||'選択フォルダ'} · ZIP ${romLibrary.zipCount}件 / このゲーム ${choice.files.length}件`:'フォルダ未選択';
 const logo=game?.family==='starblade';
 $('title-logo').hidden=!logo;$('title-name').hidden=logo;
 if(logo)$('title-logo').src=selected==='solvalou'?'solvalou-loading-logo.png':'starblade-loading-logo.png';else $('title-logo').removeAttribute('src');
 $('title-logo').alt=game?.title||'';$('title-name').textContent=game?.title||'POLYGON CLASSICS';
 $('selected-game').textContent=game?.title||'ROMフォルダを選択';
 $('selected-family').textContent={starblade:'NAMCO SYSTEM 21',model1:'SEGA MODEL 1',model2:'SEGA MODEL 2',system22:'NAMCO SYSTEM 22'}[game?.family]||'';
 $('shoot-controls').hidden=race;$('race-controls').hidden=!race;$('secondary-fire').hidden=selected!=='solvalou';
 for(const button of document.querySelectorAll('[data-race]'))button.hidden=!button.dataset.race.split(',').some(key=>key===selected||key===game?.family);
 $('game').title=game?game.title+'エミュレータ':'ゲーム画面';$('game-label').textContent=game?.title||'POLYGON CLASSICS';
 $('motion-options').hidden=!race;$('recenter-motion').hidden=!race;
 const native=$('target').value==='native';
 $('ranking-menu').hidden=HOSTED || !native || !runtime.native;
 $('ranking-menu').disabled=busy || runtime.running;
 $('target').closest('label').hidden=HOSTED;
 $('play').disabled=!game || busy || (native ? !runtime.native || runtime.running : !coreReady || !playerReady || (!localReady && !selectedReady));
 $('start').disabled=busy || !coreReady || !playerReady || !selectedReady;
 $('local').disabled=busy || !coreReady || !playerReady;
 $('local').hidden=!localReady || native;
 $('start').hidden=native;
 $('rom').disabled=busy || native;$('choose-folder').disabled=busy || native;
 $('system').disabled=!game || busy || native;
 $('target').disabled=busy;
 $('pause-toggle').hidden=!busy||boardLoading||native;
 $('pause-toggle').setAttribute('aria-expanded',String(paused));
 $('pause-panel').hidden=!paused;
 $('touch-controls').hidden=!busy || paused || boardLoading || native || !settings.touch;
 $('primary-fire').hidden=mobile&&settings.touch&&['starblad','starbladj'].includes(selected);
 $('setting-renderer').disabled=busy || (native && runtime.platform!=='win32');
 $('setting-render-style').disabled=!native;
 $('setting-glow').disabled=!native;
 $('setting-fps').disabled=!native;
 $('setting-aim').disabled=!native && !['starblad','starbladj'].includes(selected);
 $('video-note').textContent=native ? 'UIは4:3、ゲーム画面は1920×1080を維持します。GPU描画はWindows版で利用できます。' : 'Web版はWebGPUで描画します。非対応環境ではCPUへ自動切替します。縦横比を維持して、選択した表示範囲で描画します。UI発光・速度表示の切替はネイティブ版の設定です。';
 $('control-summary').textContent=!game?'ROMフォルダを選択すると、利用可能なゲームが表示されます。':race ? '5 コイン / 1 スタート / ← → ハンドル / ↑ アクセル / ↓ ブレーキ' : settings.touch ? '画面をドラッグして照準・自動射撃。離すと中央に戻ります。対応ゲームでは2本目の指で追加射撃。' : settings.aim==='Mouse' && (native || ['starblad','starbladj'].includes(selected)) ? '5 コイン / 1 スタート / マウス 照準 / 左クリック 発射' : '5 コイン / 1 スタート / 矢印キー 照準 / Ctrl 発射';
}
$('setting-display').onchange=()=>{settings.display=$('setting-display').value;try{localStorage.setItem('starblade.menu',JSON.stringify(settings));}catch{}refresh();};
function loadSettings(){for(const name of ['fullscreen','fps','touch','tilt','level','haptics'])$(`setting-${name}`).checked=!!settings[name];for(const name of ['renderer','render-style','glow','aim'])$(`setting-${name}`).value=settings[name==='render-style'?'renderStyle':name];}
function saveSettings(){for(const name of ['fullscreen','fps','touch','tilt','level','haptics'])settings[name]=$(`setting-${name}`).checked;for(const name of ['renderer','render-style','glow','aim'])settings[name==='render-style'?'renderStyle':name]=$(`setting-${name}`).value;try{localStorage.setItem('starblade.menu',JSON.stringify(settings));}catch{}refresh();}
loadSettings();
for(const kind of ['video','input']){
 $(`${kind}-menu`).onclick=()=>{refresh();$(`${kind}-dialog`).showModal();};
 $(`${kind}-dialog`).querySelector('.save').addEventListener('click',saveSettings);
 $(`${kind}-dialog`).addEventListener('close',loadSettings);
}
function resetPlayer(){generation++;playerReady=false;$('game').src='player.html';}
function menu(){releaseAll();paused=false;busy=false;boardLoading=false;$('screen').hidden=true;$('game-tools').hidden=true;$('menu').hidden=false;$('setup').hidden=false;$('selection').hidden=false;$('ended').hidden=true;document.body.classList.remove('playing','expanded');$('screen').classList.remove('expanded');refresh();}
function stop(){if(document.fullscreenElement)document.exitFullscreen().catch(()=>{});resetPlayer();menu();status('終了しました。もう一度起動できます。');}
$('stop').onclick=stop;
$('home').onclick=event=>{event.preventDefault();if(busy)stop();else menu();};
$('quit').onclick=()=>{if(busy)stop();$('menu').hidden=true;$('setup').hidden=true;$('selection').hidden=true;$('ended').hidden=false;};
$('return').onclick=()=>{menu();$('play').focus();};
async function fullscreen(){
 try{if(settings.touch){document.body.classList.toggle('expanded');$('screen').classList.toggle('expanded');return;}if(document.fullscreenElement)await document.exitFullscreen();else if($('screen').requestFullscreen)await $('screen').requestFullscreen();else{document.body.classList.toggle('expanded');$('screen').classList.toggle('expanded');}}catch{document.body.classList.toggle('expanded');$('screen').classList.toggle('expanded');}
}
$('fullscreen').onclick=fullscreen;
function setPaused(value){
 if(!busy||boardLoading)return;
 paused=value;releaseAll();mobileInput.reset();
 $('game').contentWindow.postMessage({type:'pause',paused},location.origin);
 refresh();
 if(paused)$('resume').focus();else $('game').focus();
}
$('pause-toggle').onclick=()=>setPaused(!paused);
$('resume').onclick=()=>setPaused(false);
for(const button of document.querySelectorAll('[data-menu-key]'))button.onclick=()=>{
 const run=generation,code=button.dataset.menuKey;setPaused(false);input(code,true);
 setTimeout(()=>{if(run===generation)input(code,false);},180);
};
$('target').onchange=()=>{status($('target').value==='native'?'保存済みROMを使ってアプリを起動します。':'ROMフォルダを選択するか、保存済みROMで開始してください。');refresh();};
$('choose-folder').onclick=()=>{
 if(!('webkitdirectory' in $('rom'))){showFailure('このブラウザはフォルダ選択に対応していません。対応する最新版のブラウザで開いてください。');return;}
 $('rom').click();
};
function describeFolder(){
 if(!$('system').value){status(romLibrary.zipCount?'このフォルダに対応するゲームのROM ZIPが見つかりません。':'ROM ZIPが入ったフォルダを選択してください。');return;}
 const choice=selectGameRoms(romLibrary,$('system').value);
 if(!romLibrary.zipCount){status('ROM ZIPが入ったフォルダを選択してください。');return;}
 if(choice.duplicates.length){status('同名のZIPが複数あります: '+choice.duplicates.join(' / ')+'。重複を除いたフォルダを選んでください。');return;}
 if(!choice.hasGame){status($('system').value+'.zip が選択フォルダにありません。');return;}
 status('読込対象: '+choice.files.map(f=>f.name).join(' / ')+(choice.missing.length?'。関連ZIPが見つかりません: '+choice.missing.join(' / ')+'（統合ROMの場合は不要なことがあります）。':'。ゲーム開始を押してください。'));
}
$('rom').onchange=()=>{romLibrary=indexRomFolder($('rom').files);updateGameList();localReady=false;$('boot-error').hidden=true;refresh();describeFolder();};
function showFailure(message){
 const text=$('log').textContent;
 let detail=String(message||'起動に失敗しました。');
 if(/NOT FOUND|Required files are missing/.test(text)){
  const missing=$('system').value?selectGameRoms(romLibrary,$('system').value).missing:[];
  const names=[...new Set([...text.matchAll(/^(.+?) NOT FOUND/gm)].map(m=>m[1]))];
  detail='必要なROMが不足しています: '+names.join(' / ')+'. '+(missing.length?'同じROMフォルダに '+missing.join(' / ')+' を追加し、フォルダを選び直してください。':'ZIPの内容を起動ログで確認してください。');
 }
 $('boot-error-message').textContent=detail;$('boot-error').hidden=false;
 $('boot-error').focus();$('boot-error').scrollIntoView({block:'start'});
}

async function startWeb(local){
 const run=++generation;
 try{
  $('boot-error').hidden=true;busy=true;boardLoading=true;refresh();$('log').textContent='';status('ROMを読み込んでいます…');
  $('menu').hidden=true;$('screen').hidden=false;$('game-tools').hidden=false;$('cover').hidden=false;$('cover').querySelector('p').textContent='ROMを読み込んでいます…';document.body.classList.add('playing');
  if(settings.fullscreen)await fullscreen();
  let roms;
  if(local){
   roms=[];
   for(const name of GAMES[$('system').value].roms){const response=await fetch(`/local-rom/${name}.zip`);if(response.ok)roms.push({name:`${name}.zip`,data:await response.arrayBuffer()});else if(name===$('system').value)throw Error(`${name}.zip を読み込めません。`);}
  }else{
   const choice=selectGameRoms(romLibrary,$('system').value);
   if(choice.duplicates.length)throw Error('同名のROM ZIPが複数あります: '+choice.duplicates.join(' / '));
   const files=choice.files;validateRoms(files,$('system').value);
   roms=await Promise.all(files.map(async file=>({name:file.name.toLowerCase(),data:await file.arrayBuffer()})));
  }
  if(run!==generation)return;
  $('game').contentWindow.postMessage({type:'boot',system:$('system').value,roms,renderer:settings.renderer,aim:mobile&&settings.touch?'Keyboard':settings.aim,display:settings.display},location.origin,roms.map(rom=>rom.data));
 }catch(error){if(run!==generation)return;resetPlayer();menu();status(error.message);log(error.message);showFailure(error.message);}
}
$('start').onclick=()=>startWeb(false);$('local').onclick=()=>startWeb(true);
$('play').onclick=async()=>{
 if($('target').value!=='native')return startWeb(!romLibrary.zipCount && localReady);
 try{
  busy=true;refresh();status('アプリを起動しています…');
  const response=await fetch('/api/native-launch',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(settings)});
  const data=await response.json();if(!response.ok)throw new Error(data.error||'起動できませんでした。');
  runtime.running=true;status('アプリを起動しました。ゲームの終了はEscキーです。');
 }catch(error){status(error.message);log(error.message);}finally{busy=false;refresh();}
};
window.addEventListener('message',event=>{
 if(event.origin!==location.origin || event.source!==$('game').contentWindow)return;
 const data=event.data;
 if(data.type==='haptic')mobileInput.haptic(data.strength);
 if(data.type==='ready'){playerReady=true;refresh();}
 if(data.type==='log')log(data.message);
 if(data.type==='renderer')$('renderer-status').textContent=data.message;
 if(data.type==='loading'){$('cover').querySelector('p').textContent=data.message;}
 if(data.type==='board-loading'){
  boardLoading=data.loading===true;$('cover').hidden=!boardLoading;
  if(boardLoading){$('cover').querySelector('p').textContent='基板をチェックしています…';}
  else{status(data.timeout?'チェック画面を表示しました。起動ログを確認してください。':'MAME実行中');log(`基板チェック終了: ${Number(data.elapsed).toFixed(2)}秒（ゲーム内時間）${data.timeout?' / タイムアウトで画面表示':''}`);$('game').focus();}
  refresh();
 }
 if(data.type==='running'){boardLoading=false;$('cover').hidden=true;refresh();status('MAME実行中');$('game').focus();}
 if(data.type==='error'||data.type==='exit'){resetPlayer();menu();status(data.message);log(data.message);showFailure(data.message);}
});
const held=new Map();
function input(code,active){$('game').contentWindow.postMessage({type:'input',code,active},location.origin);}
function releaseAll(){for(const [id,{code,button}] of held){input(code,false);button.classList.remove('pressed');}held.clear();}
for(const button of document.querySelectorAll('[data-key]')){
 button.addEventListener('pointerdown',event=>{if(!busy)return;event.preventDefault();try{button.setPointerCapture(event.pointerId);}catch{}held.set(event.pointerId,{code:button.dataset.key,button});button.classList.add('pressed');input(button.dataset.key,true);});
 const release=event=>{const item=held.get(event.pointerId);if(!item)return;held.delete(event.pointerId);if(![...held.values()].some(v=>v.code===item.code)){input(item.code,false);item.button.classList.remove('pressed');}};
 for(const name of ['pointerup','pointercancel','lostpointercapture'])button.addEventListener(name,release);
 button.addEventListener('contextmenu',event=>event.preventDefault());
}
window.addEventListener('blur',()=>setTimeout(()=>{if(!document.hasFocus())releaseAll();},0));document.addEventListener('visibilitychange',()=>{if(document.hidden)releaseAll();});
async function checkRuntime(initial=false){try{const response=await fetch('/api/runtime');if(!response.ok)return;const before=runtime.running;runtime=await response.json();if(mobile)runtime.native=false;$('target').querySelector('[value=native]').disabled=!runtime.native;if(initial && runtime.native)$('target').value='native';if(runtime.error)status(runtime.error);else if(before && !runtime.running)status('アプリを終了しました。もう一度起動できます。');}catch{}refresh();}
const mobileInput=setupMobileInput({getState:()=>({busy:busy&&!boardLoading&&!paused,settings,system:$('system').value,race:racing($('system').value)}),send:data=>$('game').contentWindow.postMessage(data,location.origin),status:message=>{$('motion-status').textContent=message;}});
let localCheck=0;
async function checkLocal(){const ticket=++localCheck;localReady=false;refresh();if(!HOSTED && $('system').value)try{const response=await fetch(`/local-rom/${$('system').value}.zip`,{method:'HEAD'});if(ticket===localCheck)localReady=response.ok;}catch{}refresh();}
$('system').onchange=()=>{checkLocal();$('boot-error').hidden=true;describeFolder();};
async function checkCore(){try{for(const path of ['core/starblade.js',HOSTED?'core/manifest.json':'core/starblade.wasm']){if(!(await fetch(path,{method:'HEAD',cache:'no-store'})).ok)throw new Error('Web版コアが未ビルドです。');}coreReady=true;}catch(error){log(error.message);}}
await Promise.all([HOSTED?Promise.resolve():checkRuntime(true),checkCore(),checkLocal()]);
status(runtime.native?'ゲーム開始でWindowsアプリを起動します。':coreReady?'ROMフォルダを選択して、ゲーム開始を押してください。':'Web版コアがありません。READMEのビルド手順を確認してください。');
refresh();if(!HOSTED)setInterval(()=>checkRuntime(),3000);

// Match the native 1.4-second brand screen; tapping skips it.
let splashTimer;
function hideSplash(){clearTimeout(splashTimer);$('brand-splash').hidden=true;}
function showSplash(){clearTimeout(splashTimer);$('brand-splash').hidden=false;splashTimer=setTimeout(hideSplash,1400);}
$('brand-splash').onclick=hideSplash;
$('title-button').onclick=showSplash;
$('display-tab').onclick=()=>$('system').focus();
$('restore-defaults').onclick=()=>{settings={...defaults};loadSettings();saveSettings();status('設定を初期値に戻しました。');};
showSplash();
