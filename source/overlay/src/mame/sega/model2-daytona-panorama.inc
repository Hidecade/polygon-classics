// license:BSD-3-Clause
// Replace only background side columns; leave the native viewport and HUD intact.
bool segas24_tile_device::draw_daytona_panorama_edges(bitmap_rgb32 &bitmap, int margin, const uint8_t *rom, size_t bytes)
{
    const uint16_t vscr=tile_ram[0x5006], hscr=tile_ram[0x5002];
    const model2_gpu::daytona_panorama maps{rom,bytes};
    const bool active=(vscr&0xe000)==0x2000 && !(hscr&0x8000);
    const int observed=active ? maps.identify(hscr,m_daytona_panorama,
        [&](int row,int col){return tile_ram[0x2000+row*64+col];}) : -1;
    const int phase=m_daytona_panorama.update(observed,hscr,active);
    if(phase<0 || m_daytona_panorama.bank<0) return false;
    const auto sky=maps.course(m_daytona_panorama.bank);
    if(!sky.valid()) return false;
    auto *characters=gfx(char_gfx_index);
    const pen_t *pens=palette().pens();
    for(int y=0;y<384;++y) {
        const int source_y=(y+vscr)&1023;
        if(source_y<sky.top || source_y>=sky.top+sky.height*8) continue;
        for(int side=0;side<2;++side) for(int i=0;i<margin;++i) {
            const int x=side ? 496+i : i-margin;
            const int source_x=(x+phase)&2047;
            const uint16_t entry=sky.tile(source_x/8,(source_y-sky.top)/8);
            const uint8_t *pixels=characters->get_data(entry&tile_mask);
            const unsigned pen=characters->colorbase()+characters->granularity()*((entry>>7)&255)
                +pixels[(source_y&7)*characters->rowbytes()+(source_x&7)];
            bitmap.pix(y,x+margin)=pens[pen];
        }
    }
    return true;
}
