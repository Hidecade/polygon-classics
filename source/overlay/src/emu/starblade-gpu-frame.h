// license:BSD-3-Clause
#pragma once
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <array>
#include <cmath>
#include <cstring>

// Immutable, renderer-independent packets. No GPU calls on the emulation thread.
namespace starblade_gpu {
struct span { uint32_t x0, x1, pen, depth; };
struct scan_span { uint32_t y; span pixels; };
// Two records per analytic line: (x0,y0,flag|pen,depth), (x1,y1,width,0).
// The second record is payload, never an independently binned primitive.
inline uint32_t float_word(float f) { uint32_t u; std::memcpy(&u,&f,4); return u; }
inline float word_float(uint32_t u) { float f; std::memcpy(&f,&u,4); return f; }
inline bool is_line(const span &s) { return (s.pen & 0xc0000000U) == 0xc0000000U; }
inline std::atomic<bool> lines_available{false}; // Only the native endpoint-capable shader enables this.
template<class Emit> inline void line_rows(const span &a, const span &b, unsigned width, unsigned height, Emit emit) {
    const double x0=word_float(a.x0), y0=word_float(a.x1), x1=word_float(b.x0), y1=word_float(b.x1);
    const double dx=x1-x0, dy=y1-y0;
    const double reach=std::abs(dy)>0.001 ? 2.0*std::sqrt(dx*dx+dy*dy)/std::abs(dy) : 0.0;
    const int minx=std::max(0,int(std::floor(std::min(x0,x1)-2.0)));
    const int maxx=std::min(int(width)-1,int(std::ceil(std::max(x0,x1)+2.0)));
    const int miny=std::max(0,int(std::floor(std::min(y0,y1)-2.0)));
    const int maxy=std::min(int(height)-1,int(std::ceil(std::max(y0,y1)+2.0)));
    for(int y=miny;y<=maxy;++y) {
        int left=minx,right=maxx;
        if(std::abs(dy)>0.001) {
            const double crossing=x0+(y+0.5-y0)*dx/dy;
            left=std::max(left,int(std::floor(crossing-reach-0.5)));
            right=std::min(right,int(std::ceil(crossing+reach-0.5)));
        }
        if(left<=right) emit(unsigned(y),unsigned(left),unsigned(right+1));
    }
}
// Diagnostic/snapshot/save-state fallback only. Gameplay sends endpoints to GPU.
inline std::vector<scan_span> expand_lines(const std::vector<scan_span> &source, unsigned width, unsigned height) {
    std::vector<scan_span> out;
    out.reserve(source.size());
    for(size_t i=0;i<source.size();++i) {
        const auto &a=source[i].pixels;
        if(!is_line(a)) { out.push_back(source[i]); continue; }
        const auto &b=source[++i].pixels;
        const double x0=word_float(a.x0), y0=word_float(a.x1), dx=word_float(b.x0)-x0, dy=word_float(b.x1)-y0;
        const double length=dx*dx+dy*dy, half=word_float(b.pen)*0.5;
        line_rows(a,b,width,height,[&](unsigned y,unsigned left,unsigned right) {
            unsigned start=left,last=0;
            for(unsigned x=left;x<=right;++x) {
                unsigned coverage=0;
                if(x<right) {
                    const double px=x+0.5-x0,py=y+0.5-y0;
                    const double t=length>0 ? std::clamp((px*dx+py*dy)/length,0.0,1.0) : 0;
                    const double distance=std::hypot(px-t*dx,py-t*dy);
                    coverage=unsigned(std::lround(255.0*std::clamp(half+0.5-distance,0.0,1.0)));
                }
                if(coverage!=last) {
                    if(last) out.push_back({y,{start,x,(a.pen&65535)|(last<<16)|0x80000000U,a.depth}});
                    start=x;last=coverage;
                }
            }
        });
    }
    return out;
}
struct frame {
    uint64_t serial = 0;
    unsigned width = 1920, height = 1080, auxiliary_width = 662;
    int auxiliary_base = -1;
    bool solvalou_crop = false, solvalou_status = false;
    std::vector<scan_span> spans;
    std::vector<uint32_t> sprites; // raw C355 pen in low 16 bits, effect tag above
    std::vector<uint16_t> wide_sprites; // scene pixels across the selected native-width surface
    std::vector<uint32_t> palette;
    unsigned depth_test = 0, scanlines = 0, glow_strength = 0, wire_glow_radius = 0, wire_glow_level = 2, wire_brightness = 2, vector_effect = 0;
};
// Retain allocations, but never reuse a packet still owned by the renderer,
// a presentation slot, snapshot or asynchronous validation readback.
inline std::shared_ptr<frame> acquire_frame() {
    static thread_local std::vector<std::shared_ptr<frame>> pool;
    for(auto &packet:pool) if(packet.use_count()==1) return packet;
    auto packet=std::make_shared<frame>();
    if(pool.size()<4) pool.push_back(packet);
    return packet;
}
inline std::atomic<bool> available{false}, requested{true}, glow_available{false};
inline std::mutex mutex;
struct slot { const void *base = nullptr; bitmap_ind16 *bitmap = nullptr; std::shared_ptr<const frame> packet; };
inline std::array<slot, 2> slots;
inline unsigned next_slot = 0;
inline void publish(bitmap_ind16 &bitmap, std::shared_ptr<const frame> packet) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto &s : slots) if (s.base == &bitmap.pix(0)) { s.packet = std::move(packet); return; }
    slots[next_slot++ % 2] = { &bitmap.pix(0), &bitmap, std::move(packet) };
}
inline std::shared_ptr<const frame> find(const void *base) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto &s : slots) if (s.base == base) return s.packet;
    return {};
}
inline void clear() {
    std::lock_guard<std::mutex> lock(mutex);
    slots = {}; next_slot = 0;
}
// Software reference for snapshots/recordings only; normal presentation never
// reads GPU pixels back. Uses the same MAME spans and integer blend rules.
inline bool mix(uint16_t &dest, uint16_t raw, unsigned priority) {
    if (raw == 0xffff || ((raw >> 12) & 3) != priority) return false;
    const uint16_t src = (raw & 0xfff) ^ 0xf00;
    if ((src & 255) == 255) { if (dest != 255) return false; dest = src; }
    else if ((src & 255) < 2) dest = dest != 255 ? ((src & 1) ? 0x6000 : 0x4000) | (dest & 0x1fff) : src;
    else dest = 0x1000 | src;
    return true;
}
template<class Pixel, class Colour> inline void solvalou_present(std::vector<Pixel> &pixels, unsigned width, bool status, Colour colour) {
    const auto source = pixels;
    for (unsigned y = 0; y < 1080; ++y) for (unsigned x = 0; x < width; ++x) {
        const unsigned sx = (width*209/2 + x*871)/1080;
        const unsigned sy = 209 + y*871/1080;
        pixels[y*width+x] = source[sy*width+sx];
        const unsigned left=width-744;
        if(status && x>=left && x<left+720 && y>=24 && y<129) {
            const auto pixel=source[(208-(y-24)*2)*width+(width-1440)/2+(x-left)*2];
            const uint32_t rgb=colour(pixel);
            if(((rgb>>16)&255)>24 || ((rgb>>8)&255)>24 || (rgb&255)>24) pixels[y*width+x]=pixel;

        }
    }
}
inline std::vector<uint16_t> resolve(const frame &f) {
    const unsigned left = (f.width - 1440) / 2;
    std::vector<uint16_t> out(f.width * f.height, 0xff);
    std::vector<uint16_t> depths(f.width * f.height, 0x8000), pens(f.width * f.height, 0);
    std::vector<uint16_t> edge_depths(f.width * f.height, 0x8000), edge_pens(f.width * f.height, 0);
    std::vector<uint8_t> edge_coverage(f.width * f.height, 0);
    for (auto &s : expand_lines(f.spans,f.width,f.height)) for (unsigned x = s.pixels.x0; x < s.pixels.x1; ++x) {
        unsigned i = s.y * f.width + x;
        if (s.pixels.pen & 0x80000000U) {
            const uint8_t coverage = (s.pixels.pen >> 16) & 0xff;
            if (s.pixels.depth < edge_depths[i] || (s.pixels.depth == edge_depths[i] && coverage > edge_coverage[i])) {
                edge_depths[i] = s.pixels.depth; edge_pens[i] = s.pixels.pen & 0xffff; edge_coverage[i] = coverage;
            }
        } else if (s.pixels.depth < depths[i]) { depths[i] = s.pixels.depth; pens[i] = s.pixels.pen; }
    }
    for (unsigned i = 0; i < pens.size(); ++i)
        if (edge_depths[i] <= depths[i] && (((i % f.width) * 73 + (i / f.width) * 151) & 255) < edge_coverage[i])
            pens[i] = edge_pens[i];
    uint16_t priorities[16]; priorities[0] = 0x7fc0;
    for (int i = 1; i < 16; ++i) priorities[i] = priorities[i-1] / 1.24;
    for (unsigned y = 0; y < f.height; ++y) for (unsigned x = 0; x < f.width; ++x) {
        const unsigned i = y * f.width + x, sy = y * 480 / f.height;
        const int wide_x = (int(x) - int(f.auxiliary_base)) * 496 / 1440;
        const unsigned wx = std::min(f.auxiliary_width - 1, unsigned(std::max(0, wide_x)));
        const uint16_t scene = f.wide_sprites[sy * f.auxiliary_width + wx];
        const bool centre = x >= left && x < left + 1440;
        const unsigned sx = x < left ? 0 : x >= left + 1440 ? 495 : (x - left) * 496 / 1440;
        const uint32_t sprite = f.sprites[sy * 496 + sx];
        const uint16_t raw = sprite;
        // Depth-only wireframe faces occlude the background star layer.
        if (depths[i] == 0x8000) mix(out[i], scene, 2);
        if (pens[i]) out[i] = pens[i];
        {
            uint16_t mixed = out[i];
            if (mix(mixed, scene, 0) && (!f.depth_test || ((mixed & 0x5000) && priorities[(mixed >> 8) & 15] <= depths[i]) || mixed < 0x1000)) out[i] = mixed;
        }
        mix(out[i], scene, 3);
        if (centre || (sprite >> 16) == 2) {
            if (mix(out[i], raw, 3) && f.scanlines && (sprite >> 16) == 1 && out[i] >= 0x1000 && out[i] < 0x2000
                && y == ((sy + 1) * f.height + 479) / 480 - 1) out[i] = 0x8000 | (out[i] & 0xfff);
        }
    }
    if (f.solvalou_crop) solvalou_present(out, f.width, f.solvalou_status, [&](uint16_t pen) { return f.palette[pen]; });
    return out;
}
// Full-colour GPU reference including fractional edge coverage. Indexed MAME
// snapshots use resolve() above; native presentation blends this on the GPU.
inline std::vector<uint32_t> resolve_rgba(const frame &f, std::vector<uint8_t> *visible_lines = nullptr) {
    const unsigned left = (f.width - 1440) / 2;
    const unsigned pixels = f.width * f.height;
    if (visible_lines) visible_lines->assign(pixels, 0);
    std::vector<uint32_t> out(pixels);
    std::vector<uint16_t> depths(pixels, 0x8000), pens(pixels, 0);
    std::vector<uint16_t> edge_depths(pixels, 0x8000), edge_pens(pixels, 0);
    std::vector<uint8_t> edge_coverage(pixels, 0);
    for (auto &s : expand_lines(f.spans,f.width,f.height)) for (unsigned x = s.pixels.x0; x < s.pixels.x1; ++x) {
        const unsigned i = s.y * f.width + x;
        if (s.pixels.pen & 0x80000000U) {
            const uint8_t coverage = (s.pixels.pen >> 16) & 0xff;
            if (s.pixels.depth < edge_depths[i] || (s.pixels.depth == edge_depths[i] && coverage > edge_coverage[i])) {
                edge_depths[i] = s.pixels.depth; edge_pens[i] = s.pixels.pen & 0xffff; edge_coverage[i] = coverage;
            }
        } else if (s.pixels.depth < depths[i]) { depths[i] = s.pixels.depth; pens[i] = s.pixels.pen; }
    }
    uint16_t priorities[16]; priorities[0] = 0x7fc0;
    for (int i = 1; i < 16; ++i) priorities[i] = priorities[i-1] / 1.24;
    for (unsigned y = 0; y < f.height; ++y) for (unsigned x = 0; x < f.width; ++x) {
        const unsigned i = y * f.width + x, sy = y * 480 / f.height;
        const int wide_x = (int(x) - int(f.auxiliary_base)) * 496 / 1440;
        const unsigned wx = std::min(f.auxiliary_width - 1, unsigned(std::max(0, wide_x)));
        const uint16_t scene = f.wide_sprites[sy * f.auxiliary_width + wx];
        const bool centre = x >= left && x < left + 1440;
        const unsigned sx = x < left ? 0 : x >= left + 1440 ? 495 : (x - left) * 496 / 1440;
        const uint32_t sprite = f.sprites[sy * 496 + sx];
        const uint16_t raw = sprite;
        uint16_t dest = 0xff;
        if (depths[i] == 0x8000) mix(dest, scene, 2);
        uint16_t under = dest;
        if (pens[i]) dest = pens[i];
        const bool hidden_edge = f.vector_effect && edge_pens[i] != 0 && edge_depths[i] > depths[i];
        bool edge_visible = (edge_depths[i] <= depths[i] || hidden_edge) && edge_pens[i] != 0;
        if (edge_visible) dest = edge_pens[i];
        const auto priority_zero = [&](uint16_t &value) {
            uint16_t mixed = value;
            if (mix(mixed, scene, 0) && (!f.depth_test || ((mixed & 0x5000) && priorities[(mixed >> 8) & 15] <= depths[i]) || mixed < 0x1000)) {
                value = mixed; return true;
            }
            return false;
        };
        if (priority_zero(dest)) edge_visible = false;
        priority_zero(under);
        if (mix(dest, scene, 3)) edge_visible = false;
        mix(under, scene, 3);
        if (centre || (sprite >> 16) == 2) {
            const bool covered = mix(dest, raw, 3);
            if (covered) edge_visible = false;
            if (covered && f.scanlines && (sprite >> 16) == 1 && dest >= 0x1000 && dest < 0x2000
                && y == ((sy + 1) * f.height + 479) / 480 - 1) dest = 0x8000 | (dest & 0xfff);
            const bool under_covered = mix(under, raw, 3);
            if (under_covered && f.scanlines && (sprite >> 16) == 1 && under >= 0x1000 && under < 0x2000
                && y == ((sy + 1) * f.height + 479) / 480 - 1) under = 0x8000 | (under & 0xfff);
        }
        if (visible_lines) (*visible_lines)[i] = edge_visible ? 1 : 0;
        uint32_t edge_rgb = f.palette[dest];
        if (edge_visible) {
            const unsigned gain = f.wire_brightness == 1 ? 80 : f.wire_brightness == 3 ? 125 : f.wire_brightness == 0 ? 160 : 100;
            const auto channel = [gain, hidden_edge](unsigned c) {
                const unsigned lit = std::min(255U, (c * gain + 50) / 100);
                return hidden_edge ? (lit + 1) / 2 : lit;
            };
            edge_rgb = (edge_rgb & 0xff000000) | (channel((edge_rgb >> 16) & 255) << 16)
                | (channel((edge_rgb >> 8) & 255) << 8) | channel(edge_rgb & 255);
        }
        if (!edge_visible || edge_coverage[i] == 255) { out[i] = edge_rgb; continue; }
        const uint32_t base_rgb = f.palette[under];
        const unsigned coverage = edge_coverage[i], inverse = 255 - coverage;
        const unsigned r = ((((edge_rgb >> 16) & 255) * coverage + ((base_rgb >> 16) & 255) * inverse) + 127) / 255;
        const unsigned g = ((((edge_rgb >> 8) & 255) * coverage + ((base_rgb >> 8) & 255) * inverse) + 127) / 255;
        const unsigned b = (((edge_rgb & 255) * coverage + (base_rgb & 255) * inverse) + 127) / 255;
        out[i] = (r << 16) | (g << 8) | b;
    }
    if (f.solvalou_crop) solvalou_present(out, f.width, f.solvalou_status, [](uint32_t rgb) { return rgb; });
    return out;
}
inline std::vector<uint32_t> resolve_glow(const frame &f) {
    std::vector<uint64_t> light(496 * 480);
    std::vector<uint32_t> out(496 * 480);
    static constexpr unsigned weights[] = {1, 2, 1};
    for (int sy = 0; sy < 480; ++sy) for (int sx = 0; sx < 496; ++sx) {
        const uint32_t sprite = f.sprites[sy * 496 + sx];
        const uint16_t raw = sprite;
        if ((sprite >> 16) != 1 || ((raw >> 12) & 15) != 3 || (raw & 255) < 2 || (raw & 255) == 255) continue;
        const uint32_t color = f.palette[0x1000 | ((raw & 0xfff) ^ 0xf00)];
        const uint64_t emission = (uint64_t((color >> 16) & 255) << 32) | (uint64_t((color >> 8) & 255) << 16) | (color & 255);
        for (int y = std::max(0, sy - 1); y <= std::min(479, sy + 1); ++y)
            for (int x = std::max(0, sx - 1); x <= std::min(495, sx + 1); ++x)
                light[y * 496 + x] += emission * weights[y - sy + 1] * weights[x - sx + 1];
    }
    const unsigned gain = f.glow_strength == 1 ? 20 : f.glow_strength == 2 ? 35 : f.glow_strength == 3 ? 50 : 0;
    for (unsigned i = 0; i < out.size(); ++i) {
        const uint64_t v = light[i];
        out[i] = (((v >> 32) & 0xffff) * gain / 1600 << 16)
            | (((v >> 16) & 0xffff) * gain / 1600 << 8) | ((v & 0xffff) * gain / 1600);
    }
    return out;
}
inline void prepare_snapshot() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto &s : slots) if (s.packet && s.bitmap) {
        auto pixels = resolve(*s.packet);
        for (unsigned y = 0; y < s.packet->height; ++y)
            std::copy_n(pixels.data() + y * s.packet->width, s.packet->width, &s.bitmap->pix(y));
    }
}
}
