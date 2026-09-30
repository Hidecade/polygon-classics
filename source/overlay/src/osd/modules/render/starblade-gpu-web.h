// license:BSD-3-Clause
#pragma once
#ifdef SDLMAME_EMSCRIPTEN
#include <emscripten.h>
#include "starblade-gpu-frame.h"

EM_JS(int, starblade_web_ready, (), { return globalThis.starbladeWebGPU?.ready ? 1 : 0; });
EM_JS(void, starblade_web_hide, (), { globalThis.starbladeWebGPU?.hide(); });
EM_JS(void, starblade_web_trace, (unsigned count, unsigned flags, unsigned w, unsigned h,
    float x0, float y0, float x1, float y1, float r, float g, float b, float a,
    int simple, double serial, unsigned view_width, unsigned view_height), {
    const gpu = globalThis.starbladeWebGPU;
    if (!gpu?.validate) return;
    if (count === 0) gpu.primitives = [];
    gpu.primitives.push({flags, w, h, x0, y0, x1, y1, r, g, b, a, simple, serial, view_width, view_height});
});
EM_JS(int, starblade_web_validate, (double serial, unsigned spans), {
    return globalThis.starbladeWebGPU?.wantsValidation(serial, spans) ? 1 : 0;
});
EM_JS(int, starblade_web_present, (double serial, const void *spans, unsigned span_bytes,
    const void *bins, unsigned bin_bytes, const void *sprites, const void *wide_sprites,
    const void *palette, unsigned config, const void *glow, const void *expected), {
    return globalThis.starbladeWebGPU?.present(HEAPU8, serial, spans, span_bytes, bins, bin_bytes,
        sprites, wide_sprites, palette, config, glow, expected) ? 1 : 0;
});

// SDL keeps ownership of input/audio and its CPU canvas. A second canvas presents
// the GPU image. Unrecognised primitives (MAME menus, crosshairs, FPS, etc.) use
// the original software renderer, with the indexed bitmap materialised first.
inline bool starblade_web_draw(render_primitive_list &list, int width, int height) {
    starblade_gpu::available = starblade_web_ready() != 0;
    std::shared_ptr<const starblade_gpu::frame> frame;
    const render_primitive *screen = nullptr, *glow = nullptr;
    bool simple = (width == 1440 || width == 1920 || width == 2580) && height == 1080;
    unsigned primitive_index = 0;
    for (const auto &p : list) {
        if (PRIMFLAG_GET_SCREENTEX(p.flags) && !screen) {
            frame = starblade_gpu::find(p.texture.base);
            screen = &p;
            simple &= p.bounds.x0 == 0 && p.bounds.y0 == 0 && p.bounds.x1 == width && p.bounds.y1 == 1080
                && p.color.r == 1 && p.color.g == 1 && p.color.b == 1 && p.color.a == 1
                && PRIMFLAG_GET_TEXORIENT(p.flags) == 0;
        } else if (screen && !glow && p.texture.width == 496 && p.texture.height == 480
            && p.texture.rowpixels == 496 && PRIMFLAG_GET_BLENDMODE(p.flags) == BLENDMODE_ADD
            && p.bounds.x0 == (width-1440)/2 && p.bounds.x1 == (width+1440)/2 && p.bounds.y0 == 0 && p.bounds.y1 == 1080
            && p.color.r == 1 && p.color.g == 1 && p.color.b == 1 && p.color.a == 1) {
            glow = &p;
        } else if (screen || p.texture.base || p.color.r != 0 || p.color.g != 0 || p.color.b != 0) {
            simple = false;
        }
        starblade_web_trace(primitive_index++, p.flags, p.texture.width, p.texture.height,
            p.bounds.x0, p.bounds.y0, p.bounds.x1, p.bounds.y1, p.color.r, p.color.g, p.color.b, p.color.a,
            simple, frame ? double(frame->serial) : -1, width, height);
    }
    if (simple && frame && starblade_gpu::available) {
        const unsigned columns = (width+31)/32, tiles = columns * 1080;
        std::vector<uint32_t> counts(tiles), offsets(tiles), bins(tiles * 2);
        std::vector<starblade_gpu::span> spans;
        spans.reserve(frame->spans.size());
        for (const auto &s : frame->spans) {
            spans.push_back(s.pixels);
            for (unsigned x = s.pixels.x0 / 32; x <= (s.pixels.x1 - 1) / 32; ++x) ++counts[s.y * columns + x];
        }
        unsigned total = tiles * 2;
        for (unsigned i = 0; i < tiles; ++i) { offsets[i] = bins[i*2] = total; bins[i*2+1] = counts[i]; total += counts[i]; }
        bins.resize(total);
        for (unsigned i = 0; i < frame->spans.size(); ++i) {
            const auto &s = frame->spans[i];
            for (unsigned x = s.pixels.x0 / 32; x <= (s.pixels.x1 - 1) / 32; ++x) bins[offsets[s.y * columns + x]++] = i;
        }
        std::vector<uint16_t> expected;
        if (starblade_web_validate(double(frame->serial), unsigned(spans.size()))) expected = starblade_gpu::resolve(*frame);
        if (starblade_web_present(double(frame->serial), spans.data(), spans.size()*sizeof(spans[0]),
            bins.data(), bins.size()*4, frame->sprites.data(), frame->wide_sprites.data(), frame->palette.data(),
            frame->depth_test | (frame->scanlines ? 2 : 0) | (frame->solvalou_crop ? 4 : 0) | (frame->solvalou_status ? 8 : 0), glow ? glow->texture.base : nullptr,
            expected.empty() ? nullptr : expected.data())) return true;
    }
    starblade_web_hide();
    if (frame) starblade_gpu::prepare_snapshot();
    return false;
}
#endif
