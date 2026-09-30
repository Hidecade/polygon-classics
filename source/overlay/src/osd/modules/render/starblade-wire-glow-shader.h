// license:BSD-3-Clause
#pragma once
static constexpr char starblade_wire_glow_horizontal_hlsl[] = R"HLSL(
Texture2D<float4> wire_mask : register(t0);
RWTexture2D<float4> blurred_wire : register(u1);
Texture2D<uint> palette : register(t2);
[numthreads(32, 1, 1)]
void main(uint3 pos : SV_DispatchThreadID) {
    uint width, height; blurred_wire.GetDimensions(width, height);
    int x = pos.x, y = pos.y;
    if (x >= int(width) || y >= 540) return;
    uint source_width, source_height; wire_mask.GetDimensions(source_width, source_height);
    int radius = int(palette.Load(int3(20, 144, 0)));
    float3 sum = 0;
    float total = 0;
    [unroll] for (int dx = -5; dx <= 5; ++dx) {
        [unroll] for (int sx = 0; sx < 2; ++sx) {
            float weight = max(0.0, float(radius) + 1.0 - abs(float(dx * 2 + sx) - 0.5));
            if (weight == 0.0) continue;
            int source_x = clamp((x + dx) * 2 + sx, 0, int(source_width) - 1);
            [unroll] for (int sy = 0; sy < 2; ++sy) {
                sum += wire_mask.Load(int3(source_x, y * 2 + sy, 0)).rgb * weight;
            }
            total += weight * 2.0;
        }
    }
    blurred_wire[pos.xy] = float4(sum / max(total, 1.0), 0);
}
)HLSL";
static constexpr char starblade_wire_glow_vertical_hlsl[] = R"HLSL(
Texture2D<float4> blurred_wire : register(t0);
RWTexture2D<float4> soft_wire : register(u1);
Texture2D<uint> palette : register(t2);
[numthreads(32, 1, 1)]
void main(uint3 pos : SV_DispatchThreadID) {
    uint width, height; soft_wire.GetDimensions(width, height);
    int x = pos.x, y = pos.y;
    if (x >= int(width) || y >= 540) return;
    int radius = int(palette.Load(int3(20, 144, 0)));
    float3 sum = 0;
    float total = 0;
    [unroll] for (int dy = -5; dy <= 5; ++dy) {
        float weight = max(0.0, float(radius) + 1.0 - float(abs(dy * 2)));
        if (weight == 0.0) continue;
        sum += blurred_wire.Load(int3(x, clamp(y + dy, 0, 539), 0)).rgb * weight;
        total += weight;
    }
    soft_wire[pos.xy] = float4(sum / max(total, 1.0), 0);
}
)HLSL";
static constexpr char starblade_wire_glow_composite_hlsl[] = R"HLSL(
Texture2D<float4> scene_image : register(t0);
Texture2D<float4> soft_wire : register(t1);
RWTexture2D<float4> next_history : register(u5);
RWTexture2D<float4> present_image : register(u2);
Texture2D<uint> palette : register(t3);
Texture2D<float4> sharp_wire : register(t4);
[numthreads(32, 1, 1)]
void main(uint3 pos : SV_DispatchThreadID) {
    uint width, height; scene_image.GetDimensions(width, height);
    int x = pos.x, y = pos.y;
    if (x >= int(width) || y >= 1080) return;
    if (palette.Load(int3(23, 144, 0)) != 0) {
        float3 scene = scene_image.Load(int3(x, y, 0)).rgb;
        float4 wire = sharp_wire.Load(int3(x, y, 0));
        if (wire.a > 0.998) {
            next_history[pos.xy] = 0;
            present_image[pos.xy] = float4(scene, 1);
            return;
        }
        uint brightness = palette.Load(int3(22, 144, 0));
        float gain = brightness == 1 ? 0.8 : brightness == 3 ? 1.25 : brightness == 0 ? 1.6 : 1.0;
        uint tick = palette.Load(int3(25, 144, 0)) / 3;
        float shimmer = 0.94 + float((tick * 1664525u + 1013904223u) % 121u) * 0.001;
        float3 beam = saturate(wire.rgb * gain);
        float peak = max(beam.r, max(beam.g, beam.b));
        float3 emission = saturate(beam * (1.12 * shimmer) + peak * 0.09);
        float3 previous = 0;
        if (palette.Load(int3(24, 144, 0)) == 0) previous = soft_wire.Load(int3(x, y, 0)).rgb * 0.65;
        next_history[pos.xy] = float4(max(emission, previous), 0);
        float3 core = max(emission - beam, 0);
        float3 trail = max(previous - emission, 0) * 0.60;
        present_image[pos.xy] = float4(saturate(scene + core + trail), 1);
        return;
    }
    float2 uv = (float2(x, y) + 0.5) / 2.0 - 0.5;
    int2 a = int2(floor(uv));
    float2 t = frac(uv);
    uint soft_width, soft_height; soft_wire.GetDimensions(soft_width, soft_height);
    int2 limit = int2(soft_width - 1, soft_height - 1);
    float3 c00 = soft_wire.Load(int3(clamp(a, 0, limit), 0)).rgb;
    float3 c10 = soft_wire.Load(int3(clamp(a + int2(1, 0), 0, limit), 0)).rgb;
    float3 c01 = soft_wire.Load(int3(clamp(a + int2(0, 1), 0, limit), 0)).rgb;
    float3 c11 = soft_wire.Load(int3(clamp(a + int2(1, 1), 0, limit), 0)).rgb;
    float3 halo = lerp(lerp(c00, c10, t.x), lerp(c01, c11, t.x), t.y);
    float3 scene = scene_image.Load(int3(x, y, 0)).rgb;
    float core = sharp_wire.Load(int3(x, y, 0)).a;
    halo *= 1.0 - smoothstep(0.01, 0.30, core);
    uint level = palette.Load(int3(21, 144, 0));
    float gain = level == 1 ? 1.0 : level == 3 ? 3.5 : 2.0;
    present_image[pos.xy] = float4(saturate(scene + halo * gain), 1);
}
)HLSL";
