export const STICK_RADIUS=56/.9;
export function stickPosition(origin,point){const dx=point.x-origin.x,dy=point.y-origin.y;const scale=Math.min(1,STICK_RADIUS/(Math.hypot(dx,dy)||1));return {dx:dx*scale,dy:dy*scale,x:.5+dx*scale/STICK_RADIUS*.5,y:.5+dy*scale/STICK_RADIUS*.5};}
export class TiltFilter {
 constructor(){this.reset();}
 reset(){this.gravitySign=null;this.reference=null;this.raw=0;this.roll=0;this.angle=0;this.steering=.5;}
 center(){this.reference=this.raw;this.angle=0;this.steering=.5;}
 sampleAcceleration(total,linear,orientation=0){
  // Remove user acceleration when supplied. Browsers differ in accelerometer
  // gravity sign; normalize once from the upright screen, without UA sniffing.
  const x=(total.x-(Number.isFinite(linear?.x)?linear.x:0))/9.81;
  const y=(total.y-(Number.isFinite(linear?.y)?linear.y:0))/9.81;
  const a=orientation*Math.PI/180,screenY=x*Math.sin(a)+y*Math.cos(a);
  if(this.gravitySign===null){if(Math.abs(screenY)<.2)return false;this.gravitySign=screenY>0?-1:1;}
  return this.sample({x:x*this.gravitySign,y:y*this.gravitySign},orientation);
 }
 sample(g,orientation=0){
  const a=orientation*Math.PI/180,c=Math.cos(a),s=Math.sin(a),x=g.x*c-g.y*s,y=g.x*s+g.y*c;
  if(!Number.isFinite(x+y)||Math.hypot(x,y)<.2)return false;
  const raw=Math.atan2(x,-y);this.raw=raw;if(this.reference===null)this.reference=raw;
  const delta=Math.atan2(Math.sin(raw-this.reference),Math.cos(raw-this.reference));
  this.roll+=(Math.max(-Math.PI/4,Math.min(Math.PI/4,raw))-this.roll)*.22;
  this.angle+=(delta-this.angle)*.22;
  const dead=2*Math.PI/180;this.steering=.5+Math.sign(this.angle)*Math.min(1,Math.max(0,Math.abs(this.angle)-dead)/(Math.PI/6-dead))*.5;return true;
 }
}
export function setupMobileInput({getState,send,status}){
 const $=id=>document.getElementById(id),filter=new TiltFilter(),surface=$('stick-surface'),ring=$('stick-ring');
 let primary=null,origin=null,stick={x:.5,y:.5},groundUntil=0,available=false,listening=false,lastMotion=0,lastHaptic=0;
 const race=()=>getState().race;
 function reset(){primary=null;origin=null;stick={x:.5,y:.5};groundUntil=0;ring.hidden=true;filter.reset();available=false;sendSample();}
 function sendSample(){const s=getState(),sensor=s.race&&available&&performance.now()-lastMotion<500,motion=sensor&&s.settings.tilt;
  send({type:'analog',active:s.busy&&s.settings.touch&&matchMedia('(pointer:coarse)').matches || s.busy&&motion,x:motion?filter.steering:stick.x,y:stick.y,fire:!s.race&&(primary!==null||!!document.querySelector('[data-key="ControlLeft"].pressed')),ground:performance.now()<groundUntil||!!document.querySelector('[data-key="AltLeft"].pressed'),brake:!!document.querySelector('[data-key="ArrowDown"].pressed'),roll:s.busy&&s.settings.level&&sensor?-filter.roll:0});
 }
 function release(e){if(e&&primary!==e.pointerId)return;primary=null;origin=null;stick={x:.5,y:.5};ring.hidden=true;sendSample();}
 surface.addEventListener('pointerdown',e=>{
  if(e.pointerType==='mouse')return;
  const state=getState();if(!state.busy || !state.settings.touch || race()&&state.settings.tilt&&available&&performance.now()-lastMotion<500)return;
  e.preventDefault();try{surface.setPointerCapture(e.pointerId);}catch{}
  if(primary!==null){if(state.system==='solvalou')groundUntil=performance.now()+120;sendSample();return;}
  primary=e.pointerId;const bounds=surface.getBoundingClientRect();origin={x:e.clientX,y:e.clientY};stick={x:.5,y:.5};ring.hidden=false;ring.style.left=`${e.clientX-bounds.left}px`;ring.style.top=`${e.clientY-bounds.top}px`;ring.style.setProperty('--dx','0px');ring.style.setProperty('--dy','0px');sendSample();
 });
 surface.addEventListener('pointermove',e=>{if(e.pointerId!==primary||!origin)return;e.preventDefault();stick=stickPosition(origin,{x:e.clientX,y:e.clientY});ring.style.setProperty('--dx',stick.dx+'px');ring.style.setProperty('--dy',stick.dy+'px');sendSample();});
 for(const type of ['pointerup','pointercancel','lostpointercapture'])surface.addEventListener(type,release);
 window.addEventListener('blur',()=>{reset();navigator.vibrate?.(0);});document.addEventListener('visibilitychange',()=>{if(document.hidden){reset();navigator.vibrate?.(0);}});
 async function enable(){
  if(!isSecureContext){status('傾き操作にはHTTPS接続が必要です。');return;}
  if(!window.DeviceMotionEvent){status('このブラウザでは傾きセンサを利用できません。');return;}
  try{
   if(typeof DeviceMotionEvent.requestPermission==='function' && await DeviceMotionEvent.requestPermission()!=='granted'){status('傾きセンサの利用が許可されていません。タッチスティックを利用できます。');return;}
   if(!listening){window.addEventListener('devicemotion',e=>{if(document.hidden)return;const g=e.accelerationIncludingGravity;if(!g || !Number.isFinite(g.x)||!Number.isFinite(g.y))return;const angle=screen.orientation?.angle??window.orientation??0;if(filter.sampleAcceleration(g,e.acceleration,angle)){available=true;lastMotion=performance.now();sendSample();}});listening=true;}
   filter.reset();status('センサを有効にしました。持ちやすい角度で「中央補正」を押してください。');
  }catch(e){status(`センサを開始できません: ${e.message}`);}
 }
 $('enable-motion').onclick=enable;$('recenter-motion').onclick=()=>{filter.center();sendSample();};
 function haptic(strength){const s=getState(),now=performance.now();if(!s.busy||!s.settings.haptics||document.hidden||!Number.isFinite(strength)||strength<=0||now-lastHaptic<80)return;lastHaptic=now;navigator.vibrate?.(Math.round(20+50*Math.min(1,strength)));}
 $('test-haptic').onclick=()=>{if(!navigator.vibrate){status('このブラウザは端末の振動に対応していません。iPhoneではネイティブ版をご利用ください。');return;}status(navigator.vibrate(70)?'振動を送信しました。':'振動を利用できません。');};
 $('setting-haptics').disabled=!navigator.vibrate;
 $('haptic-support').textContent=navigator.vibrate?'対応端末ではゲームの衝撃に合わせて振動します。':'このブラウザは端末振動に非対応です（iPhone Safariを含む）。';
 let lastSystem;
 const timer=setInterval(()=>{const s=getState();if(s.system!==lastSystem){reset();lastSystem=s.system;}surface.hidden=!s.busy||!s.settings.touch||!matchMedia('(pointer:coarse)').matches;surface.classList.toggle('racing-stick',s.race);surface.style.pointerEvents=s.race&&s.settings.tilt&&available&&performance.now()-lastMotion<500?'none':'';if(!s.busy||document.hidden){if(primary!==null)release();return;}sendSample();},1000/60);
 window.addEventListener('pagehide',()=>{clearInterval(timer);navigator.vibrate?.(0);});
 return {haptic,reset};
}
