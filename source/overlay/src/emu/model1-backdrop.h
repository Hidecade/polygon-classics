#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace model1_backdrop {
// VR's native race HUD has fixed TIME and SPEED labels. Attract races,
// rankings and course selection do not. Inspect the unrotated HUD from this
// exact frame, so a saved state or return to the demo needs no latched UI flag.
inline bool vrRaceHUD(const std::vector<uint32_t> &hud) {
    if (hud.size()!=496*384) return false;
    constexpr uint64_t time[7]={0x1fec36ff,0x6e7618,0x6ff618,0xfedb618,0x6c3618,0x6c3618,0x1fec3618};
    constexpr uint64_t speed[7]={0x7f7fbfcfe7e,0xc30180d86c3,0xc30180d8603,0xc33f9fcfe7e,0xc30180c06c0,0xc30180c06c3,0x7f7fbfc067e};
    const auto matches=[&](int x,int y,int width,const uint64_t *glyph) {
        for(int row=0;row<7;++row) {
            uint64_t mask=0;
            for(int col=0;col<width;++col)
                if(hud[(y+row)*496+x+col]==0xffffffff) mask|=uint64_t(1)<<col;
            if(mask!=glyph[row]) return false;
        }
        return true;
    };
    return matches(233,13,31,time) && matches(34,341,45,speed);
}

// The V.R. HUD plate splits vertically into horizontal bands. Each row is
// either an opaque piece with a blue/black surround or an entirely open gap.
// Do not require the gaps to close before extending the plate's sides.
inline bool titlePlate(const std::vector<uint32_t> &hud) {
    if (hud.size()!=496*384) return false;
    uint32_t surround=0;
    bool foundPlate=false;
    for (int y=0;y<384;++y) {
        const auto edge=hud[y*496];
        const bool opaque=(edge>>24)==255;
        if (opaque) {
            if ((edge & 0xffffff00)!=0xff000000) return false;
            if (foundPlate && edge!=surround) return false;
            surround=edge;
            foundPlate=true;
        } else if ((edge>>24)!=0) return false;
        for (int x=0;x<496;++x) {
            const auto c=hud[y*496+x];
            if ((c>>24)!=(opaque ? 255u : 0u)) return false;
            if (opaque && (x<4 || x>=492) && c!=surround) return false;
        }
    }
    return foundPlate;
}
// A flat/vertical-gradient title backdrop can continue indefinitely horizontally.
// Inspect every native pixel: even a small course-selection image must reject it.
// Ignore the extra columns, which may contain stale tiles outside the hardware view.
inline bool horizontallyUniform(const std::vector<uint32_t> &pixels, int width) {
    if (width < 496 || pixels.size() % width != 0 || pixels.size()/width < 384) return false;
    const int margin = (width - 496) / 2;
    const int top = (int(pixels.size()/width) - 384) / 2;
    for (int y = 0; y < 384; ++y) {
        const auto *row = pixels.data() + size_t(y+top) * width + margin;
        for (int x = 1; x < 496; ++x)
            if ((row[x] & 0xffffff) != (row[0] & 0xffffff)) return false;
    }
    return true;
}
}
