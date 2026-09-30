// license:BSD-3-Clause
// Adapted from Sega System 24 tile addressing (Olivier Galibert).
// Extend the background from source tiles, not a repeated 496-pixel screenshot.
// The native compositor still supplies the central window unchanged.
void segas24_tile_device::draw_wide_edges(bitmap_rgb32 &bitmap, int layer, int flags, int margin)
{
    const int plane=layer>>1;
    const uint16_t hreg=tile_ram[0x5000+plane];
    const uint16_t vscr=tile_ram[0x5004+plane];
    const int mode=(tile_ram[0x5004+(plane&2)]>>13)&3;
    if ((vscr&0x8000) || (mode && (plane&1))) return;
    const uint16_t *mask=tile_ram.get()+((layer&4)?0x6800:0x6000);
    const pen_t *pens=palette().pens();
    const int priority=(layer&1)|TILEMAP_PIXEL_LAYER0;
    // Materialize the relevant pixmaps once, including dirty character tiles.
    auto &first=tile_layer[plane]->pixmap();
    auto &first_flags=tile_layer[plane]->flagsmap();
    auto &second=tile_layer[mode ? (plane|1) : plane]->pixmap();
    auto &second_flags=tile_layer[mode ? (plane|1) : plane]->flagsmap();
    const int my=(bitmap.height()-384)/2;
    for (int y=-my;y<384+my;++y) {
        const int edgeY=std::clamp(y,0,383);
        // Vertical overscan belongs to VR's sky plane. Native HUD/other pages
        // have no world outside the hardware viewport and must not wrap there.
        if (my && y!=edgeY && plane<2) continue;
        const int worldY=my && mode==1 ? model1_hd::vr_background_row(y,vscr) : y+vscr;
        const int h=(hreg&0x8000)?tile_ram[0x4000+0x200*plane+edgeY]:hreg;
        const int sy=worldY&511;
        for (int x=-margin;x<496+margin;++x) {
            if (y>=0 && y<384 && x>=0 && x<496) continue;
            bool other=false;
            if (mode) {
                other=mode==1 ? (worldY&512)!=0 : ((x-(h&1023))&512)!=0;
            } else {
                // Window masks cover only the hardware viewport. Continue the
                // edge's layer selection; never read outside the mask row.
                const int edge=std::clamp(x,0,495);
                const bool selected=(mask[edgeY*4+edge/128] & (0x8000>>((edge%128)/8)))!=0;
                if (selected != bool(plane&1)) continue;
            }
            const int sx=(x-h)&511;
            auto &pixels=other?second:first;
            auto &coverage=other?second_flags:first_flags;
            const int bits=coverage.pix(sy,sx);
            // Special modes use tilemap::draw: opaque disables transparency,
            // but retains category filtering. The normal draw_rect path does not.
            const bool visible=mode
                ? ((bits&TILEMAP_PIXEL_CATEGORY_MASK)==(layer&1) &&
                   ((bits&TILEMAP_PIXEL_LAYER0) || (flags&TILEMAP_DRAW_OPAQUE)))
                : (bits==priority || (flags&TILEMAP_DRAW_OPAQUE));
            if (visible)
                bitmap.pix(y+my,x+margin)=pens[pixels.pix(sy,sx)];
        }
    }
}

