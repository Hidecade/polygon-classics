// license:BSD-3-Clause
#pragma once
#include <algorithm>
#include <vector>

// Emit only UI light at its original raster size. The renderer smoothly scales
// this small additive layer over the sharp 4:3 HUD; the scene is never blurred.
class starblade_ui_glow
{
    std::vector<uint64_t> m_light = std::vector<uint64_t>(496 * 480);
    std::vector<int> m_touched;
public:
    void render(bitmap_rgb32 &output, const bitmap_ind16 &sprites,
        const bitmap_ind8 &mask, const rgb_t *palette, unsigned strength)
    {
        output.fill(0);
        if (!strength) return;
        static constexpr int weights[] = {1, 2, 1};
        for (int sy = 0; sy < 480; ++sy)
            for (int sx = 0; sx < 496; ++sx)
            {
                const u16 raw = sprites.pix(sy, sx);
                if (mask.pix(sy, sx) != 1 || ((raw >> 12) & 15) != 3
                    || (raw & 255) < 2 || (raw & 255) == 255)
                    continue;
                const rgb_t color = palette[0x1000 | ((raw & 0xfff) ^ 0xf00)];
                const uint64_t emission = (uint64_t(color.r()) << 32) | (uint64_t(color.g()) << 16) | color.b();
                if (!emission) continue;
                for (int y = std::max(0, sy - 1); y <= std::min(479, sy + 1); ++y)
                    for (int x = std::max(0, sx - 1); x <= std::min(495, sx + 1); ++x)
                    {
                        const int index = y * 496 + x;
                        if (!m_light[index]) m_touched.push_back(index);
                        // Each channel sums at most 255*16; packed lanes cannot carry.
                        m_light[index] += emission * weights[y - sy + 1] * weights[x - sx + 1];
                    }
            }
        const unsigned gain = strength == 1 ? 20 : strength == 2 ? 35 : 50;
        for (const int index : m_touched)
        {
            const auto v = m_light[index];
            output.pix(index / 496, index % 496) = rgb_t(unsigned((v >> 32) & 0xffff) * gain / 1600,
                unsigned((v >> 16) & 0xffff) * gain / 1600, unsigned(v & 0xffff) * gain / 1600);
            m_light[index] = 0;
        }
        m_touched.clear();
    }
};
