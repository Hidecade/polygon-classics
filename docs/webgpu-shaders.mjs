// MAME-compatible integer rasterisation. Each invocation owns one pixel; span
// order and strict depth comparison match the original CPU renderer exactly.
export const computeShader = /* wgsl */`
@group(0) @binding(0) var<storage, read> spans: array<vec4u>;
@group(0) @binding(1) var<storage, read> bins: array<u32>;
@group(0) @binding(2) var<storage, read> sprites: array<u32>;
@group(0) @binding(3) var<storage, read> wideSprites: array<u32>;
@group(0) @binding(4) var<storage, read> palette: array<u32>;
@group(0) @binding(5) var<uniform> config: vec4u;
@group(0) @binding(6) var outputImage: texture_storage_2d<rgba8unorm, write>;
fn mixPen(dest: u32, raw: u32, priority: u32) -> vec2u {
  if (raw == 65535u || ((raw >> 12u) & 3u) != priority) { return vec2u(dest, 0u); }
  let src = (raw & 4095u) ^ 3840u;
  if ((src & 255u) == 255u) {
    if (dest != 255u) { return vec2u(dest, 0u); }
    return vec2u(src, 1u);
  }
  if ((src & 255u) < 2u) {
    if (dest == 255u) { return vec2u(src, 1u); }
    return vec2u(select(16384u, 24576u, (src & 1u) != 0u) | (dest & 8191u), 1u);
  }
  return vec2u(4096u | src, 1u);
}
fn shade(pos: vec2u) -> vec4f {
  let x = pos.x; let y = pos.y; let i = y * 1920u + x;
  let tile = (y * 60u + x / 32u) * 2u;
  let begin = bins[tile]; let count = bins[tile + 1u];
  var depth = 32768u; var pen = 0u; var edgeDepth = 32768u; var edgePen = 0u; var edgeCoverage = 0u;
  for (var j = 0u; j < count; j++) {
    let s = spans[bins[begin + j]];
    let coverage = (s.z >> 16u) & 255u;
    if (x < s.x || x >= s.y) { continue; }
    if ((s.z & 0x80000000u) != 0u) {
      if (s.w < edgeDepth || (s.w == edgeDepth && coverage > edgeCoverage)) {
        edgeDepth = s.w; edgePen = s.z & 65535u; edgeCoverage = coverage;
      }
    } else if (s.w < depth) { depth = s.w; pen = s.z; }
  }
  if (edgeDepth <= depth) { pen = edgePen; }
  let sy = y * 480u / 1080u;
  let wx = (x + 1u) * 496u / 1440u;
  let wi = sy * 662u + wx;
  let scene = (wideSprites[wi / 2u] >> ((wi & 1u) * 16u)) & 65535u;
  let centre = x >= 240u && x < 1680u;
  var sx = 0u;
  if (x >= 1680u) { sx = 495u; } else if (x >= 240u) { sx = (x - 240u) * 496u / 1440u; }
  let sprite = sprites[sy * 496u + sx]; let raw = sprite & 65535u; let effect = sprite >> 16u;
  // Transparent wireframe faces still occlude background-priority stars.
  var dest = 255u;
  if (depth == 32768u) { dest = mixPen(dest, scene, 2u).x; }
  var under = dest; var edgeVisible = edgeDepth <= depth && edgePen != 0u;
  if (pen != 0u) { dest = pen; }
  {
    let mixed = mixPen(dest, scene, 0u);
    let threshold = palette[36864u + ((mixed.x >> 8u) & 15u)];
    if (mixed.y != 0u && ((config.x & 1u) == 0u || ((mixed.x & 20480u) != 0u && threshold <= depth) || mixed.x < 4096u)) { dest = mixed.x; edgeVisible = false; }
  }
  {
    let mixed = mixPen(under, scene, 0u);
    let threshold = palette[36864u + ((mixed.x >> 8u) & 15u)];
    if (mixed.y != 0u && ((config.x & 1u) == 0u || ((mixed.x & 20480u) != 0u && threshold <= depth) || mixed.x < 4096u)) { under = mixed.x; }
  }
  {
    let mixed = mixPen(dest, scene, 3u); dest = mixed.x;
    if (mixed.y != 0u) { edgeVisible = false; }
  }
  under = mixPen(under, scene, 3u).x;
  if (centre || effect == 2u) {
    let mixed = mixPen(dest, raw, 3u); dest = mixed.x;
    if (mixed.y != 0u) { edgeVisible = false; }
    if (mixed.y != 0u && (config.x & 2u) != 0u && effect == 1u && dest >= 4096u && dest < 8192u
        && y == ((sy + 1u) * 1080u + 479u) / 480u - 1u) { dest = 32768u | (dest & 4095u); }
    let below = mixPen(under, raw, 3u); under = below.x;
    if (below.y != 0u && (config.x & 2u) != 0u && effect == 1u && under >= 4096u && under < 8192u
        && y == ((sy + 1u) * 1080u + 479u) / 480u - 1u) { under = 32768u | (under & 4095u); }
  }
  let rgb = palette[dest];
  var color = vec3f(f32((rgb >> 16u) & 255u), f32((rgb >> 8u) & 255u), f32(rgb & 255u));
  if (edgeVisible && edgeCoverage < 255u) {
    let base = palette[under];
    let baseColor = vec3f(f32((base >> 16u) & 255u), f32((base >> 8u) & 255u), f32(base & 255u));
    color = mix(baseColor, color, f32(edgeCoverage) / 255.0);
  }
  return vec4f(color,255.0) / 255.0;
}
@compute @workgroup_size(32, 1, 1)
fn main(@builtin(global_invocation_id) pos:vec3u) {
 var source=pos.xy;
 if((config.x&4u)!=0u){source=vec2u((1920u*209u/2u+pos.x*871u)/1080u,209u+pos.y*871u/1080u);}
 var color=shade(source);
 if((config.x&12u)==12u && pos.x>=1176u && pos.x<1896u && pos.y>=24u && pos.y<129u){
  let status=shade(vec2u(240u+(pos.x-1176u)*2u,208u-(pos.y-24u)*2u));
  if(any(round(status.rgb*255.)>vec3f(24.))){color=status;}
 }
 textureStore(outputImage,pos.xy,color);
}
`;

