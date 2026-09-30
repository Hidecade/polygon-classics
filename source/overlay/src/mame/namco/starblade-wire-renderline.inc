// license:BSD-3-Clause
// Analytic 1-2 pixel stroke. Sample the actual line segment at pixel centres;
// rounding three parallel DDA lines made diagonal edges visibly stair-step.
void namcos21_3d_device::renderline(const n21_vertex &v0, const n21_vertex &v1, u16 color, int zsort)
{
    double x0 = v0.x, y0 = v0.y, x1 = v1.x, y1 = v1.y;
    const double dx = x1 - x0, dy = y1 - y0;
    double t0 = 0.0, t1 = 1.0;
    const auto clip = [&t0, &t1](double p, double q)
    {
        if (p == 0.0) return q >= 0.0;
        const double r = q / p;
        if (p < 0.0) t0 = std::max(t0, r); else t1 = std::min(t1, r);
        return t0 <= t1;
    };
    if (!clip(-dx, x0) || !clip(dx, m_poly_frame_width - 1 - x0)
        || !clip(-dy, y0) || !clip(dy, m_poly_frame_height - 1 - y0)) return;
    x1 = x0 + dx * t1;
    y1 = y0 + dy * t1;
    x0 += dx * t0;
    y0 += dy * t0;

    if (m_gpu_collect && starblade_gpu::lines_available && starblade_gpu::requested && zsort < 0x8000)
    {
        // Shared polygon edges must use identical arithmetic in both directions.
        if (x0 > x1 || (x0 == x1 && y0 > y1)) { std::swap(x0,x1); std::swap(y0,y1); }
        m_gpu_work.push_back({0xffffffffU, {starblade_gpu::float_word(float(x0)), starblade_gpu::float_word(float(y0)),
            0xc0000000U | unsigned(color), unsigned(zsort)}});
        m_gpu_work.push_back({0xfffffffeU, {starblade_gpu::float_word(float(x1)), starblade_gpu::float_word(float(y1)),
            starblade_gpu::float_word(float(m_wire_width)), 0}});
        return;
    }

    const auto emit = [this, zsort](int y, int left, int right, u16 pen, u8 coverage)
    {
        left = std::max(0, left);
        right = std::min(m_poly_frame_width, right);
        if (!coverage || y < 0 || y >= m_poly_frame_height || left >= right || zsort >= 0x8000) return;
        if (m_gpu_collect)
        {
            m_gpu_work.push_back({unsigned(y), {unsigned(left), unsigned(right),
                uint32_t(pen) | (uint32_t(coverage) << 16) | 0x80000000U, unsigned(zsort)}});
            return;
        }
        u16 *dest = m_poly_framebuffer_pens.get() + y * m_poly_frame_width;
        u16 *zbuf = m_poly_framebuffer_z.get() + y * m_poly_frame_width;
        for (int x = left; x < right; ++x)
            if (((x * 73 + y * 151) & 255) < coverage && zsort < zbuf[x])
            { dest[x] = pen; zbuf[x] = zsort; }
    };

    const double vx = x1 - x0, vy = y1 - y0;
    const double length_sq = vx * vx + vy * vy;
    const double horizontal_reach = std::abs(vy) > 0.001
        ? 2.0 * std::sqrt(length_sq) / std::abs(vy) : 0.0;
    const int min_x = std::max(0, int(std::floor(std::min(x0, x1) - 2.0)));
    const int max_x = std::min(m_poly_frame_width - 1, int(std::ceil(std::max(x0, x1) + 2.0)));
    const int min_y = std::max(0, int(std::floor(std::min(y0, y1) - 2.0)));
    const int max_y = std::min(m_poly_frame_height - 1, int(std::ceil(std::max(y0, y1) + 2.0)));
    for (int y = min_y; y <= max_y; ++y)
    {
        int row_min = min_x, row_max = max_x;
        if (std::abs(vy) > 0.001)
        {
            const double crossing = x0 + (y + 0.5 - y0) * vx / vy;
            row_min = std::max(min_x, int(std::floor(crossing - horizontal_reach - 0.5)));
            row_max = std::min(max_x, int(std::ceil(crossing + horizontal_reach - 0.5)));
        }
        if (row_min > row_max) continue;
        int run_begin = -1;
        u8 run_coverage = 0;
        for (int x = row_min; x <= row_max + 1; ++x)
        {
            u8 coverage = 0;
            if (x <= row_max)
            {
                const double px = x + 0.5, py = y + 0.5;
                const double t = length_sq > 0.0
                    ? std::clamp(((px - x0) * vx + (py - y0) * vy) / length_sq, 0.0, 1.0) : 0.0;
                const double distance = std::hypot(px - (x0 + t * vx), py - (y0 + t * vy));
                coverage = u8(std::lround(255.0 * std::clamp(m_wire_width * 0.5 + 0.5 - distance, 0.0, 1.0)));
            }
            if (coverage != run_coverage)
            {
                if (run_coverage) emit(y, run_begin, x, color, run_coverage);
                run_begin = x;
                run_coverage = coverage;
            }
        }
    }
}
