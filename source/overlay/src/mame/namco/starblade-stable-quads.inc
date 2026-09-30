// license:BSD-3-Clause
int namcos21_3d_device::draw_stable_quads(const u16 *source, const u8 *pointram, const u32 ptram_size, u32 quad_idx)
{
    const u32 ptram_mask = ptram_size - 1;
    uint64_t shape = 14695981039346656037ULL ^ quad_idx;
    quad_idx = (quad_idx * 6) & ptram_mask;
    struct face { u16 color; u8 index[4]; };
    std::vector<face> faces;
    bool used[256] = {};
    int max_vi = -1;
    for (u32 count = 0; count < ptram_size / 6; ++count) {
        u8 data[6];
        for (int i = 0; i < 6; ++i) {
            data[i] = pointram[quad_idx]; quad_idx = (quad_idx + 1) & ptram_mask;
            shape = (shape ^ data[i]) * 1099511628211ULL;
        }
        faces.push_back({u16((data[1] | (data[0] << 8)) & 0x7fff), {data[2], data[3], data[4], data[5]}});
        for (int i = 2; i < 6; ++i) { used[data[i]] = true; max_vi = std::max(max_vi, int(data[i])); }
        if (data[0] & 0x80) break;
    }
    const bool stabilize = m_wireframe && (m_projection_scale_x > 1 || m_projection_scale_y > 1);
    starblade_wire_stabilizer::mesh filtered;
    if (stabilize) {
        starblade_wire_stabilizer::mesh raw(max_vi + 1, {0, 0, 0});
        for (int i = 0; i <= max_vi; ++i) if (used[i])
            raw[i] = {double(s16(source[i * 3])), double(s16(source[i * 3 + 1])), double(s16(source[i * 3 + 2]))};
        filtered = m_wire_stabilizer.apply(shape, raw);
    }
    for (const auto &face : faces) {
        int sx[4], sy[4], zcode[4]; n21_vertex stable[4];
        for (int i = 0; i < 4; ++i) {
            const unsigned vi = face.index[i];
            sx[i] = s16(source[vi * 3]); sy[i] = s16(source[vi * 3 + 1]); zcode[i] = s16(source[vi * 3 + 2]);
            if (stabilize) stable[i] = {m_poly_frame_width / 2 + filtered[vi].x * m_projection_scale_x,
                m_poly_frame_height / 2 + filtered[vi].y * m_projection_scale_y, filtered[vi].z};
        }
        blit_single_quad(sx, sy, zcode, face.color, stabilize ? stable : nullptr);
    }
    return (max_vi + 1) * 3;
}
