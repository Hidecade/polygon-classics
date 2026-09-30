system22_profile::scope video_profile(4);
if (system22_profile::enabled) ++system22_profile::frames;
if (system22_gpu::enabled && system22_gpu::available) {
    system22_profile::scope packet_profile(5);
    if (!system22_gpu::texture_data) {
        auto data=std::make_shared<system22_gpu::assets>();
        data->map.assign(m_texture_tilemap,m_texture_tilemap+0x100000);
        data->attributes.assign(m_texture_tileattr.get(),m_texture_tileattr.get()+0x100000);
        data->texels.assign(m_texture_tiledata,m_texture_tiledata+m_gfxdecode->gfx(1)->elements()*256);
        std::copy_n(m_texture_ayx_to_pixel.get(),4096,data->lookup.begin());
        std::copy_n(&m_gamma_proms[0],768,data->gamma.begin());
        system22_gpu::texture_data=std::move(data);
    }
    auto packet=std::make_shared<system22_gpu::frame>();
    if (system22_profile::enabled && system22_profile::frames%120==0) packet->cpu_profile=system22_profile::report();
    packet->emulation_speed=machine().video().speed_percent();
    packet->textures=system22_gpu::texture_data;
    packet->hud.resize(640*480);
    std::copy_n(m_palette->pens(),32768,packet->palette.begin());
    packet->mix.background=m_palette->pen(m_bg_palbase|0xff);
    packet->mix.fade_r=m_screen_fade_r;packet->mix.fade_g=m_screen_fade_g;packet->mix.fade_b=m_screen_fade_b;
    packet->mix.shadow=BIT(m_mixer_flags,8);
    for (int n=0;n<3;++n) packet->mix.shadow_colors[n]=
        (nthbyte(m_mixer,8+n*3)<<16)|(nthbyte(m_mixer,9+n*3)<<8)|nthbyte(m_mixer,10+n*3);
    system22_gpu::collecting=std::move(packet);
} else {
    system22_gpu::collecting.reset();
    system22_gpu::published.reset();
}
