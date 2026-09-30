import {mameArguments, validateRoms, GAMES, inputConfiguration, displayWidth} from './config.mjs';
import {createPolygonGPU} from './polygon-webgpu.mjs';
import {createWebGPU} from './webgpu.mjs';
import {HOSTED} from './deployment.mjs';
import {loadHostedCore} from './site-core.mjs';
const canvas = document.getElementById('canvas');
const send = (type,message) => parent.postMessage({type,message}, location.origin);
let started = false;
const keys = {Digit5:['5',53],Digit1:['1',49],ArrowUp:['ArrowUp',38],ArrowDown:['ArrowDown',40],ArrowLeft:['ArrowLeft',37],ArrowRight:['ArrowRight',39],ControlLeft:['Control',17],AltLeft:['Alt',18],Space:[' ',32],KeyA:['a',65],KeyS:['s',83],KeyD:['d',68],KeyF:['f',70],KeyG:['g',71],KeyH:['h',72],KeyJ:['j',74],KeyZ:['z',90],KeyX:['x',88],Enter:['Enter',13]};
const pressed = new Set();
window.addEventListener('error', event => { send('log',event.error?.stack || event.message); send('error', event.message); });
window.addEventListener('unhandledrejection', event => { send('log',event.reason?.stack || String(event.reason)); send('error', String(event.reason)); });
canvas.addEventListener('contextmenu', event=>event.preventDefault());
function activate() {
  canvas.focus();
  window.jsmame_web_audio?.get_context()?.resume().catch(error=>send('log',`音声: ${error.message}`));
}
canvas.addEventListener('pointerdown', activate);
canvas.addEventListener('keydown', event=>{
  // Tab belongs to MAME's menu while the game has focus, not browser navigation.
  if(['Tab','ArrowUp','ArrowDown','ArrowLeft','ArrowRight',' '].includes(event.key))event.preventDefault();
  activate();
});
window.addEventListener('message', async event => {
  if(event.origin !== location.origin || event.source !== parent || !event.data) return;
  if(event.data.type==='analog'){
    const d=event.data;if(!started || ![d.x,d.y,d.roll].every(Number.isFinite))return;
    window.polygonInput={active:d.active===true,x:Math.max(0,Math.min(1,d.x)),y:Math.max(0,Math.min(1,d.y)),fire:d.fire===true,ground:d.ground===true,brake:d.brake===true};
    window.polygonRoll=Math.max(-Math.PI/4,Math.min(Math.PI/4,d.roll));return;
  }
  if(event.data.type === 'input') {
    const {code,active} = event.data;
    if(!started || !Object.hasOwn(keys,code) || typeof active !== 'boolean' || pressed.has(code) === active) return;
    if(active) { pressed.add(code); activate(); } else pressed.delete(code);
    const [key,keyCode] = keys[code];
    canvas.dispatchEvent(new KeyboardEvent(active ? 'keydown' : 'keyup',{key,code,keyCode,which:keyCode,bubbles:true,cancelable:true,ctrlKey:code==='ControlLeft' && active}));
    return;
  }
  if(event.data.type !== 'boot' || started) return;
  started = true;
  try {
    const {roms,system} = event.data;
    const width=displayWidth(event.data.display);
    window.polygonRenderWidth=width;
    const aim=event.data.aim==='Mouse'?'Mouse':'Keyboard';
    if(aim==='Mouse' && canvas.requestPointerLock){
      // SDL can request capture several times during the same click. Coalesce
      // requests and handle browser refusal without terminating the emulator.
      const request=canvas.requestPointerLock.bind(canvas);
      let pending=null;
      canvas.requestPointerLock=(options)=>{
        if(document.pointerLockElement===canvas)return Promise.resolve();
        if(pending)return pending;
        try {
          pending=Promise.resolve(request(options)).catch(error=>send('log',`マウス捕捉: ${error.message}`)).finally(()=>{pending=null;});
          return pending;
        }catch(error){send('log',`マウス捕捉: ${error.message}`);return Promise.resolve();}
      };
    }
    validateRoms(roms.map(rom=>({name:rom.name,size:rom.data.byteLength})),system);
    let wasmBinary;
    if(HOSTED) {
      let percent=-1;
      wasmBinary=await loadHostedCore((loaded,total)=>{
        const next=Math.floor(loaded/total*100);
        if(next!==percent){percent=next;send('loading',`実行コアを読み込んでいます… ${percent}%`);}
      });
    }
    if(event.data.renderer !== 'CPU') {
      const family=GAMES[system].family;
      const create=family==='starblade'?createWebGPU:createPolygonGPU;
      const renderer=await create({
        width,
        kind:{model1:1,system22:2,model2:3}[family],
        canvas:document.getElementById('gpu-canvas'),
        log:message=>send('log',message),
        onState:message=>send('renderer',message),
        validate:new URL(location.href).searchParams.get('validateGPU') === '1'
      });
      if(family==='starblade')window.starbladeWebGPU=renderer;else window.polygonWebGPU=renderer;
    } else {send('log','CPU描画を選択しました');send('renderer','CPU描画（設定で選択）');}
    window.Module = {
      canvas,
      ...(wasmBinary ? {instantiateWasm(imports,success){
        WebAssembly.instantiate(wasmBinary,imports).then(({instance,module})=>{
          wasmBinary=undefined;success(instance,module);
        }).catch(error=>send('error',`実行コアを起動できません: ${error.message}`));
        return {};
      }} : {}),
      arguments: mameArguments(system,aim,event.data.display),
      locateFile: path => new URL(`core/${path}`,location.href).href,
      print: text => send('log',String(text)),
      printErr: text => send('log',String(text)),
      onAbort: reason => send('error',`MAME停止: ${reason}`),
      onExit: code => send('exit',`MAMEが終了しました (code ${code})。ログを確認してください。`),
      preRun: [() => {
        const fs = window.Module.FS;
        for(const dir of ['/roms','/cfg','/nvram']) fs.mkdirTree(dir);
        fs.writeFile('/cfg/default.cfg',inputConfiguration(system,aim));
        for(const rom of roms) fs.writeFile(`/roms/${rom.name}`,new Uint8Array(rom.data));
      }],
      onRuntimeInitialized: () => { send('loading','基板をチェックしています…'); }
    };
    const script = document.createElement('script');
    script.src = 'core/starblade.js';
    script.onerror = () => send('error','MAMEコアの読み込みに失敗しました。');
    document.body.append(script);
  } catch(error) { send('error',error.message); }
});
window.addEventListener('pagehide',()=>{window.starbladeWebGPU?.dispose();window.polygonWebGPU?.dispose();});
send('ready');