// Presentation keeps the 496x480 HUD glow in the central 4:3 area. The game
// image is already 1080p; only the small glow texture is interpolated.
export const displayShader = /* wgsl */`
@group(0) @binding(0) var picture: texture_2d<f32>;
@group(0) @binding(1) var glow: texture_2d<u32>;
@vertex fn vertex(@builtin(vertex_index) id: u32) -> @builtin(position) vec4f {
  let positions = array<vec2f,3>(vec2f(-1,-1), vec2f(3,-1), vec2f(-1,3));
  return vec4f(positions[id],0,1);
}
fn glowPixel(p: vec2i) -> vec3f {
  let rgb = textureLoad(glow, clamp(p,vec2i(0),vec2i(495,479)),0).x;
  return vec3f(f32((rgb >> 16u) & 255u), f32((rgb >> 8u) & 255u), f32(rgb & 255u)) / 255.0;
}
@fragment fn fragment(@builtin(position) p: vec4f) -> @location(0) vec4f {
  var rgb = textureLoad(picture, vec2i(p.xy),0).rgb;
  if (p.x >= 240.0 && p.x < 1680.0) {
    let uv = (p.xy - vec2f(240,0)) * vec2f(496.0/1440.0,480.0/1080.0) - vec2f(0.5);
    let a = vec2i(floor(uv)); let t = fract(uv);
    rgb += mix(mix(glowPixel(a),glowPixel(a+vec2i(1,0)),t.x),mix(glowPixel(a+vec2i(0,1)),glowPixel(a+vec2i(1,1)),t.x),t.y);
  }
  return vec4f(min(rgb,vec3f(1)),1);
}
`;

// Specialise the existing rasteriser for the selected native capture width.
export function shadersForWidth(width) {
 const margin=(width-1440)/2, auxiliary={1440:496,1920:662,2580:890}[width];
 const base=margin-Math.floor(((auxiliary-496)/2*1440+248)/496);
 let compute=computeShader.replaceAll('1920u',`${width}u`).replaceAll('60u',`${Math.ceil(width/32)}u`)
  .replaceAll('662u',`${auxiliary}u`).replaceAll('240u',`${margin}u`).replaceAll('1680u',`${margin+1440}u`)
  .replace('(x + 1u)',`u32(max(0,i32(x)-(${base})))`)
  .replace('var source=pos.xy;',`if(pos.x>=${width}u){return;} var source=pos.xy;`)
  .replaceAll('1176u',`${width-744}u`).replaceAll('1896u',`${width-24}u`);
 let display=displayShader.replaceAll('240.0',`${margin}.0`).replaceAll('1680.0',`${margin+1440}.0`).replace('vec2f(240,0)',`vec2f(${margin},0)`);
 return {compute,display};
}
