import * as shaders from './polygon-shaders.mjs';
export async function createPolygonGPU({canvas,kind,width=1920,log=()=>{},onState=()=>{}}) {
 const r=new PolygonGPU(canvas,kind,width,log,onState);let timer;
 try{await Promise.race([r.initialize(),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('GPU初期化タイムアウト')),15000);})]);}catch(e){r.fail(e.message);}finally{clearTimeout(timer);}return r;
}
class PolygonGPU {
 constructor(canvas,kind,width,log,onState){Object.assign(this,{canvas,kind,width,log,onState,ready:false,failed:false,frames:0,polygonFrames:0,buffers:[],uniforms:[],staticUploads:new Map()});this.hide();}
 hide(){this.canvas.hidden=true;}
 fail(reason){if(this.failed)return;this.failed=true;this.ready=false;this.reason=reason;this.hide();this.log(`WebGPU: ${reason}。CPU描画へ切り替えます。`);this.onState(`CPU描画 — ${reason}`);this.device?.destroy();}
 dispose(){this.ready=false;this.failed=true;this.hide();this.device?.destroy();}
 async pipeline(code,vertex='vertex',fragment='fragment',depth=false,format='rgba8unorm') {
  const module=this.device.createShaderModule({code});const info=await module.getCompilationInfo();const errors=info.messages.filter(m=>m.type==='error');if(errors.length)throw Error(errors.map(m=>`${m.lineNum}: ${m.message}`).join('\n'));
  return this.device.createRenderPipelineAsync({layout:'auto',vertex:{module,entryPoint:vertex},fragment:{module,entryPoint:fragment,targets:[{format}]},primitive:{topology:'triangle-list',cullMode:'none'},...(depth?{depthStencil:{format:'depth32float',depthWriteEnabled:true,depthCompare:'less'}}:{})});
 }
 texture(extra=0,format='rgba8unorm'){return this.device.createTexture({size:[this.width,1080],format,usage:GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.TEXTURE_BINDING|extra});}
 async initialize(){
  if(!isSecureContext || !navigator.gpu)throw Error('WebGPU非対応、またはHTTPS接続ではありません');
  const adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});if(!adapter)throw Error('WebGPUアダプターがありません');
  this.device=await adapter.requestDevice();if(this.failed){this.device.destroy();return;}
  this.device.lost.then(i=>{if(!this.failed)this.fail(i.message||i.reason);});this.device.addEventListener('uncapturederror',e=>{e.preventDefault();this.fail(e.error.message);});
  this.canvas.width=this.width;this.canvas.height=1080;this.context=this.canvas.getContext('webgpu');if(!this.context)throw Error('WebGPUキャンバスがありません');
  const format=navigator.gpu.getPreferredCanvasFormat();this.context.configure({device:this.device,format,alphaMode:'opaque'});
  this.display=await this.pipeline(shaders.display,'quad','display',false,format);
  if(this.kind===1){this.polygons=await this.pipeline(shaders.model1);this.tiles=await this.pipeline(shaders.model1Tile,'quad');this.snapshot=this.texture(GPUTextureUsage.COPY_DST);}
  else {this.polygons=await this.pipeline(this.kind===2?shaders.system22:shaders.model2,'vertex','fragment',this.kind===3);this.mix=await this.pipeline(this.kind===2?shaders.system22Mix:shaders.model2Mix,'quad');this.scene=this.texture();if(this.kind===3)this.depth=this.texture(0,'depth32float');}
  this.output=this.texture(GPUTextureUsage.COPY_SRC);
  if(this.failed)return;this.ready=true;this.onState(`GPU / WebGPU · ${this.width} × 1080`);this.log(`WebGPU: ${['','Model 1','System 22','Model 2'][this.kind]} ポリゴン描画が有効です`);
 }
 upload(index,heap,ptr,bytes,once=false){
  const size=Math.max(256,(bytes+3)&~3);let buffer=this.buffers[index];
  if(!buffer || buffer.size<size){buffer?.destroy();buffer=this.buffers[index]=this.device.createBuffer({size,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST});this.staticUploads.delete(index);}
  if(bytes && (!once || this.staticUploads.get(index)!==`${ptr}:${bytes}`)){this.device.queue.writeBuffer(buffer,0,heap,ptr,bytes);if(once)this.staticUploads.set(index,`${ptr}:${bytes}`);}return buffer;
 }
 uniform(index,cfg){if(!this.uniforms[index])this.uniforms[index]=this.device.createBuffer({size:48,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});const data=new ArrayBuffer(48);new Uint32Array(data).set(cfg.slice(0,8));new Float32Array(data)[8]=globalThis.polygonRacing?(globalThis.polygonRoll||0):0;this.device.queue.writeBuffer(this.uniforms[index],0,data);return this.uniforms[index];}
 group(pipeline,resources){return this.device.createBindGroup({layout:pipeline.getBindGroupLayout(0),entries:resources.map(([binding,value])=>({binding,resource:value instanceof GPUBuffer?{buffer:value}:value.createView()}))});}
 pass(encoder,target,load='clear',clear={r:0,g:0,b:0,a:0},depth=false){return encoder.beginRenderPass({colorAttachments:[{view:target.createView(),loadOp:load,storeOp:'store',clearValue:clear}],...(depth?{depthStencilAttachment:{view:this.depth.createView(),depthLoadOp:'clear',depthStoreOp:'discard',depthClearValue:1}}:{})});}
 draw(pass,pipeline,resources,count){pass.setPipeline(pipeline);pass.setBindGroup(0,this.group(pipeline,resources));pass.draw(count);}
 present(heap,parts,cfg){
  if(!this.ready)return false;
  try {
   const count=parts.length/2;for(let i=0;i<count;i++)this.upload(i,heap,parts[2*i],parts[2*i+1],this.kind===2 && [3,4,5,6,8].includes(i));
   const encoder=this.device.createCommandEncoder();cfg=[...cfg];cfg[0]=this.width;const uniform=this.uniform(0,cfg);const b=this.buffers;
   let vertices=0;
   if(this.kind===1){
    const scissor=p=>{if(!cfg[4])p.setScissorRect((this.width-1440)/2,0,1440,1080);};
    const background=this.uniform(1,[...cfg.slice(0,7),cfg[2]]);const hud=this.uniform(2,[...cfg.slice(0,7),cfg[3]]);const above=this.uniform(3,[...cfg.slice(0,7),1]);
    let p=this.pass(encoder,this.output);scissor(p);this.draw(p,this.tiles,[[0,b[2]],[7,background]],3);
    if(parts[1])this.draw(p,this.polygons,[[0,b[0]],[1,this.snapshot],[7,uniform]],parts[1]/28);
    this.draw(p,this.tiles,[[0,b[3]],[7,hud]],3);p.end();
    if(parts[3]){encoder.copyTextureToTexture({texture:this.output},{texture:this.snapshot},[this.width,1080]);p=this.pass(encoder,this.output,'load');scissor(p);this.draw(p,this.polygons,[[0,b[1]],[1,this.snapshot],[7,above]],parts[3]/28);p.end();}
    vertices=(parts[1]+parts[3])/28;
   }else{
    const c=cfg[6];const clear=this.kind===2?{r:((c>>>16)&255)/255,g:((c>>>8)&255)/255,b:(c&255)/255,a:0}:{r:0,g:0,b:0,a:0};
    let p=this.pass(encoder,this.scene,'clear',clear,this.kind===3);vertices=parts[1]/(this.kind===2?28:24);
    if(vertices)this.draw(p,this.polygons,[...b.slice(0,7).map((v,i)=>[i,v]),[7,uniform]],vertices);p.end();
    p=this.pass(encoder,this.output);const resources=this.kind===2?[[0,this.scene],[1,b[7]],[2,b[2]],[3,b[8]],[4,b[9]],[7,uniform]]:[[0,this.scene],[1,b[7]],[2,b[8]],[7,uniform]];
    this.draw(p,this.mix,resources,3);p.end();
   }
   const p=this.pass(encoder,this.context.getCurrentTexture());this.draw(p,this.display,[[0,this.output]],3);p.end();this.device.queue.submit([encoder.finish()]);
   this.canvas.hidden=false;this.frames++;if(vertices)this.polygonFrames++;this.vertices=vertices;return true;
  }catch(e){this.fail(e.message);return false;}
 }
}
