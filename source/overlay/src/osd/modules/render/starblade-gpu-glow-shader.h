// license:BSD-3-Clause
#pragma once
static constexpr char starblade_glow_hlsl[] = R"HLSL(
Texture2D<uint> sprites : register(t0);
Texture2D<uint> palette : register(t1);
RWTexture2D<float4> glow_image : register(u2);
[numthreads(16, 16, 1)]
void main(uint3 pos : SV_DispatchThreadID) {
    int x = pos.x, y = pos.y;
    if (x >= 496 || y >= 480) return;
    uint3 sum = 0;
    for (int sy = max(0, y - 1); sy <= min(479, y + 1); ++sy) {
        for (int sx = max(0, x - 1); sx <= min(495, x + 1); ++sx) {
            uint sprite = sprites.Load(int3(sx, sy, 0)), raw = sprite & 65535;
            if ((sprite >> 16) != 1 || ((raw >> 12) & 15) != 3 || (raw & 255) < 2 || (raw & 255) == 255) continue;
            uint pen = 4096 | ((raw & 4095) ^ 3840);
            uint color = palette.Load(int3(pen % 256, pen / 256, 0));
            uint weight = (sx == x ? 2 : 1) * (sy == y ? 2 : 1);
            sum += uint3((color >> 16) & 255, (color >> 8) & 255, color & 255) * weight;
        }
    }
    uint strength = palette.Load(int3(0, 144, 0)) >> 2;
    uint gain = strength == 1 ? 20 : strength == 2 ? 35 : strength == 3 ? 50 : 0;
    glow_image[pos.xy] = float4(sum * gain / 1600, 0) / 255.0;
}
)HLSL";
