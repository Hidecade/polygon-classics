// System 22 immutable Metal packets. Producer state is emulation-thread-only.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
namespace system22_gpu {
inline unsigned width=1920;
inline int rotation_extra_x=0, rotation_extra_y=0;
constexpr unsigned height=1080,native_width=640,native_height=480;
struct vertex { float x,y,z,u,v,i; uint32_t material; };
struct material {
    float left,top,right,bottom;
    uint32_t palette,bank,cmode,priority,flags,fogfactor,fogcolor,pad;
};
struct assets {
    std::vector<uint16_t> map;
    std::vector<uint8_t> attributes,texels;
    std::array<uint8_t,4096> lookup;
    std::array<uint8_t,768> gamma;
};
struct config {
    uint32_t fade_r=256,fade_g=256,fade_b=256,shadow=0;
    uint32_t shadow_colors[3]{};
    uint32_t background=0;
};
struct frame {
    unsigned output_width=width;
    double emulation_speed=0;
    std::string cpu_profile;
    std::shared_ptr<const assets> textures;
    std::vector<vertex> vertices;
    std::vector<material> materials;
    std::array<uint32_t,32768> palette;
    std::vector<uint32_t> hud;
    config mix;
};
static_assert(sizeof(vertex)==28 && sizeof(material)==48 && sizeof(config)==32);
inline std::atomic<bool> available{false};
inline bool enabled=false;
inline std::shared_ptr<const assets> texture_data;
inline std::shared_ptr<frame> collecting;
inline std::shared_ptr<const frame> published;
inline void clear() { collecting.reset();published.reset();texture_data.reset(); }
}
