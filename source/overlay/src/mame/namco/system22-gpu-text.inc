if (system22_gpu::collecting) {
    auto &packet=*system22_gpu::collecting;
    for (int y=0;y<480;++y)
        for (int x=0;x<640;++x)
            packet.hud[y*640+x]=screen.priority().pix(y,x)==2 ? (0x10000|m_mix_bitmap->pix(y,x)) : 0;
    system22_gpu::published=std::move(system22_gpu::collecting);
    return;
}
