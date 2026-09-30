// license:BSD-3-Clause
#pragma once
#include <algorithm>
#include <cstdint>

// The main blast uses up to four overlapping priority-0 sprite groups.
inline bool starblade_explosion_tile(uint32_t code, int priority, int sprite_tiles)
{
    // The known blast atlas is priority 0. Other large low-priority effect
    // sprites (including missile impacts) use the same 8x8 C355 format.
    return priority >= 0 && priority <= 1
        && ((code >= 0x07ee && code <= 0x08fd) || sprite_tiles >= 48);
}

// Draw three faceted fragments per native tile. Their outward displacement
// changes in short steps, producing a scattered polygon wave without copying
// the original bitmap into the output surface.
template<class Bitmap>
void starblade_draw_explosion_tile(Bitmap &dest, int clip_left, int clip_top,
    int clip_right, int clip_bottom, int left, int top, int width, int height,
    const uint8_t *source, int rowbytes, int source_width, int source_height,
    uint8_t transparent, uint16_t palette_base, uint8_t priority,
    uint32_t code, uint64_t frame, bool wireframe)
{
    if (width <= 0 || height <= 0) return;
    // A blast is an 8x8 tile sprite, often drawn in four overlapping layers.
    // Drawing fragments on every tile recreates the dense bitmap silhouette.
    // Keep a stable, sparse sample of tiles and never fall back to their art.
    if (((code * 1664525U + 1013904223U) & 7U) != 0) return;
    const int phase = int((frame / 3 + code / 7) % 8);
    for (int shard = 0; shard < 2; ++shard)
    {
        const uint32_t seed = code * 1664525U + uint32_t(shard) * 1013904223U;
        const int sx = int((seed >> 8) % uint32_t(source_width));
        const int sy = int((seed >> 17) % uint32_t(source_height));
        uint8_t ink = source[sy * rowbytes + sx];
        if (ink == transparent)
        {
            bool found = false;
            for (int i = 0; i < source_width * source_height; ++i)
                if (source[(i / source_width) * rowbytes + i % source_width] != transparent)
                { ink = source[(i / source_width) * rowbytes + i % source_width]; found = true; break; }
            if (!found) continue;
        }
        const int direction_x = int((seed >> 3) % 5) - 2;
        const int direction_y = int((seed >> 13) % 5) - 2;
        const int cx = left + (int((seed >> 22) % 3) + 1) * width / 4 + direction_x * phase / 3;
        const int cy = top + (shard + 1) * height / 4 + direction_y * phase / 3;
        const int size = std::max(2, std::min(width, height) / 2);
        const int ax = cx - size, ay = cy + size / 2;
        const int bx = cx + size, by = cy - size / 2;
        const int dx = cx + direction_x * size / 2, dy = cy + size;
        const int x0 = std::max(clip_left, std::min({ax, bx, dx}));
        const int x1 = std::min(clip_right, std::max({ax, bx, dx}));
        const int y0 = std::max(clip_top, std::min({ay, by, dy}));
        const int y1 = std::min(clip_bottom, std::max({ay, by, dy}));
        const int area = (bx - ax) * (dy - ay) - (by - ay) * (dx - ax);
        if (!area) continue;
        const uint16_t pen = ((priority & 15) << 12) | ((palette_base + ink) & 0xfff);
        const int64_t edge_ab = int64_t(bx - ax) * (bx - ax) + int64_t(by - ay) * (by - ay);
        const int64_t edge_bd = int64_t(dx - bx) * (dx - bx) + int64_t(dy - by) * (dy - by);
        const int64_t edge_da = int64_t(ax - dx) * (ax - dx) + int64_t(ay - dy) * (ay - dy);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
            {
                const int a = (bx - ax) * (y - ay) - (by - ay) * (x - ax);
                const int b = (dx - bx) * (y - by) - (dy - by) * (x - bx);
                const int c = (ax - dx) * (y - dy) - (ay - dy) * (x - dx);
                const bool inside = (area > 0 && a >= 0 && b >= 0 && c >= 0)
                    || (area < 0 && a <= 0 && b <= 0 && c <= 0);
                if (!inside) continue;
                if (wireframe)
                {
                    // Keep only a 1.5-native-pixel outline of the shard.
                    if (16LL * a * a > 9LL * edge_ab
                        && 16LL * b * b > 9LL * edge_bd
                        && 16LL * c * c > 9LL * edge_da) continue;
                }
                dest.pix(y, x) = pen;
            }
    }
}
