// The VR program pages its 2048-pixel sky into the 512-pixel tile RAM window.
// Extend from the original strip map, never by repeating that temporary window.
bool segas24_tile_device::draw_vr_panorama_edges(bitmap_rgb32 &bitmap, int margin, const uint8_t *rom, size_t bytes, int *phase_out)
{
    const uint16_t vscr=tile_ram[0x5006], hscr=tile_ram[0x5002];
    model1_hd::vr_panorama sky{rom,bytes};
    const bool active=(vscr&0xe000)==0x2000 && !(hscr&0x8000);
    const int observed=active ? sky.identify(hscr,m_vr_panorama,
        [&](int row,int col){return tile_ram[0x2000+row*64+col];}) : -1;
    const int phase=m_vr_panorama.update(observed,hscr,active);
    if(phase_out) { phase_out[0]=observed;phase_out[1]=phase; }
    if(phase<0 || m_vr_panorama.bank<0) return false;
    sky=sky.course(m_vr_panorama.bank);
    if(!sky.valid()) return false;
    auto *characters=gfx(char_gfx_index);
    const pen_t *pens=palette().pens();
    const int my=(bitmap.height()-384)/2;
    for(int y=-my;y<384+my;++y) {
        const int source_y=my ? model1_hd::vr_background_row(y,vscr) : (y+vscr)&1023;
        // Other rows/pages are the untextured sky and ground from draw_wide_edges.
        if(source_y<sky.top || source_y>=sky.top+sky.height*8) continue;
        for(int x=-margin;x<496+margin;++x) {
            if (y>=0 && y<384 && x>=0 && x<496) continue;
            const int source_x=(x+phase)&2047;
            const uint16_t entry=sky.tile(source_x/8,(source_y-sky.top)/8);
            const uint8_t *pixels=characters->get_data(entry&tile_mask);
            const unsigned pen=characters->colorbase()+characters->granularity()*((entry>>7)&255)
                +pixels[(source_y&7)*characters->rowbytes()+(source_x&7)];
            bitmap.pix(y+my,x+margin)=pens[pen];
        }
    }
    return true;
}
