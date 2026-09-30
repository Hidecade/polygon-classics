// Included at the end of System 22 material preparation, before CPU rasterization.
if (system22_gpu::collecting && !m_state.m_is_ss22) {
    auto &packet=*system22_gpu::collecting;
    system22_gpu::material mat{};
    mat.left=m_cliprect.min_x*2.25f+(packet.output_width-1440)/2.f;
    mat.right=(m_cliprect.max_x+1)*2.25f+(packet.output_width-1440)/2.f;
    mat.top=m_cliprect.min_y*2.25f;
    mat.bottom=(m_cliprect.max_y+1)*2.25f;
    mat.palette=extra.pens-m_state.m_palette->pens();
    mat.bank=extra.bn;mat.cmode=extra.cmode;mat.priority=extra.prioverchar;
    mat.flags=(extra.texture_enabled?1:0)|(extra.shade_enabled?2:0);
    mat.fogfactor=extra.fogfactor;mat.fogcolor=extra.fogfactor ? uint32_t(extra.fogcolor.to_rgba()) : 0U;
    system22_gpu::vertex vertices[6];
    for (int n=0;n<clipverts;++n) {
        auto &a=clipv[n];
        vertices[n]={float(a.x*2.25+(packet.output_width-1440)/2.f),float(a.y*2.25),float(a.p[0]),float(a.p[1]),float(a.p[2]),float(a.p[3]),uint32_t(packet.materials.size())};
        auto &b=vertices[n];
        if (!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||!std::isfinite(b.u)||!std::isfinite(b.v)||!std::isfinite(b.i)) return;
    }
    packet.materials.push_back(mat);
    // The near-plane clip leaves a convex polygon with at most six vertices.
    for (int n=1;n<clipverts-1;++n)
        for (int index : {0,n,n+1}) packet.vertices.push_back(vertices[index]);
    return;
}
