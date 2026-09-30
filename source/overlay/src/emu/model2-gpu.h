// license:BSD-3-Clause
// Immutable Model 2 packets; all producer globals belong to the emulation thread.
#pragma once
#include <array>
#include <cmath>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
namespace model2_gpu {
inline unsigned width=1920;
constexpr double scale_x=1440.0/496, scale_y=1080.0/384;
inline unsigned rotation_extra_x=0, rotation_extra_y=0;
// Native-coordinate coverage of the inverse-rotated output for roll +/-45 degrees.
inline void rotation_coverage(bool active) {
    rotation_extra_x=active ? unsigned(std::ceil((std::hypot(double(width),1080.)-width)/(2*scale_x))) : 0;
    rotation_extra_y=active ? unsigned(std::ceil(((width+1080.)/std::sqrt(2.)-1080.)/(2*scale_y))) : 0;
}
inline unsigned margin() { return unsigned((width-1440)/2/scale_x+1)+rotation_extra_x; }
inline bool enabled=false;
inline std::atomic<bool> available{false};
struct vertex { float x,y,ooz,uoz,voz; uint32_t material; };
struct material {
    float left,top,right,bottom;
    uint32_t flags,color,luma,lumabase;
    uint32_t texwidth,texheight,texx,texy;
    uint32_t sheet,wrapx,wrapy,mirrorx,mirrory,utex,utexminlod,utexx,utexy;
    int32_t texlod;
};
struct scene { std::vector<vertex> vertices; std::vector<material> materials; };
struct frame {
    unsigned output_width=width, tile_width=496+2*margin();
    double emulation_speed=0;
    std::shared_ptr<scene> polygons=std::make_shared<scene>();
    std::vector<uint32_t> background,hud,textures;
    std::array<uint16_t,8192> palette;
    std::array<uint16_t,24576> colorxlat;
    std::array<uint8_t,32768> luma;
    std::array<uint8_t,256> gamma;
};
static_assert(sizeof(vertex)==24 && sizeof(material)==88);
inline std::shared_ptr<frame> collecting;
inline std::shared_ptr<const frame> published;
inline std::shared_ptr<scene> previous;
inline void clear() { collecting.reset(); published.reset(); previous.reset(); }
}
