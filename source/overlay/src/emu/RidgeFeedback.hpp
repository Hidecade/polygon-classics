#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include "system22-gpu.h"

// Read the native HUD, before scaling/rotation/fade. No ROM/RAM patch or OCR
// of the presented screen: locate the fixed km/h stencil, then read its
// neighbouring seven-segment digits.  The three System 22 racers move this HUD.
template<class Pixel> int ridgeHUDSpeed(Pixel pixel) {
    constexpr uint64_t label[16]={0,0x40000002,0x40000002,0x48000002,0x48000882,
        0x7440778c2,0xdc40dd862,0x18c2088832,0x104208881a,0x104108881e,
        0x1041088836,0x1040888862,0x10408888c2,0x1040488982,0x1040488902,0};
    constexpr int xs[7]={7,11,10,6,1,2,6}, ys[7]={450,453,459,462,459,453,456};
    constexpr unsigned digitMasks[10]={0x3f,6,0x5b,0x4f,0x66,0x6d,0x7d,7,0x7f,0x6f};
    auto white=[](uint32_t c) { return (c>>24) && (c&0xffffff)==0xf8f8f8; };
    for(int top=0;top<=464;++top) for(int left=0;left<=592;++left) {
        if(!white(pixel(left+1,top+2)) || !white(pixel(left+30,top+2))) continue;
        bool match=true;
        for(int y=0;y<16 && match;++y) {
            uint64_t bits=0;
            for(int x=0;x<48;++x) if(white(pixel(left+x,top+y))) bits|=uint64_t(1)<<x;
            match=bits==label[y];
        }
        if(!match) continue;
        int speed=0;
        for(int offset : {-48,-32,-16}) {
            unsigned mask=0;
            for(int i=0;i<7;++i)
                if(pixel(left+offset+xs[i],top+2+(ys[i]-450))==0xffffff00) mask|=1<<i;
            int digit=-1;
            if (!mask && offset!=-16) digit=0;
            else for(int i=0;i<10;++i) if(mask==digitMasks[i]) digit=i;
            if(digit<0) return -1;
            speed=speed*10+digit;
        }
        return speed<=400 ? speed : -1;
    }
    return -1;
}
inline int ridgeHUDSpeed(const system22_gpu::frame &frame) {
    if(frame.hud.size()!=640*480) return -1;
    return ridgeHUDSpeed([&](int x,int y) {
        auto h=frame.hud[y*640+x];
        return h&0x10000 ? 0xff000000u|(frame.palette[h&0x7fff]&0xffffff) : 0u;
    });
}

// Supplemental phone impact feedback, not emulation of a cabinet motor.
// A sharp loss of speed while not braking indicates contact; ordinary coasting,
// gear changes, standing starts, demos and invalid/incomplete HUDs stay silent.
struct RidgeImpact {
    struct Sample { int speed=-1;double time=-1; };
    std::array<Sample,16> history{};
    unsigned next=0;
    double lastImpact=-1;
    float sample(int speed,double now,bool braking) {
        if(speed<0 || braking) { history={};next=0;return 0; }
        int prior=speed;
        for(auto s:history) if(s.time>=0 && now>=s.time && now-s.time<=0.12) prior=std::max(prior,s.speed);
        history[next++%history.size()]={speed,now};
        if(prior<45 || prior-speed<15 || now-lastImpact<0.25) return 0;
        lastImpact=now;
        return std::min(0.9f,0.5f+(prior-speed)/80.f);
    }
};
