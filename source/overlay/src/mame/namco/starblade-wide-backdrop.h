// license:BSD-3-Clause
#pragma once

// Continue the top and bottom edges of a screen-space panel to the display
// margins. Extrapolating Y along each edge preserves its animated slope;
// scaling X alone would flatten the tilt. Keep already offscreen vertices.
template <typename Vertex>
inline void starblade_extend_panel(Vertex *v, double width)
{
    for (int row = 0; row < 2; ++row) {
        auto &left = v[row];
        auto &right = v[3 - row];
        const double dx = right.x - left.x;
        if (dx <= 0) continue;
        const double slope = (right.y - left.y) / dx;
        if (left.x > 0) {
            left.y -= left.x * slope;
            left.x = 0;
        }
        if (right.x < width) {
            right.y += (width - right.x) * slope;
            right.x = width;
        }
    }
}

// Solvalou's palette-animated screen effect, observed in the DSP stream.
// Exact geometry/depth avoids stretching terrain using the same palette.
inline bool solvalou_screen_flash(const int *sx, const int *sy, const int *z, unsigned color)
{
    if (color != 0x8000) return false;
    const int x[4] = {-256, -256, 256, 256};
    const int y[4] = {-256, 256, 256, -256};
    for (int i = 0; i < 4; ++i)
        if (sx[i] != x[i] || sy[i] != y[i] || z[i] != 0x77d0) return false;
    return true;
}

// ST2 ranking intro: a screen-covering, constant-depth purple quad. Match the
// emulated command, not RGB (the palette can fade), and leave scenery alone.
inline bool starblade_ranking_backdrop(const int *sx, const int *sy, const int *z, unsigned color)
{
    if (color != 0x8000) return false;
    const int x[4] = {-256, -256, 256, 256};
    const int y[4] = {-256, 256, 256, -256};
    for (int i = 0; i < 4; ++i)
        if (sx[i] != x[i] || sy[i] != y[i] || z[i] != 0x7fd0) return false;
    return true;
}

// Captured ST2 ranking commands: base 8000/7fd0, plus 22 overlapping fade
// strips (80c9..80d6/7fc1..7fce, 80c1..80c8/7fcf..7fd6).
// These reserved far depths and palette pairs distinguish the moving panel
// from scene geometry and the 8000/7fff offscreen control quad.
inline bool starblade_ranking_panel(const int *sx, const int *sy, const int *z, unsigned color)
{
    int depth;
    if (color == 0x8000) depth = 0x7fd0;
    else if (color >= 0x80c9 && color <= 0x80d6) depth = 0x7fc1 + color - 0x80c9;
    else if (color >= 0x80c1 && color <= 0x80c8) depth = 0x7fcf + color - 0x80c1;
    else return false;
    for (int i = 0; i < 4; ++i)
        if (z[i] != depth) return false;
    return sx[0] < 0 && sx[1] < 0 && sx[2] > 0 && sx[3] > 0
        && sy[1] > sy[0] && sy[2] > sy[3];
}
