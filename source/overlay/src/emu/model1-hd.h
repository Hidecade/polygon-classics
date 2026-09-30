// Immutable Model 1 render packets; producer state belongs to the emulation thread.
#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include <vector>
namespace model1_hd {
inline int width=1920;
inline constexpr int height=1080, native_width=496, native_height=384;
inline constexpr float scale_x=1440.f/496, scale_y=1080.f/384;
inline float offset_x=240;
inline int extra=83;
inline int extra_y=0;
inline int background_width=native_width+extra*2;
struct vertex { float x,y; uint32_t color; float left,top,right,bottom; };
struct frame { bool virtua_racing=true; int output_width=width, tile_width=background_width; std::vector<vertex> below,above; std::vector<uint32_t> background,hud; std::array<uint16_t,8> tile_registers{}; std::vector<uint16_t> sky_tiles; int sky_phase[2]={-1,-1}; };
inline std::shared_ptr<frame> collecting;
inline std::shared_ptr<const frame> published;
inline bool above=false, enabled=false;
inline const uint32_t *pixels=nullptr;
inline int stride=0;
inline void clear() { pixels=nullptr; stride=0; collecting.reset(); published.reset(); }
}
