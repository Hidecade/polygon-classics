import {shadersForWidth} from './webgpu-shaders.mjs';

const HEIGHT=1080, HUD=496*480;
export async function createWebGPU({canvas,width=1920,log=()=>{},onState=()=>{},validate=false}={}) {
  const renderer=new StarbladeWebGPU(canvas,width,log,validate,onState);
  let timer;
  try {
    await Promise.race([renderer.initialize(),new Promise((_,reject)=>{
      timer=setTimeout(()=>reject(Error('GPU初期化がタイムアウトしました')),10000);
    })]);
  } catch(error) { renderer.fail(error.message); }
  finally { clearTimeout(timer); }
  return renderer;
}

class StarbladeWebGPU {
  constructor(canvas,width,log,validate,onState) {
    this.width=width;this.pixels=width*HEIGHT;this.wide={1440:496,1920:662,2580:890}[width]*480;this.rowBytes=Math.ceil(width*4/256)*256;
    this.onState=onState;this.canvas=canvas;this.log=log;this.validate=validate;
    this.ready=false;this.failed=false;this.serial=-1;this.pending=false;
    this.frames=0;this.polygonFrames=0;this.validations=[];this.lastValidation=-600;
    this.buffers=[];this.sizes=[];this.hide();
  }
  async initialize() {
    if(!globalThis.isSecureContext || !navigator.gpu) throw Error('WebGPU非対応、またはHTTPS接続ではありません');
    const adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});
    if(!adapter)throw Error('WebGPUアダプターがありません');
    if(this.failed)return;
    const device=await adapter.requestDevice();
    this.device=device;
    if(this.failed){device.destroy();return;}
    device.lost.then(info=>this.fail(`GPUデバイス切断: ${info.message || info.reason}`));
    device.addEventListener('uncapturederror',event=>{event.preventDefault();this.fail(event.error.message);});
    this.context=this.canvas.getContext('webgpu');
    if(!this.context)throw Error('WebGPUキャンバスを作成できません');
    this.canvas.width=this.width;this.canvas.height=HEIGHT;
    const format=navigator.gpu.getPreferredCanvasFormat();
    this.context.configure({device,format,alphaMode:'opaque'});
    const {compute:computeShader,display:displayShader}=shadersForWidth(this.width);
    const compute=device.createShaderModule({label:'StarBlade raster + composition',code:computeShader});
    const display=device.createShaderModule({label:'StarBlade HUD glow + presentation',code:displayShader});
    for(const module of [compute,display]) {
      const info=await module.getCompilationInfo();
      const errors=info.messages.filter(m=>m.type==='error');
      if(errors.length)throw Error(errors.map(m=>`${m.lineNum}:${m.linePos} ${m.message}`).join('\n'));
    }
    [this.compute,this.display]=await Promise.all([
      device.createComputePipelineAsync({layout:'auto',compute:{module:compute,entryPoint:'main'}}),
      device.createRenderPipelineAsync({layout:'auto',vertex:{module:display,entryPoint:'vertex'},fragment:{module:display,entryPoint:'fragment',targets:[{format}]}})
    ]);
    if(this.failed)return;
    this.output=device.createTexture({size:[this.width,HEIGHT],format:'rgba8unorm',usage:GPUTextureUsage.STORAGE_BINDING|GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_SRC});
    this.glow=device.createTexture({size:[496,480],format:'r32uint',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST});
    this.zeroGlow=new Uint32Array(HUD);
    this.uniform=device.createBuffer({size:16,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
    this.thresholds=new Uint32Array(16);
    let threshold=0x7fc0;for(let i=0;i<16;i++){this.thresholds[i]=threshold;threshold=Math.trunc(threshold/1.24);}
    this.displayGroup=device.createBindGroup({layout:this.display.getBindGroupLayout(0),entries:[
      {binding:0,resource:this.output.createView()},{binding:1,resource:this.glow.createView()}
    ]});
    this.ready=true;
    this.onState(`GPU / WebGPU · ${this.width} × 1080`);
    this.log(`WebGPU: ${this.width}×1080のポリゴン描画・画面合成が有効です`);
  }
  hide(){this.canvas.hidden=true;}
  fail(reason) {
    if(this.failed)return;
    this.failed=true;this.ready=false;this.reason=reason;this.hide();
    this.onState(`CPU描画 — ${reason}`);
    this.log(`WebGPU: ${reason}。CPU描画へ切り替えます。`);
    this.context?.unconfigure();this.device?.destroy();
  }
  dispose(){if(!this.failed){this.failed=true;this.ready=false;this.hide();this.context?.unconfigure();this.device?.destroy();}}
  wantsValidation(serial,spans) {
    return this.ready && this.validate && !this.pending && spans>0 && serial>=this.lastValidation+120 && this.validations.length<12;
  }
  upload(index,heap,offset,length,extra=0) {
    const size=Math.max(16,length+extra);
    if(!this.buffers[index] || this.sizes[index]<size) {
      this.buffers[index]?.destroy();
      const capacity=Math.ceil(size/65536)*65536;
      if(capacity>this.device.limits.maxStorageBufferBindingSize)throw Error('GPUバッファの上限を超えました');
      this.buffers[index]=this.device.createBuffer({size:capacity,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST});
      this.sizes[index]=capacity;this.computeGroup=null;
    }
    // Use the current heap for every call: Emscripten can grow/detach its memory.
    if(length)this.device.queue.writeBuffer(this.buffers[index],0,heap,offset,length);
  }
  present(heap,serial,spans,spanBytes,bins,binBytes,sprites,wideSprites,palette,config,glow,expected) {
    if(!this.ready)return false;
    this.config=config;
    try {
      // Paused screens can be hidden temporarily by MAME menus and shown again.
      // Present them again because the swap-chain texture is transient.
      if(this.serial!==serial) {
        this.upload(0,heap,spans,spanBytes);this.upload(1,heap,bins,binBytes);
        this.upload(2,heap,sprites,HUD*4);this.upload(3,heap,wideSprites,this.wide*2);
        this.upload(4,heap,palette,0x9000*4,64);
        this.device.queue.writeBuffer(this.buffers[4],0x9000*4,this.thresholds);
        this.device.queue.writeBuffer(this.uniform,0,new Uint32Array([config,0,0,0]));
        this.device.queue.writeTexture({texture:this.glow},glow?heap.subarray(glow,glow+HUD*4):this.zeroGlow,{bytesPerRow:496*4},[496,480]);
        if(!this.computeGroup)this.computeGroup=this.device.createBindGroup({layout:this.compute.getBindGroupLayout(0),entries:[
          ...this.buffers.map((buffer,binding)=>({binding,resource:{buffer}})),
          {binding:5,resource:{buffer:this.uniform}},{binding:6,resource:this.output.createView()}
        ]});
      }
      const encoder=this.device.createCommandEncoder();
      if(this.serial!==serial) {
        const pass=encoder.beginComputePass();pass.setPipeline(this.compute);pass.setBindGroup(0,this.computeGroup);pass.dispatchWorkgroups(Math.ceil(this.width/32),1080);pass.end();
      }
      const pass=encoder.beginRenderPass({colorAttachments:[{view:this.context.getCurrentTexture().createView(),loadOp:'clear',clearValue:{r:0,g:0,b:0,a:1},storeOp:'store'}]});
      pass.setPipeline(this.display);pass.setBindGroup(0,this.displayGroup);pass.draw(3);pass.end();
      let capture;
      if(expected && !this.pending) {
        capture=this.device.createBuffer({size:this.rowBytes*HEIGHT,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ});
        encoder.copyTextureToBuffer({texture:this.output},{buffer:capture,bytesPerRow:this.rowBytes},[this.width,HEIGHT]);
      }
      this.device.queue.submit([encoder.finish()]);
      if(this.serial!==serial){this.frames++;if(spanBytes)this.polygonFrames++;}
      this.serial=serial;this.canvas.hidden=false;
      if(capture) {
        this.pending=true;this.lastValidation=serial;
        // Diagnostic-only copies; normal frames never read GPU pixels back.
        const pens=new Uint16Array(heap.buffer,heap.byteOffset+expected,this.pixels).slice();
        const colors=new Uint32Array(heap.buffer,heap.byteOffset+palette,0x9000).slice();
        this.check(capture,pens,colors,serial,spanBytes/16);
      }
      return true;
    } catch(error) {this.fail(error.message);return false;}
  }
  async check(buffer,pens,colors,serial,spans) {
    try {
      await buffer.mapAsync(GPUMapMode.READ);
      const bytes=new Uint8Array(buffer.getMappedRange());let differences=0;const samples=[];
      for(let i=0;i<this.pixels;i++) {
        const rgb=colors[pens[i]];const pixel=Math.floor(i/this.width)*this.rowBytes+(i%this.width)*4;
        if(bytes[pixel]!==((rgb>>>16)&255)||bytes[pixel+1]!==((rgb>>>8)&255)||bytes[pixel+2]!==(rgb&255)||bytes[pixel+3]!==255){differences++;if(samples.length<3)samples.push({x:i%this.width,y:Math.floor(i/this.width),expected:rgb,actual:Array.from(bytes.subarray(pixel,pixel+4))});}
      }
      this.validations.push({serial,spans,pixels:this.pixels,differences,...(differences?{config:this.config,samples}:{})});
      this.log(`WEBGPU_VALIDATE frame=${serial} spans=${spans} pixels=${this.pixels} differences=${differences}`);
      if(differences)this.fail('CPU参照画像との不一致を検出しました');
    } catch(error){if(!this.failed)this.fail(`GPU検証: ${error.message}`);}
    finally{buffer.destroy();this.pending=false;}
  }
}
