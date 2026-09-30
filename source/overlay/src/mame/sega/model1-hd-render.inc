// Keep Model 1 timing and painter ordering; Metal rasterizes these screen-space packets.
void model1_state::fill_quad(bitmap_rgb32 &bitmap, view_t *view, const quad_t &q) const
{
    if (bitmap.width() != model1_hd::width) {
        fill_quad_native(bitmap, view, q);
        return;
    }
    if (!model1_hd::collecting) return;
    auto &vertices = model1_hd::above ? model1_hd::collecting->above : model1_hd::collecting->below;
    uint32_t color = q.col < 0 ? -1 - q.col : q.col;
    model1_hd::vertex v[4];
    for (int i=0;i<4;++i) {
        if (!std::isfinite(q.p[i]->hd_x) || !std::isfinite(q.p[i]->hd_y)) return;
        v[i] = {q.p[i]->hd_x * model1_hd::scale_x + model1_hd::offset_x,
                q.p[i]->hd_y * model1_hd::scale_y, color,
                view->x1 * model1_hd::scale_x + model1_hd::offset_x,
                view->y1 * model1_hd::scale_y,
                (view->x2+1)*model1_hd::scale_x + model1_hd::offset_x,
                (view->y2+1)*model1_hd::scale_y};
    }
    int end=-1; bool line=true;
    for (int i=1;i<4;++i) {
        if (v[i].x==v[0].x && v[i].y==v[0].y) continue;
        if (end<0) end=i;
        else if (v[i].x!=v[end].x || v[i].y!=v[end].y) line=false;
    }
    if (line) {
        if (end<0) return;
        auto a=v[0],b=v[end];
        float dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
        float nx=-dy/length*.5f,ny=dx/length*.5f;
        v[0]=v[1]=a; v[2]=v[3]=b;
        v[0].x+=nx;v[0].y+=ny;v[1].x-=nx;v[1].y-=ny;
        v[2].x-=nx;v[2].y-=ny;v[3].x+=nx;v[3].y+=ny;
    }
    for (int i : {0,1,2,0,2,3}) vertices.push_back(v[i]);
}

uint32_t model1_state::screen_update_model1_hd(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
    constexpr uint32_t transparent = 0x01234567;
    if (!m_hd_bitmap.valid()) m_hd_bitmap.allocate(model1_hd::width, model1_hd::height);
    if (!m_hd_overlay.valid()) m_hd_overlay.allocate(bitmap.width(), bitmap.height());
    m_view->ayyc = cos(m_view->ayy);
    m_view->ayys = sin(m_view->ayy);
    screen.priority().fill(0);
    bitmap.fill(m_palette->pen(0), cliprect);
    m_tiles->draw(screen, bitmap, cliprect, 6, 0, TILEMAP_DRAW_OPAQUE);
    m_tiles->draw(screen, bitmap, cliprect, 4, 0, TILEMAP_DRAW_OPAQUE);
    m_tiles->draw(screen, bitmap, cliprect, 2, 0, 0);
    m_tiles->draw(screen, bitmap, cliprect, 0, 0, 0);
    auto packet = std::make_shared<model1_hd::frame>();
    packet->virtua_racing = std::string(machine().system().name) == "vr";
    for (int i=0;i<8;++i) packet->tile_registers[i]=m_tiles->tile_r(0x5000+i);
    if (std::getenv("MODEL1_CAPTURE")) {
        packet->sky_tiles.resize(4096);
        for (int i=0;i<4096;++i) packet->sky_tiles[i]=m_tiles->tile_r(0x2000+i);
    }
    if (!m_hd_background.valid()) m_hd_background.allocate(model1_hd::background_width,384+2*model1_hd::extra_y);
    m_hd_background.fill(m_palette->pen(0));
    for (int layer : {6,4,2,0}) {
        m_tiles->draw_wide_edges(m_hd_background,layer,layer>=4 ? TILEMAP_DRAW_OPAQUE : 0,model1_hd::extra);
        if(layer==4 && packet->virtua_racing) {
            auto *rom=memregion("maincpu");
            m_tiles->draw_vr_panorama_edges(m_hd_background,model1_hd::extra,rom->base(),rom->bytes(),packet->sky_phase);
        }
    }
    packet->background.resize(model1_hd::background_width*m_hd_background.height());
    packet->hud.resize(496*384);
    for (int y=0;y<384;++y) {
        std::copy_n(&bitmap.pix(y),496,&m_hd_background.pix(y+model1_hd::extra_y,model1_hd::extra));

    }
    for (int y=0;y<m_hd_background.height();++y)
        std::copy_n(&m_hd_background.pix(y),model1_hd::background_width,packet->background.data()+y*model1_hd::background_width);
    model1_hd::collecting=packet;
    const rectangle wideclip(-model1_hd::extra,495+model1_hd::extra,-model1_hd::extra_y,383+model1_hd::extra_y);
    model1_hd::above=false;
    tgp_render(m_hd_bitmap,wideclip,RENDER_BELOW_HUD);
    m_hd_overlay.fill(transparent);
    for (int layer : {7,5,3,1}) m_tiles->draw(screen,m_hd_overlay,cliprect,layer,0,0);
    for (int y=0;y<384;++y)
        for (int x=0;x<496;++x) {
            uint32_t c=m_hd_overlay.pix(y,x);
            packet->hud[y*496+x]=c==transparent ? 0 : (c | 0xff000000);
        }
    model1_hd::above=true;
    tgp_render(m_hd_bitmap,wideclip,RENDER_ABOVE_HUD);
    model1_hd::collecting.reset();
    model1_hd::published=std::move(packet);
    return 0;
}
