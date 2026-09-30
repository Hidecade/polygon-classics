// license:BSD-3-Clause
#include <cstdio>
#include <cstdlib>
void model2_renderer::capture_gpu(polygon *poly)
{
    auto &scene=*model2_gpu::collecting->polygons;
    model2_gpu::material m{};
    m.left=poly->viewport[0]+m_xoffs; m.right=poly->viewport[2]+m_xoffs+1;
    m.top=384-poly->viewport[3]+m_yoffs; m.bottom=384-poly->viewport[1]+m_yoffs+1;
    if (m.left<=8 && m.right>=488) { m.left-=model2_gpu::margin(); m.right+=model2_gpu::margin(); }
    if (m.top<=8 && m.bottom>=376) {
        m.top-=model2_gpu::rotation_extra_y; m.bottom+=model2_gpu::rotation_extra_y;
    }
    const auto *h=poly->texheader;
    m.flags=(h[0]>>13)&7; m.color=(h[3]>>6)&0x3ff; m.luma=poly->luma; m.lumabase=(h[1]&255)<<7;
    m.texwidth=32<< (h[0]&7); m.texheight=32<<((h[0]>>3)&7);
    m.texx=32*(h[2]&63); m.texy=32*((h[2]>>6)&31); m.sheet=(h[2]>>12)&1;
    m.mirrorx=(h[0]>>8)&1; m.mirrory=(h[0]>>9)&1;
    m.wrapx=((h[0]>>6)&1)&~m.mirrorx; m.wrapy=((h[0]>>7)&1)&~m.mirrory;
    m.utex=(h[0]>>12)&1; m.utexminlod=(h[0]>>10)&3;
    m.utexx=((h[2]>>13)&1)*128; m.utexy=((h[2]>>14)&3)*128; m.texlod=poly->texlod;
    const uint32_t index=scene.materials.size(); scene.materials.push_back(m);
    auto vertex=[&](int i) {
        const auto &v=poly->v[i];
        float ooz=1.0f/(v.pz+std::numeric_limits<float>::min());
        return model2_gpu::vertex{v.x,v.y,ooz,v.pu*ooz/8,v.pv*ooz/8,index};
    };
    for(int i=1;i+1<poly->num_vertices;++i) {
        scene.vertices.push_back(vertex(0)); scene.vertices.push_back(vertex(i)); scene.vertices.push_back(vertex(i+1));
    }
}

u32 model2_state::screen_update_gpu(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
    auto packet=std::make_shared<model2_gpu::frame>();
    packet->emulation_speed=machine().video().speed_percent();
    const unsigned tw=packet->tile_width, margin=model2_gpu::margin();
    bitmap_rgb32 background(tw,384);
    background.fill(m_palette->pen(0));
    // draw_wide_edges intentionally touches ONLY the extra columns. Preserve
    // MAME's native center as well (title/logo tiles live in these even layers).
    const rectangle native_clip(0,495,0,383);
    m_sys24_bitmap.fill(0,native_clip);
    for(int layer=3;layer>=0;--layer) {
        const int flags=layer>=2 ? TILEMAP_DRAW_OPAQUE : 0;
        m_tiles->draw(screen,m_sys24_bitmap,native_clip,layer<<1,0,flags);
    }
    for(int y=0;y<384;++y) for(int x=0;x<496;++x) {
        const u32 color=m_sys24_bitmap.pix(y,x);
        if(color) background.pix(y,x+margin)=color;
    }
    model2_gpu::collecting=packet;
    render_polygons(bitmap,cliprect);
    model2_gpu::collecting.reset();
    model2_gpu::previous=packet->polygons;
    // Pure 2D title screens have no world beyond the hardware viewport.
    // Wrapping their tiles into the side field repeats the logo at both edges.
    if(!packet->polygons->vertices.empty()) {
        for(int layer=3;layer>=0;--layer) {
            m_tiles->draw_wide_edges(background,layer<<1,layer>=2 ? TILEMAP_DRAW_OPAQUE : 0,margin);
            if(layer==2 && std::string(machine().system().name)=="daytona") {
                auto *rom=memregion("main_data");
                m_tiles->draw_daytona_panorama_edges(background,margin,rom->base(),rom->bytes());
            }
        }
    }
    packet->background.resize(tw*384);
    for(int y=0;y<384;++y) std::copy_n(&background.pix(y),tw,packet->background.data()+y*tw);
    // Opt-in local diagnostics for source-tile seams. No files in normal play.
    if(const char *directory=std::getenv("MODEL2_CAPTURE")) {
        static unsigned serial=0;
        if(++serial<=7680 && serial%120==0) {
            auto dump=[&](const char *suffix,const void *data,size_t bytes) {
                const auto path=util::string_format("%s/%05u.%s",directory,serial,suffix);
                if(auto *file=std::fopen(path.c_str(),"wb")) { std::fwrite(data,1,bytes,file);std::fclose(file); }
            };
            std::vector<uint16_t> tiles(0x8000);
            for(unsigned i=0;i<tiles.size();++i) tiles[i]=m_tiles->tile_r(i);
            dump("tiles",tiles.data(),tiles.size()*2);
            dump("bgra",packet->background.data(),packet->background.size()*4);
            if(serial==120) {
                auto *rom=memregion("main_data");
                if(rom) dump("rom",rom->base(),rom->bytes());
            }
        }
    }
    m_sys24_bitmap.fill(0,cliprect);
    for(int layer=3;layer>=0;--layer) m_tiles->draw(screen,m_sys24_bitmap,cliprect,(layer<<1)|1,0,0);
    packet->hud.resize(496*384);
    for(int y=0;y<384;++y) for(int x=0;x<496;++x) {
        const u32 c=m_sys24_bitmap.pix(y,x);
        packet->hud[y*496+x]=c ? c|0xff000000 : 0;
    }
    packet->textures.resize(0x80000);
    std::copy_n(&m_textureram0[0],0x40000,packet->textures.data());
    std::copy_n(&m_textureram1[0],0x40000,packet->textures.data()+0x40000);
    std::copy_n(m_palram.get(),packet->palette.size(),packet->palette.data());
    std::copy_n(m_colorxlat.get(),packet->colorxlat.size(),packet->colorxlat.data());
    std::copy_n(m_lumaram.get(),packet->luma.size(),packet->luma.data());
    std::copy_n(m_gamma_table,256,packet->gamma.data());
    model2_gpu::published=packet;
    return 0;
}
