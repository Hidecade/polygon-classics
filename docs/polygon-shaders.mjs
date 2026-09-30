// BSD-3-Clause. Browser counterparts of the existing Model1/Model2/System22 Metal shaders.
const config=`struct Config {width:u32,tileWidth:u32,margin:u32,hudMode:u32,panorama:u32,materials:u32,background:u32,pad:u32,motion:vec4f};
@group(0) @binding(7) var<uniform> cfg:Config;
fn rotatePoint(p:vec2f,angle:f32)->vec2f{let d=p-vec2f(f32(cfg.width)*.5,540.);let c=cos(angle);let s=sin(angle);return vec2f(c*d.x-s*d.y,s*d.x+c*d.y)+vec2f(f32(cfg.width)*.5,540.);}
fn projected(p:vec2f,z:f32)->vec4f{let q=rotatePoint(p,cfg.motion.x);return vec4f(q.x*2./f32(cfg.width)-1.,1.-q.y/540.,z,1.);}
fn rgb(c:u32)->vec3f{return vec3f(f32((c>>16u)&255u),f32((c>>8u)&255u),f32(c&255u))/255.;}
fn rgbu(c:u32)->vec3u{return vec3u((c>>16u)&255u,(c>>8u)&255u,c&255u);}`;
export const quad=`@vertex fn quad(@builtin(vertex_index) id:u32)->@builtin(position) vec4f {
let p=array<vec2f,3>(vec2f(-1.,-1.),vec2f(3.,-1.),vec2f(-1.,3.));return vec4f(p[id],0.,1.);}`;
export const display=quad+`@group(0) @binding(0) var picture:texture_2d<f32>;
@fragment fn display(@builtin(position) p:vec4f)->@location(0) vec4f{return textureLoad(picture,vec2i(p.xy),0);}`;
export const model1=config+`
struct Vertex{x:f32,y:f32,color:u32,left:f32,top:f32,right:f32,bottom:f32};
@group(0) @binding(0) var<storage,read> vertices:array<Vertex>;
@group(0) @binding(1) var snapshot:texture_2d<f32>;
struct Out{@builtin(position) position:vec4f,@location(0) @interpolate(flat) color:u32,@location(1) @interpolate(flat) clip:vec4f,@location(2) source:vec2f};
@vertex fn vertex(@builtin(vertex_index) id:u32)->Out {
let a=vertices[id];var o:Out;o.position=projected(vec2f(a.x,a.y),0.);o.color=a.color;o.clip=vec4f(a.left,a.top,a.right,a.bottom);o.source=vec2f(a.x,a.y);return o;}
@fragment fn fragment(v:Out)->@location(0) vec4f {
if(v.source.x<v.clip.x || v.source.y<v.clip.y || v.source.x>=v.clip.z || v.source.y>=v.clip.w){discard;}
if((v.color&0x1000000u)!=0u && ((u32(v.position.x)^u32(v.position.y))&1u)!=0u){discard;}
if(cfg.pad!=0u && any(textureLoad(snapshot,vec2i(v.position.xy),0).rgb>vec3f(8./255.))){discard;}
return vec4f(rgb(v.color),1.);}`;
export const model1Tile=config+quad+`
@group(0) @binding(0) var<storage,read> pixels:array<u32>;
@fragment fn fragment(@builtin(position) p:vec4f)->@location(0) vec4f {
let source=rotatePoint(p.xy,-cfg.motion.x);
let mode=cfg.pad;let hud=mode==1u || mode==4u;
var x=(source.x-(f32(cfg.width)-1440.)*.5)/1440.;var y=source.y/1080.;
if((mode==1u || mode==2u)&&(x<0. || x>=1. || y<0. || y>=1.)){discard;}
if(mode==3u || mode==4u){x=clamp(x,.5/496.,495.5/496.);}
var w=496u;var h=384u;
if(!hud){w=cfg.tileWidth;h=arrayLength(&pixels)/w;x=(x*496.+f32(w-496u)*.5)/f32(w);y=(y*384.+f32(h-384u)*.5)/f32(h);}
let ix=u32(clamp(x*f32(w),0.,f32(w-1u)));let iy=u32(clamp(y*f32(h),0.,f32(h-1u)));
let c=pixels[iy*w+ix];if(hud && (c>>24u)==0u){discard;}return vec4f(rgb(c),1.);}`;
export const system22=config+`
struct Vertex{x:f32,y:f32,z:f32,u:f32,v:f32,i:f32,material:u32};
struct Material{left:f32,top:f32,right:f32,bottom:f32,palette:u32,bank:u32,cmode:u32,priority:u32,flags:u32,fogfactor:u32,fogcolor:u32,pad:u32};
@group(0) @binding(0) var<storage,read> vertices:array<Vertex>;
@group(0) @binding(1) var<storage,read> materials:array<Material>;
@group(0) @binding(2) var<storage,read> palette:array<u32>;
@group(0) @binding(3) var<storage,read> tiles:array<u32>;
@group(0) @binding(4) var<storage,read> attributes:array<u32>;
@group(0) @binding(5) var<storage,read> texels:array<u32>;
@group(0) @binding(6) var<storage,read> lookup:array<u32>;
struct Out{@builtin(position) position:vec4f,@location(0) @interpolate(linear) parameters:vec4f,@location(1) @interpolate(linear) source:vec2f,@location(2) @interpolate(flat) material:u32};
@vertex fn vertex(@builtin(vertex_index) id:u32)->Out {
let v=vertices[id];var o:Out;o.position=projected(vec2f(v.x,v.y),0.);o.parameters=vec4f(v.z,v.u,v.v,v.i);o.source=vec2f(v.x,v.y);o.material=v.material;return o;}
@fragment fn fragment(v:Out)->@location(0) vec4f {
let m=materials[v.material];if(v.source.x<m.left || v.source.x>=m.right || v.source.y<m.top || v.source.y>=m.bottom || v.parameters.x<=0.){discard;}
let inverse=1./v.parameters.x;var pen=0u;var base=m.palette;var mask=255u;var shift=0u;
if((m.cmode&4u)!=0u){base+=0xecu+((m.cmode&8u)<<1u);mask=3u;shift=2u*(~m.cmode&3u);}else if((m.cmode&2u)!=0u){base+=0xe0u+((m.cmode&8u)<<1u);mask=15u;shift=4u*(~m.cmode&1u);}
if((m.flags&1u)!=0u){
let tx=u32(i32(v.parameters.y*inverse))&0xfffu;let ty=(u32(i32(v.parameters.z*inverse))&0xfffu)|(m.bank*0x1000u);
let offset=((ty<<4u)&0xfff00u)|(tx>>4u);let tile=(tiles[offset/2u]>>((offset&1u)*16u))&65535u;
let attr=(attributes[offset/4u]>>((offset&3u)*8u))&255u;let li=(attr<<8u)|((ty<<4u)&0xf0u)|(tx&15u);
let address=(tile<<8u)|((lookup[li/4u]>>((li&3u)*8u))&255u);pen=(texels[address/4u]>>((address&3u)*8u))&255u;}
var c=rgbu(palette[(base+((pen>>shift)&mask))&0x7fffu]);
if(m.fogfactor!=0u){c=(c*(255u-m.fogfactor)+rgbu(m.fogcolor)*(1u+m.fogfactor))>>vec3u(8u);}
if((m.flags&2u)!=0u){c=vec3u(clamp((vec3i(c)*(i32(v.parameters.w*inverse)*4))>>vec3u(8u),vec3i(0),vec3i(255)));}
return vec4f(vec3f(c)/255.,f32(m.priority&1u));}`;
export const system22Mix=config+quad+`
@group(0) @binding(0) var scene:texture_2d<f32>;
@group(0) @binding(1) var<storage,read> hud:array<u32>;
@group(0) @binding(2) var<storage,read> palette:array<u32>;
@group(0) @binding(3) var<storage,read> gamma:array<u32>;
@group(0) @binding(4) var<storage,read> mixcfg:array<u32>;
fn gam(i:u32)->u32{return (gamma[i/4u]>>((i&3u)*8u))&255u;}
@fragment fn fragment(@builtin(position) p:vec4f)->@location(0) vec4f {
let color=textureLoad(scene,vec2i(p.xy),0);var c=vec3u(round(color.rgb*255.));let left=(cfg.width-1440u)/2u;let source=rotatePoint(p.xy,-cfg.motion.x);let x=u32(max(0.,source.x));let y=u32(max(0.,source.y));
if(source.x>=0. && source.y>=0. && source.y<1080. && x>=left && x<left+1440u && color.a<.5){let index=hud[(y*480u/1080u)*640u+(x-left)*640u/1440u];if((index&0x10000u)!=0u){let pen=index&255u;if(mixcfg[3]!=0u && pen>=252u && pen<=254u){c=(c*rgbu(mixcfg[4u+pen-252u]))>>vec3u(8u);}else{c=rgbu(palette[index&0x7fffu]);}}}
let fade=vec3u(mixcfg[0],mixcfg[1],mixcfg[2]);c+=select(vec3u(0),vec3u(1),(c==vec3u(0))&(fade>vec3u(256)));c=min((c*fade)>>vec3u(8u),vec3u(255));
return vec4f(vec3f(f32(gam(c.r)),f32(gam(256u+c.g)),f32(gam(512u+c.b)))/255.,1.);}`;
export const model2=config+`
struct Vertex{x:f32,y:f32,ooz:f32,uoz:f32,voz:f32,material:u32};
struct Material{left:f32,top:f32,right:f32,bottom:f32,flags:u32,color:u32,luma:u32,lumabase:u32,texwidth:u32,texheight:u32,texx:u32,texy:u32,sheet:u32,wrapx:u32,wrapy:u32,mirrorx:u32,mirrory:u32,utex:u32,utexminlod:u32,utexx:u32,utexy:u32,texlod:i32};
@group(0) @binding(0) var<storage,read> vertices:array<Vertex>;
@group(0) @binding(1) var<storage,read> materials:array<Material>;
@group(0) @binding(2) var<storage,read> textures:array<u32>;
@group(0) @binding(3) var<storage,read> palette:array<u32>;
@group(0) @binding(4) var<storage,read> xlat:array<u32>;
@group(0) @binding(5) var<storage,read> luma:array<u32>;
@group(0) @binding(6) var<storage,read> gamma:array<u32>;
struct Out{@builtin(position) position:vec4f,@location(0) uv:vec3f,@location(1) source:vec2f,@location(2) @interpolate(flat) material:u32};
@vertex fn vertex(@builtin(vertex_index) id:u32)->Out {let a=vertices[id];var o:Out;
let x=(f32(cfg.width)-1440.)*.5+a.x*(1440./496.);o.position=projected(vec2f(x,a.y*(1080./384.)),(f32(a.material)+1.)/(f32(cfg.materials)+1.));o.uv=vec3f(a.ooz,a.uoz,a.voz);o.source=vec2f(a.x,a.y);o.material=a.material;return o;}
fn lerp2(x:u32,y:u32,a:u32)->u32{return (x+(((y-x)*a)>>8u))&0x00ff00ffu;}
fn texel(sheet:u32,bx:u32,by:u32,x:u32,y:u32)->u32 {var x2=bx+x;var y2=by+y;if(x2>=1024u){x2-=1024u;y2^=1024u;}let offset=(y2/2u)*512u+x2/2u;var t=textures[sheet*0x40000u+((offset>>1u)&0x3ffffu)];if((offset&1u)!=0u){t>>=16u;}if((y&1u)==0u){t>>=8u;}if((x&1u)==0u){t>>=4u;}return t&15u;}
fn bilinear(m:Material,level:i32,uu:i32,vv:i32)->u32 {
var u=uu;var v=vv;var w:u32;var h:u32;var x:u32;var y:u32;var sheet:u32;
if(level<0){w=128u;h=128u;x=m.utexx;y=m.utexy;sheet=m.sheet^1u;u=i32(u32(u)<<(1u<<m.utexminlod));v=i32(u32(v)<<(1u<<m.utexminlod));}
else{w=m.texwidth>>u32(level);h=m.texheight>>u32(level);x=((m.texx-2048u)>>u32(level))&2047u;y=((m.texy-1024u)>>u32(level))&1023u;sheet=m.sheet^(u32(level)&1u);u>>=u32(level);v>>=u32(level);}
if(m.mirrorx!=0u && (u&i32(w<<8u))!=0){u=~u;}if(m.mirrory!=0u && (v&i32(h<<8u))!=0){v=~v;}u-=128;v-=128;
var uf=u32(u)&255u;var vf=u32(v)&255u;var u0=u32(u>>8u)&(w-1u);var u1=(u0+1u)&(w-1u);var v0=u32(v>>8u)&(h-1u);var v1=(v0+1u)&(h-1u);
if(m.wrapx==0u && u1==0u){if(uf>=128u){u0=u1;u1++;uf=0u;}else{u1=u0;u0--;uf=256u;}}
if(m.wrapy==0u && v1==0u){if(vf>=128u){v0=0u;v1++;vf=0u;}else{v1=v0;v0--;vf=256u;}}
var a=texel(sheet,x,y,u0,v0)<<4u;var b=texel(sheet,x,y,u1,v0)<<4u;var d=texel(sheet,x,y,u0,v1)<<4u;var e=texel(sheet,x,y,u1,v1)<<4u;let trans=(m.flags&1u)!=0u;
if(trans){if(a!=240u){a|=0x00800000u;}if(b!=240u){b|=0x00800000u;}if(d!=240u){d|=0x00800000u;}if(e!=240u){e|=0x00800000u;}if(a==240u){a=b&255u;}if(b==240u){b=a&255u;}if(d==240u){d=e&255u;}if(e==240u){e=d&255u;}}
var ab=lerp2(a,b,uf);var de=lerp2(d,e,uf);if(trans){if(ab==240u){ab=de&255u;}if(de==240u){de=ab&255u;}}return lerp2(ab,de,vf);}
fn pal(i:u32)->u32{return (palette[i/2u]>>((i&1u)*16u))&65535u;}
fn xl(i:u32)->u32{return (xlat[i/2u]>>((i&1u)*16u))&255u;}
fn gam(i:u32)->u32{return (gamma[i/4u]>>((i&3u)*8u))&255u;}
@fragment fn fragment(v:Out)->@location(0) vec4f {
let m=materials[v.material];if(v.source.x<m.left || v.source.x>=m.right || v.source.y<m.top || v.source.y>=m.bottom){discard;}
if((m.flags&4u)!=0u && ((i32(floor(v.source.x))^i32(floor(v.source.y)))&1)==0){discard;}
var lum=m.luma>>2u;if((m.flags&2u)!=0u){let z=1./v.uv.x;let mml=-m.texlod+i32(floor(log2(max(z,1e-20))*256.));let maxLevel=30-i32(countLeadingZeros(min(m.texwidth,m.texheight)));let level=clamp(mml>>7u,0,maxLevel);let u=i32(v.uv.y*z*256.);let vv=i32(v.uv.z*z*256.);var t=bilinear(m,level,u,vv);
if(mml>0 && level<maxLevel){t=lerp2(t,bilinear(m,level+1,u,vv),u32((mml&127)<<1u));}else if(m.utex!=0u && mml<0){t=lerp2(t,bilinear(m,-1,u,vv),u32(min((-mml)>>m.utexminlod,127)));}
if((m.flags&1u)!=0u){if(t<0x00400000u){discard;}t&=255u;}let li=(m.lumabase+(t>>1u))&32767u;lum=min(((luma[li/4u]>>((li&3u)*8u))&255u)*m.luma/256u,63u);
}else if((m.flags&1u)!=0u){discard;}
let c=pal(0x1000u+m.color);let r=gam(xl(((c&31u)<<8u)+lum));let g=gam(xl(0x2000u+(((c>>5u)&31u)<<8u)+lum));let b=gam(xl(0x4000u+(((c>>10u)&31u)<<8u)+lum));return vec4f(vec3f(f32(r),f32(g),f32(b))/255.,1.);}`;
export const model2Mix=config+quad+`
@group(0) @binding(0) var scene:texture_2d<f32>;
@group(0) @binding(1) var<storage,read> background:array<u32>;
@group(0) @binding(2) var<storage,read> hud:array<u32>;
@fragment fn fragment(@builtin(position) p:vec4f)->@location(0) vec4f {
let source=rotatePoint(p.xy,-cfg.motion.x);let nx=i32(floor((source.x-(f32(cfg.width)-1440.)*.5)*(496./1440.)));let ny=u32(clamp(source.y*(384./1080.),0.,383.));let bx=u32(clamp(nx+i32(cfg.margin),0,i32(cfg.tileWidth)-1));var c=vec4f(rgb(background[ny*cfg.tileWidth+bx]),1.);let poly=textureLoad(scene,vec2i(p.xy),0);if(poly.a>0.){c=poly;}
if(source.y>=0. && source.y<1080. && nx>=0 && nx<496){let h=hud[ny*496u+u32(nx)];if((h>>24u)!=0u){c=vec4f(rgb(h),1.);}}return c;}`;
