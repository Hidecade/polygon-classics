// license:BSD-3-Clause
#pragma once
static constexpr char starblade_compute_hlsl[] = R"HLSL(
Texture2D<uint4> spans : register(t0);
Texture2D<uint> bins : register(t1);
Texture2D<uint> sprites : register(t2);
Texture2D<uint> wide_sprites : register(t3);
Texture2D<uint> palette : register(t4);
RWTexture2D<float4> output_image : register(u5);
RWTexture2D<float4> wire_mask : register(u6);
uint word(uint i) { return bins.Load(int3(i % 2048, i / 2048, 0)); }
bool mix(inout uint dest, uint raw, uint priority) {
    if (raw == 65535 || ((raw >> 12) & 3) != priority) return false;
    uint src = (raw & 4095) ^ 3840;
    if ((src & 255) == 255) { if (dest != 255) return false; dest = src; }
    else if ((src & 255) < 2) dest = dest != 255 ? (((src & 1) != 0 ? 24576 : 16384) | (dest & 8191)) : src;
    else dest = 4096 | src;
    return true;
}
float4 sample_pixel(uint x, uint y, out float4 mask) {
    uint config = palette.Load(int3(0,144,0));
    uint output_width = palette.Load(int3(17,144,0));
    int auxiliary_base = int(palette.Load(int3(18,144,0)));
    uint auxiliary_width = palette.Load(int3(19,144,0));
    uint tiles_x = (output_width+31)/32;
    uint tile = (y * tiles_x + x / 32) * 2;
    uint begin = word(tile), count = word(tile + 1);
    uint depth = 32768, pen = 0, edge_depth = 32768, edge_pen = 0, edge_coverage = 0;
    [loop] for (uint j = 0; j < count; ++j) {
        uint index = word(begin + j);
        uint4 s = spans.Load(int3(index % 2048, index / 2048, 0));
        uint coverage = (s.z >> 16) & 255;
        if ((s.z & 0xc0000000) == 0xc0000000) {
            uint4 endpoint = spans.Load(int3((index + 1) % 2048, (index + 1) / 2048, 0));
            precise float2 origin = asfloat(s.xy);
            precise float2 delta = asfloat(endpoint.xy) - origin;
            precise float2 pixel = float2(x, y) + 0.5 - origin;
            precise float length2 = dot(delta, delta);
            precise float along = dot(pixel, delta);
            precise float cross = pixel.x * delta.y - pixel.y * delta.x;
            precise float2 tail = pixel - delta;
            precise float distance2 = along <= 0.0 ? dot(pixel,pixel) : along >= length2 ? dot(tail,tail) : cross * cross / length2;
            if (along > 0.0 && along < length2) {
                if (delta.y == 0.0) distance2 = pixel.y * pixel.y;
                else if (delta.x == 0.0) distance2 = pixel.x * pixel.x;
            }
            coverage = uint(floor(255.0 * saturate(asfloat(endpoint.z) * 0.5 + 0.5 - sqrt(distance2)) + 0.5));
            if (coverage == 0) continue;
        } else if (x < s.x || x >= s.y) continue;
        if ((s.z & 0x80000000) != 0) {
            if (s.w < edge_depth || (s.w == edge_depth && coverage > edge_coverage)) {
                edge_depth = s.w; edge_pen = s.z & 65535; edge_coverage = coverage;
            }
        } else if (s.w < depth) { depth = s.w; pen = s.z; }
    }
    bool hiddenEdge = palette.Load(int3(23, 144, 0)) != 0 && edge_pen != 0 && edge_depth > depth;
    if (edge_depth <= depth || hiddenEdge) pen = edge_pen;
    uint sy = y * 480 / 1080;
    uint wx = uint(clamp((int(x) - auxiliary_base) * 496 / 1440, 0, int(auxiliary_width) - 1));
    uint packed_scene = wide_sprites.Load(int3(wx / 2, sy, 0));
    uint scene = (packed_scene >> ((wx & 1) * 16)) & 65535;
    uint left = (output_width - 1440) / 2;
    bool centre = x >= left && x < left + 1440;
    uint sx = x < left ? 0 : x >= left + 1440 ? 495 : (x - left) * 496 / 1440;
    uint sprite = sprites.Load(int3(sx, sy, 0)), raw = sprite & 65535, effect = sprite >> 16;
    uint dest = 255;
    // Transparent wireframe faces still occlude background-priority stars.
    if (depth == 32768) mix(dest, scene, 2);
    uint under = dest;
    bool edge_visible = (edge_depth <= depth || hiddenEdge) && edge_pen != 0;
    bool vectorBlocked = false;
    if (pen != 0) dest = pen;
    {
        uint mixed = dest;
        if (mix(mixed, scene, 0)) {
            uint threshold = palette.Load(int3(1 + ((mixed >> 8) & 15), 144, 0));
            if ((config & 1) == 0 || (((mixed & 20480) != 0) && threshold <= depth) || mixed < 4096) { dest = mixed; edge_visible = false; vectorBlocked = true; }
        }
    }
    {
        uint mixed = under;
        if (mix(mixed, scene, 0)) {
            uint threshold = palette.Load(int3(1 + ((mixed >> 8) & 15), 144, 0));
            if ((config & 1) == 0 || (((mixed & 20480) != 0) && threshold <= depth) || mixed < 4096) under = mixed;
        }
    }
    if (mix(dest, scene, 3)) { edge_visible = false; vectorBlocked = true; }
    mix(under, scene, 3);
    if (centre || effect == 2) {
        bool covered = mix(dest, raw, 3);
        if (covered) { edge_visible = false; vectorBlocked = true; }
        if (covered && (config & 2) != 0 && effect == 1 && dest >= 4096 && dest < 8192
            && y == ((sy + 1) * 1080 + 479) / 480 - 1) dest = 32768 | (dest & 4095);
        covered = mix(under, raw, 3);
        if (covered && (config & 2) != 0 && effect == 1 && under >= 4096 && under < 8192
            && y == ((sy + 1) * 1080 + 479) / 480 - 1) under = 32768 | (under & 4095);
    }
    uint color = palette.Load(int3(dest % 256, dest / 256, 0));
    float3 rgb = float3((color >> 16) & 255, (color >> 8) & 255, color & 255);
    if (edge_visible) {
        uint brightness = palette.Load(int3(22, 144, 0));
        float gain = brightness == 1 ? 80.0 : brightness == 3 ? 125.0 : brightness == 0 ? 160.0 : 100.0;
        rgb = min(255.0, floor((rgb * gain + 50.0) / 100.0));
        if (hiddenEdge) rgb = floor(rgb * 0.5 + 0.5);
    }
    if (edge_visible && edge_coverage < 255) {
        uint below = palette.Load(int3(under % 256, under / 256, 0));
        float3 under_rgb = float3((below >> 16) & 255, (below >> 8) & 255, below & 255);
        rgb = lerp(under_rgb, rgb, float(edge_coverage) / 255.0);
    }
    float3 wire = edge_visible && edge_coverage != 0
        ? float3((color >> 16) & 255, (color >> 8) & 255, color & 255) * (float(edge_coverage) / 255.0) * (hiddenEdge ? 0.5 : 1.0)
        : float3(0, 0, 0);
    mask = float4(wire, vectorBlocked ? 255.0 : (edge_visible ? float(edge_coverage) * (254.0 / 255.0) : 0)) / 255.0;
    return float4(rgb, 255) / 255.0;
}
[numthreads(32,1,1)]
void main(uint3 pos : SV_DispatchThreadID) {
    uint width=palette.Load(int3(17,144,0));
    uint x=pos.x,y=pos.y;
    if(x>=width || y>=1080) return;
    bool crop=palette.Load(int3(26,144,0))!=0;
    uint sx=crop?(width*209/2+x*871)/1080:x;
    uint sy=crop?209+y*871/1080:y;
    float4 mask;
    float4 color=sample_pixel(sx,sy,mask);
    uint left=width-744;
    if(crop && palette.Load(int3(27,144,0))!=0 && x>=left && x<left+720 && y>=24 && y<129) {
        float4 statusMask;
        float4 status=sample_pixel((width-1440)/2+(x-left)*2,208-(y-24)*2,statusMask);
        if(max(status.r,max(status.g,status.b))>24.5/255.0) color=status;
    }
    wire_mask[pos.xy]=mask;
    output_image[pos.xy]=color;
}

)HLSL";
