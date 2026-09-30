// license:BSD-3-Clause
#pragma once
#include "starblade-gpu-bins.h"
#include "starblade-gpu-frame.h"
#include "starblade-gpu-shader.h"
#include "starblade-gpu-glow-shader.h"
#include "starblade-wire-glow-shader.h"
#include "starblade-gpu-metal.bin.h"
#include "starblade-gpu-glow-metal.bin.h"
#include "starblade-wire-glow-horizontal-metal.bin.h"
#include "starblade-wire-glow-vertical-metal.bin.h"
#include "starblade-wire-glow-composite-metal.bin.h"
#include <fstream>
#include <cstdlib>
#include <chrono>
#ifdef OSD_WINDOWS
#include <d3dcompiler.h>
#endif

class starblade_gpu_renderer {
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_glow_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_wire_blur_program = BGFX_INVALID_HANDLE, m_wire_vertical_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_wire_composite_program = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_inputs[5] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    bgfx::TextureHandle m_output = BGFX_INVALID_HANDLE, m_glow = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_wire_mask = BGFX_INVALID_HANDLE, m_wire_blur = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_wire_soft = BGFX_INVALID_HANDLE, m_wire_present = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_readback = BGFX_INVALID_HANDLE, m_glow_readback = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_wire_readback = BGFX_INVALID_HANDLE, m_mask_readback = BGFX_INVALID_HANDLE;
    bool m_capture_reset = false;
    std::vector<uint8_t> m_mask_pixels;
    unsigned m_width[5] = {}, m_height[5] = {};
    bool m_attempted = false;
    const void *m_base = nullptr, *m_glow_base = nullptr;
    uint64_t m_serial = 0, m_capture_serial = 0;
    uint32_t m_frame = 0, m_ready = 0;
    unsigned m_output_width = 1920;
    bool m_wire_glow_active = false, m_vector_active = false;
    uint64_t m_vector_serial = 0;
    bgfx::TextureHandle m_vector_history = BGFX_INVALID_HANDLE, m_vector_next = BGFX_INVALID_HANDLE;
    std::shared_ptr<const starblade_gpu::frame> m_pending;
    std::vector<uint8_t> m_pixels, m_glow_pixels, m_wire_pixels;
    std::vector<uint32_t> m_counts, m_offsets, m_bins, m_palette_upload;
    std::vector<uint64_t> m_references;
    std::vector<starblade_gpu::span> m_spans;
    const bool m_profile = std::getenv("STARBLADE_GPU_PROFILE") != nullptr;

#ifdef OSD_WINDOWS
    static bgfx::ShaderHandle compile_hlsl(decltype(&D3DCompile) compile, const char *source, size_t length) {
        ID3DBlob *code = nullptr, *errors = nullptr;
        HRESULT hr = compile ? compile(source, length, "starblade", nullptr, nullptr, "main", "cs_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors) : E_FAIL;
        if (FAILED(hr)) osd_printf_warning("StarBlade GPU: shader compile failed: %s\n",
            errors ? static_cast<const char *>(errors->GetBufferPointer()) : "compiler entry point unavailable");
        if (errors) errors->Release();
        if (FAILED(hr)) return BGFX_INVALID_HANDLE;
        std::vector<uint8_t> binary = {'C','S','H',11, 0,0,0,0, 0,0,0,0, 0,0};
        uint32_t size = uint32_t(code->GetBufferSize());
        for (int i = 0; i < 4; ++i) binary.push_back(size >> (i * 8));
        const auto *data = static_cast<const uint8_t *>(code->GetBufferPointer());
        binary.insert(binary.end(), data, data + size);
        binary.insert(binary.end(), {0, 0, 0, 0});
        code->Release();
        return bgfx::createShader(bgfx::copy(binary.data(), uint32_t(binary.size())));
    }
#endif

    bool initialize() {
        if (m_attempted) return bgfx::isValid(m_program) && bgfx::isValid(m_glow_program)
            && bgfx::isValid(m_wire_blur_program) && bgfx::isValid(m_wire_vertical_program)
            && bgfx::isValid(m_wire_composite_program)
            && bgfx::isValid(m_output) && bgfx::isValid(m_glow)
            && bgfx::isValid(m_wire_mask) && bgfx::isValid(m_wire_blur)
            && bgfx::isValid(m_wire_soft) && bgfx::isValid(m_wire_present) && bgfx::isValid(m_vector_history) && bgfx::isValid(m_vector_next);
        m_attempted = true;
        const auto *caps = bgfx::getCaps();
        const auto renderer = bgfx::getRendererType();
        if ((renderer != bgfx::RendererType::Direct3D11 && renderer != bgfx::RendererType::Metal)
            || !(caps->supported & BGFX_CAPS_COMPUTE)
            || !(caps->formats[bgfx::TextureFormat::RGBA8] & BGFX_CAPS_FORMAT_TEXTURE_IMAGE_WRITE)) return false;
        bgfx::ShaderHandle shader = BGFX_INVALID_HANDLE, glow_shader = BGFX_INVALID_HANDLE;
        bgfx::ShaderHandle wire_blur_shader = BGFX_INVALID_HANDLE, wire_vertical_shader = BGFX_INVALID_HANDLE;
        bgfx::ShaderHandle wire_composite_shader = BGFX_INVALID_HANDLE;
#ifdef OSD_WINDOWS
        if (renderer == bgfx::RendererType::Direct3D11) {
            HMODULE dll = LoadLibraryW(L"d3dcompiler_47.dll");
            if (!dll) { osd_printf_warning("StarBlade GPU: shader compiler unavailable; using CPU\n"); return false; }
            auto compile = reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(dll, "D3DCompile"));
            shader = compile_hlsl(compile, starblade_compute_hlsl, sizeof(starblade_compute_hlsl) - 1);
            glow_shader = compile_hlsl(compile, starblade_glow_hlsl, sizeof(starblade_glow_hlsl) - 1);
            wire_blur_shader = compile_hlsl(compile, starblade_wire_glow_horizontal_hlsl, sizeof(starblade_wire_glow_horizontal_hlsl) - 1);
            wire_vertical_shader = compile_hlsl(compile, starblade_wire_glow_vertical_hlsl, sizeof(starblade_wire_glow_vertical_hlsl) - 1);
            wire_composite_shader = compile_hlsl(compile, starblade_wire_glow_composite_hlsl, sizeof(starblade_wire_glow_composite_hlsl) - 1);
            FreeLibrary(dll);
        }
#endif
        if (renderer == bgfx::RendererType::Metal) {
            shader = bgfx::createShader(bgfx::copy(starblade_compute_metal, sizeof(starblade_compute_metal)));
            glow_shader = bgfx::createShader(bgfx::copy(starblade_glow_metal, sizeof(starblade_glow_metal)));
            wire_blur_shader = bgfx::createShader(bgfx::copy(starblade_wire_glow_horizontal_metal, sizeof(starblade_wire_glow_horizontal_metal)));
            wire_vertical_shader = bgfx::createShader(bgfx::copy(starblade_wire_glow_vertical_metal, sizeof(starblade_wire_glow_vertical_metal)));
            wire_composite_shader = bgfx::createShader(bgfx::copy(starblade_wire_glow_composite_metal, sizeof(starblade_wire_glow_composite_metal)));
        }
        if (!bgfx::isValid(shader) || !bgfx::isValid(glow_shader)
            || !bgfx::isValid(wire_blur_shader) || !bgfx::isValid(wire_vertical_shader)
            || !bgfx::isValid(wire_composite_shader)) {
            if (bgfx::isValid(shader)) bgfx::destroy(shader);
            if (bgfx::isValid(glow_shader)) bgfx::destroy(glow_shader);
            if (bgfx::isValid(wire_blur_shader)) bgfx::destroy(wire_blur_shader);
            if (bgfx::isValid(wire_vertical_shader)) bgfx::destroy(wire_vertical_shader);
            if (bgfx::isValid(wire_composite_shader)) bgfx::destroy(wire_composite_shader);
            return false;
        }
        m_program = bgfx::createProgram(shader, true);
        m_glow_program = bgfx::createProgram(glow_shader, true);
        m_wire_blur_program = bgfx::createProgram(wire_blur_shader, true);
        m_wire_vertical_program = bgfx::createProgram(wire_vertical_shader, true);
        m_wire_composite_program = bgfx::createProgram(wire_composite_shader, true);
        if (!bgfx::isValid(m_program) || !bgfx::isValid(m_glow_program)
            || !bgfx::isValid(m_wire_blur_program) || !bgfx::isValid(m_wire_vertical_program)
            || !bgfx::isValid(m_wire_composite_program)) return false;
        m_output_width = std::getenv("STARBLADE_FOUR_THREE") ? 1440
            : std::getenv("STARBLADE_ULTRAWIDE") ? 2580 : 1920;
        m_output = bgfx::createTexture2D(m_output_width, 1080, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        if (!bgfx::isValid(m_output)) return false;
        const uint64_t wire_flags = BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
            | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        m_wire_mask = bgfx::createTexture2D(m_output_width, 1080, false, 1, bgfx::TextureFormat::RGBA8, wire_flags);
        m_wire_blur = bgfx::createTexture2D((m_output_width + 1) / 2, 540, false, 1, bgfx::TextureFormat::RGBA8, wire_flags);
        m_wire_soft = bgfx::createTexture2D((m_output_width + 1) / 2, 540, false, 1, bgfx::TextureFormat::RGBA8, wire_flags);
        m_vector_next = bgfx::createTexture2D(m_output_width, 1080, false, 1, bgfx::TextureFormat::RGBA8, wire_flags);
        m_vector_history = bgfx::createTexture2D(m_output_width, 1080, false, 1, bgfx::TextureFormat::RGBA8, wire_flags);
        m_wire_present = bgfx::createTexture2D(m_output_width, 1080, false, 1, bgfx::TextureFormat::RGBA8, wire_flags);
        if (!bgfx::isValid(m_wire_mask) || !bgfx::isValid(m_wire_blur)
            || !bgfx::isValid(m_wire_soft) || !bgfx::isValid(m_wire_present) || !bgfx::isValid(m_vector_history) || !bgfx::isValid(m_vector_next)) return false;
        m_glow = bgfx::createTexture2D(496, 480, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        if (!bgfx::isValid(m_glow)) return false;
        osd_printf_info("StarBlade GPU: %s compute rasterizer enabled (%ux1080; no per-frame readback)\n",
            renderer == bgfx::RendererType::Metal ? "Metal" : "Direct3D 11", m_output_width);
        osd_printf_info("StarBlade GPU: native UI glow compute enabled (496x480)\n");
        osd_printf_info("StarBlade GPU: 3D wire glow compute enabled (visible edges only)\n");
        starblade_gpu::lines_available = std::getenv("STARBLADE_CPU_LINES") == nullptr;
        osd_printf_info("StarBlade GPU: analytic line generation %s\n", starblade_gpu::lines_available ? "enabled" : "disabled (CPU comparison)");
        starblade_gpu::available = true;
        starblade_gpu::glow_available = true;
        return true;
    }
    void upload(int index, unsigned width, unsigned height, bgfx::TextureFormat::Enum format, const void *data, uint32_t bytes) {
        if (m_width[index] != width || m_height[index] < height) {
            if (bgfx::isValid(m_inputs[index])) bgfx::destroy(m_inputs[index]);
            m_inputs[index] = bgfx::createTexture2D(width, height, false, 1, format,
                BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
            m_width[index] = width; m_height[index] = height;
        }
        bgfx::updateTexture2D(m_inputs[index], 0, 0, 0, 0, width, height, bgfx::copy(data, bytes));
        bgfx::setImage(index, m_inputs[index], 0, bgfx::Access::Read, format);
    }
    void finish_capture() {
        if (!m_pending || m_frame < m_ready) return;
        std::vector<uint8_t> visible_lines;
        auto expected = starblade_gpu::resolve_rgba(*m_pending, &visible_lines);
        const bool analytic = std::any_of(m_pending->spans.begin(),m_pending->spans.end(),
            [](const auto &s){return starblade_gpu::is_line(s.pixels);});
        unsigned differences = 0, rounding_pixels = 0, max_error = 0;
        for (unsigned i = 0; i < expected.size(); ++i) {
            uint32_t rgb = expected[i];
            const unsigned error=std::max({unsigned(std::abs(int(m_pixels[4*i])-int((rgb>>16)&255))),
                unsigned(std::abs(int(m_pixels[4*i+1])-int((rgb>>8)&255))),
                unsigned(std::abs(int(m_pixels[4*i+2])-int(rgb&255)))});
            max_error=std::max(max_error,error);
            // Float GPU distance vs double CPU reference: at most one colour
            // step, only on independently identified visible line pixels.
            const bool rounding=analytic && visible_lines[i] && error==1 && m_pixels[4*i+3]==255;
            rounding_pixels+=rounding;
            const bool mismatch=(!rounding && error!=0) || m_pixels[4*i+3]!=255;
            if (mismatch && differences < 4) osd_printf_info(
                "STARBLADE_GPU_MISMATCH frame=%llu x=%u y=%u gpu=%02x%02x%02x cpu=%06x\n",
                (unsigned long long)m_pending->serial, i % m_pending->width, i / m_pending->width,
                m_pixels[4*i], m_pixels[4*i+1], m_pixels[4*i+2], rgb & 0xffffff);
            differences += mismatch;
        }
        osd_printf_info("STARBLADE_GPU_VALIDATE frame=%llu spans=%u pixels=%u differences=%u\n",
            (unsigned long long)m_pending->serial, unsigned(m_pending->spans.size()), unsigned(expected.size()), differences);
        osd_printf_info("STARBLADE_GPU_LINE_ROUNDING frame=%llu pixels=%u max_channel_error=%u\n",
            (unsigned long long)m_pending->serial,rounding_pixels,max_error);
        if (!m_wire_pixels.empty()) {
            unsigned brightened = 0, darkened = 0;
            for (unsigned i = 0; i < expected.size() * 4; i += 4) for (unsigned c = 0; c < 3; ++c) {
                brightened += m_wire_pixels[i+c] > m_pixels[i+c];
                darkened += m_wire_pixels[i+c] < m_pixels[i+c];
            }
            osd_printf_info("STARBLADE_WIRE_GLOW_VALIDATE frame=%llu brightened=%u darkened=%u\n",
                (unsigned long long)m_pending->serial, brightened, darkened);
        }
        if (m_pending->vector_effect && !m_wire_pixels.empty() && !m_mask_pixels.empty()) {
            unsigned protected_pixels = 0, protected_changes = 0, trail_pixels = 0, darkened = 0;
            for (unsigned i = 0; i < m_pixels.size(); i += 4) {
                const bool protected_pixel = m_mask_pixels[i+3] == 255;
                const bool line = m_mask_pixels[i] || m_mask_pixels[i+1] || m_mask_pixels[i+2];
                bool changed = false;
                for (unsigned c = 0; c < 3; ++c) {
                    changed |= m_wire_pixels[i+c] != m_pixels[i+c];
                    darkened += m_wire_pixels[i+c] < m_pixels[i+c];
                }
                protected_pixels += protected_pixel;
                protected_changes += protected_pixel && changed;
                trail_pixels += !line && changed;
            }
            osd_printf_info("STARBLADE_VECTOR_VALIDATE frame=%llu reset=%u protected=%u protected_changes=%u trail=%u darkened=%u\n",
                (unsigned long long)m_pending->serial, m_capture_reset, protected_pixels, protected_changes, trail_pixels, darkened);
        }
        if (m_pending->glow_strength && !m_glow_pixels.empty()) {
            const auto glow = starblade_gpu::resolve_glow(*m_pending);
            unsigned glow_differences = 0;
            for (unsigned i = 0; i < glow.size(); ++i)
                glow_differences += m_glow_pixels[4*i] != ((glow[i] >> 16) & 255)
                    || m_glow_pixels[4*i+1] != ((glow[i] >> 8) & 255) || m_glow_pixels[4*i+2] != (glow[i] & 255);
            osd_printf_info("STARBLADE_GPU_GLOW_VALIDATE frame=%llu pixels=%u differences=%u\n",
                (unsigned long long)m_pending->serial, unsigned(glow.size()), glow_differences);
        }
        if (const char *dir = std::getenv("STARBLADE_GPU_CAPTURE")) {
            std::ofstream file(std::string(dir) + "/gpu-" + std::to_string(m_pending->serial) + ".rgba", std::ios::binary);
            file.write(reinterpret_cast<const char *>(m_pixels.data()), m_pixels.size());
            if (!m_wire_pixels.empty()) {
                std::ofstream effect(std::string(dir) + "/vector-" + std::to_string(m_pending->serial) + ".rgba", std::ios::binary);
                effect.write(reinterpret_cast<const char *>(m_wire_pixels.data()), m_wire_pixels.size());
            }
        }
        m_pending.reset();
    }
public:
    ~starblade_gpu_renderer() {
        // readTexture owns the destination pointer until its returned frame.
        // Drain an outstanding diagnostic capture before freeing that memory.
        if (m_pending) {
            while (m_frame < m_ready) m_frame = bgfx::frame();
            finish_capture();
        }
        starblade_gpu::available = false;
        starblade_gpu::lines_available = false;
        starblade_gpu::glow_available = false;
        starblade_gpu::clear();
        for (auto t : m_inputs) if (bgfx::isValid(t)) bgfx::destroy(t);
        if (bgfx::isValid(m_output)) bgfx::destroy(m_output);
        if (bgfx::isValid(m_wire_mask)) bgfx::destroy(m_wire_mask);
        if (bgfx::isValid(m_wire_blur)) bgfx::destroy(m_wire_blur);
        if (bgfx::isValid(m_wire_soft)) bgfx::destroy(m_wire_soft);
        if (bgfx::isValid(m_vector_next)) bgfx::destroy(m_vector_next);
        if (bgfx::isValid(m_vector_history)) bgfx::destroy(m_vector_history);
        if (bgfx::isValid(m_wire_present)) bgfx::destroy(m_wire_present);
        if (bgfx::isValid(m_glow)) bgfx::destroy(m_glow);
        if (bgfx::isValid(m_readback)) bgfx::destroy(m_readback);
        if (bgfx::isValid(m_glow_readback)) bgfx::destroy(m_glow_readback);
        if (bgfx::isValid(m_mask_readback)) bgfx::destroy(m_mask_readback);
        if (bgfx::isValid(m_wire_readback)) bgfx::destroy(m_wire_readback);
        if (bgfx::isValid(m_program)) bgfx::destroy(m_program);
        if (bgfx::isValid(m_glow_program)) bgfx::destroy(m_glow_program);
        if (bgfx::isValid(m_wire_blur_program)) bgfx::destroy(m_wire_blur_program);
        if (bgfx::isValid(m_wire_vertical_program)) bgfx::destroy(m_wire_vertical_program);
        if (bgfx::isValid(m_wire_composite_program)) bgfx::destroy(m_wire_composite_program);
    }
    void advanced(uint32_t frame) { m_frame = frame; }
    bool matches(const void *base) const { return m_base && base == m_base; }
    bool matches_glow(const void *base) const { return m_glow_base && base == m_glow_base; }
    bgfx::TextureHandle texture() const { return (m_wire_glow_active || m_vector_active) ? m_wire_present : m_output; }
    bgfx::TextureHandle glow_texture() const { return m_glow; }
    void prepare(uint32_t &view, render_primitive *first, bool starblade) {
        m_base = m_glow_base = nullptr;
        finish_capture();
        if (!starblade || !initialize()) return;
        std::shared_ptr<const starblade_gpu::frame> f;
        for (auto *p = first; p; p = p->next()) if (PRIMFLAG_GET_SCREENTEX(p->flags)) {
            f = starblade_gpu::find(p->texture.base);
            if (f) { m_base = p->texture.base; break; }
        }
        if (f && f->glow_strength) for (auto *p = first; p; p = p->next())
            if (!PRIMFLAG_GET_SCREENTEX(p->flags) && p->texture.width == 496 && p->texture.height == 480
                && p->texture.rowpixels == 496 && PRIMFLAG_GET_BLENDMODE(p->flags) == BLENDMODE_ADD) {
                m_glow_base = p->texture.base; break;
            }
        if (!f) { m_vector_serial = 0; return; }
        if (m_serial == f->serial) return;
        m_vector_active = f->vector_effect != 0;
        const bool reset_vector = m_vector_serial + 1 != f->serial || m_vector_serial == 0;
        m_vector_serial = m_vector_active ? f->serial : 0;
        m_serial = f->serial;
        m_wire_glow_active = !m_vector_active && f->wire_glow_radius != 0 && f->wire_glow_level != 0
            && std::any_of(f->spans.begin(), f->spans.end(), [](const auto &s) {
            return (s.pixels.pen & 0x80000000U) != 0;
        });
        const unsigned tiles_x = (f->width + 31) / 32;
        const unsigned tiles = tiles_x * 1080;
        const auto begin_pack = std::chrono::steady_clock::now();
        auto &counts=m_counts; auto &offsets=m_offsets; auto &bins=m_bins; auto &spans=m_spans;
        unsigned line_count=0, reference_count=0;
        if(f->solvalou_crop && std::none_of(f->spans.begin(),f->spans.end(),[](const auto &s){return starblade_gpu::is_line(s.pixels);})) {
            reference_count=starblade_pack_solid(f->spans,f->width,f->height,counts,offsets,bins,spans);
        } else {
        counts.assign(tiles,0); offsets.resize(tiles); bins.resize(tiles*2);
        spans.clear(); spans.reserve(f->spans.size()+2048); m_references.clear();
        const auto reference = [&](unsigned index,unsigned y,unsigned left,unsigned right) {
            for(unsigned x=left/32;x<=(right-1)/32;++x) {
                const unsigned tile=y*tiles_x+x;
                ++counts[tile];m_references.push_back((uint64_t(tile)<<32)|index);
            }
        };
        for(unsigned i=0;i<f->spans.size();++i) {
            const auto &s=f->spans[i];spans.push_back(s.pixels);
            if(starblade_gpu::is_line(s.pixels)) {
                const auto &endpoint=f->spans[i+1].pixels;spans.push_back(endpoint);++line_count;
                starblade_gpu::line_rows(s.pixels,endpoint,f->width,f->height,
                    [&](unsigned y,unsigned left,unsigned right){reference(i,y,left,right);});
                ++i;
            } else reference(i,s.y,s.pixels.x0,s.pixels.x1);
        }
        unsigned total=tiles*2;
        for(unsigned i=0;i<tiles;++i){offsets[i]=bins[i*2]=total;bins[i*2+1]=counts[i];total+=counts[i];}
        bins.resize((total+2047)/2048*2048);
        for(uint64_t ref:m_references) bins[offsets[ref>>32]++]=uint32_t(ref);
            reference_count=unsigned(m_references.size());
        }
        const unsigned payload_records=unsigned(spans.size());
        spans.resize(std::max<size_t>(2048,(spans.size()+2047)/2048*2048));
        const auto packed=std::chrono::steady_clock::now();
        upload(0,2048,unsigned(spans.size()/2048),bgfx::TextureFormat::RGBA32U,spans.data(),uint32_t(spans.size()*sizeof(spans[0])));
        upload(1,2048,unsigned(bins.size()/2048),bgfx::TextureFormat::R32U,bins.data(),uint32_t(bins.size()*4));
        if(m_profile && f->serial%120==0) {
            const auto uploaded=std::chrono::steady_clock::now();
            osd_printf_info("STARBLADE_GPU_PROFILE frame=%llu lines=%u records=%u refs=%u bytes=%u pack_us=%lld submit_us=%lld\n",
                (unsigned long long)f->serial,line_count,payload_records,reference_count,unsigned(spans.size()*16+bins.size()*4),
                (long long)std::chrono::duration_cast<std::chrono::microseconds>(packed-begin_pack).count(),
                (long long)std::chrono::duration_cast<std::chrono::microseconds>(uploaded-packed).count());
        }
        upload(2, 496, 480, bgfx::TextureFormat::R32U, f->sprites.data(), f->sprites.size()*4);
        // Two native 16-bit scene pixels share one R32U texel.
        upload(3, (f->auxiliary_width + 1) / 2, 480, bgfx::TextureFormat::R32U, f->wide_sprites.data(), f->wide_sprites.size()*2);
        auto &palette = m_palette_upload; palette.assign(f->palette.begin(), f->palette.end()); palette.resize(256*145);
        palette[256*144] = f->depth_test | (f->scanlines ? 2 : 0) | (f->glow_strength << 2);
        uint16_t pri = 0x7fc0;
        for (int i = 0; i < 16; ++i) { palette[256*144+1+i] = pri; pri = pri / 1.24; }
        palette[256*144+17] = f->width;
        palette[256*144+18] = uint32_t(f->auxiliary_base);
        palette[256*144+19] = f->auxiliary_width;
        palette[256*144+20] = std::min(f->wire_glow_radius, 10U);
        palette[256*144+21] = std::min(f->wire_glow_level, 3U);
        palette[256*144+22] = f->wire_brightness;
        palette[256*144+23] = m_vector_active;
        palette[256*144+24] = reset_vector;
        palette[256*144+25] = uint32_t(f->serial);
        palette[256*144+26] = f->solvalou_crop;
        palette[256*144+27] = f->solvalou_status;
        upload(4, 256, 145, bgfx::TextureFormat::R32U, palette.data(), palette.size()*4);
        bgfx::setImage(5, m_output, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
        bgfx::setImage(6, m_wire_mask, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
        bgfx::setViewName(view, "StarBlade GPU raster + sprite composition");
        bgfx::dispatch(view++, m_program, tiles_x, 1080, 1);
        if (m_wire_glow_active) {
            bgfx::setImage(0, m_wire_mask, 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(1, m_wire_blur, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(2, m_inputs[4], 0, bgfx::Access::Read, bgfx::TextureFormat::R32U);
            bgfx::setViewName(view, "StarBlade GPU wire glow horizontal blur");
            bgfx::dispatch(view++, m_wire_blur_program, ((m_output_width + 1) / 2 + 31) / 32, 540, 1);
            bgfx::setImage(0, m_wire_blur, 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(1, m_wire_soft, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(2, m_inputs[4], 0, bgfx::Access::Read, bgfx::TextureFormat::R32U);
            bgfx::setViewName(view, "StarBlade GPU wire glow vertical blur");
            bgfx::dispatch(view++, m_wire_vertical_program, ((m_output_width + 1) / 2 + 31) / 32, 540, 1);
            bgfx::setImage(0, m_output, 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(1, m_wire_soft, 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(2, m_wire_present, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(3, m_inputs[4], 0, bgfx::Access::Read, bgfx::TextureFormat::R32U);
            bgfx::setImage(4, m_wire_mask, 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(5, m_vector_next, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
            bgfx::setViewName(view, "StarBlade GPU wire glow composite");
            bgfx::dispatch(view++, m_wire_composite_program, tiles_x, 1080, 1);
        }
        if (m_vector_active) {
            bgfx::setImage(0, m_output, 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(1, m_vector_history, 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(2, m_wire_present, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(3, m_inputs[4], 0, bgfx::Access::Read, bgfx::TextureFormat::R32U);
            bgfx::setImage(4, m_wire_mask, 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA8);
            bgfx::setImage(5, m_vector_next, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
            bgfx::setViewName(view, "StarBlade GPU vector phosphor");
            bgfx::dispatch(view++, m_wire_composite_program, tiles_x, 1080, 1);
            std::swap(m_vector_history, m_vector_next);
        }
        if (f->glow_strength && m_glow_base) {
            bgfx::setImage(0, m_inputs[2], 0, bgfx::Access::Read, bgfx::TextureFormat::R32U);
            bgfx::setImage(1, m_inputs[4], 0, bgfx::Access::Read, bgfx::TextureFormat::R32U);
            bgfx::setImage(2, m_glow, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
            bgfx::setViewName(view, "StarBlade GPU UI glow");
            bgfx::dispatch(view++, m_glow_program, 31, 30, 1);
        }
        if (std::getenv("STARBLADE_GPU_VALIDATE") && !m_pending && f->serial != m_capture_serial
            && ((m_vector_active && reset_vector) || f->serial % 600 == 0 || f->serial == 2500 || f->serial == 2840 || f->serial == 2835 || f->serial == 3040 || f->serial == 3900)) {
            if (!bgfx::isValid(m_readback)) m_readback = bgfx::createTexture2D(m_output_width,1080,false,1,bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
            m_pixels.resize(m_output_width*1080*4);
            bgfx::blit(view++, m_readback, 0, 0, m_output);
            m_ready = bgfx::readTexture(m_readback, m_pixels.data());
            m_capture_reset = reset_vector;
            if (m_vector_active) {
                if (!bgfx::isValid(m_mask_readback)) m_mask_readback = bgfx::createTexture2D(m_output_width,1080,false,1,bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
                m_mask_pixels.resize(m_output_width*1080*4);
                bgfx::blit(view++, m_mask_readback, 0, 0, m_wire_mask);
                m_ready = std::max(m_ready, bgfx::readTexture(m_mask_readback, m_mask_pixels.data()));
            } else m_mask_pixels.clear();
            if (m_wire_glow_active || m_vector_active) {
                if (!bgfx::isValid(m_wire_readback)) m_wire_readback = bgfx::createTexture2D(m_output_width,1080,false,1,bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
                m_wire_pixels.resize(m_output_width*1080*4);
                bgfx::blit(view++, m_wire_readback, 0, 0, m_wire_present);
                m_ready = std::max(m_ready, bgfx::readTexture(m_wire_readback, m_wire_pixels.data()));
            } else m_wire_pixels.clear();
            if (f->glow_strength && m_glow_base) {
                if (!bgfx::isValid(m_glow_readback)) m_glow_readback = bgfx::createTexture2D(496,480,false,1,bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
                m_glow_pixels.resize(496*480*4);
                bgfx::blit(view++, m_glow_readback, 0, 0, m_glow);
                m_ready = std::max(m_ready, bgfx::readTexture(m_glow_readback, m_glow_pixels.data()));
            } else m_glow_pixels.clear();
            m_pending = f; m_capture_serial = f->serial;
        }
    }
};
