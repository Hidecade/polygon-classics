// Opt-in host-time sampling; all mutable counters belong to the emulation thread.
#pragma once
#include <array>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdint>
namespace system22_profile {
inline bool enabled=false;
inline std::array<uint64_t,9> nanoseconds{},calls{};
inline std::array<std::array<uint32_t,65536>,2> dsp_pcs{};
inline uint64_t frames=0;
inline void reset(bool active) { enabled=active;nanoseconds={};calls={};dsp_pcs={};frames=0; }
struct scope {
    int id;
    std::chrono::steady_clock::time_point start;
    scope(int unit,uint32_t pc=0):id(enabled?unit:-1) {
        if(id<0)return;
        start=std::chrono::steady_clock::now();
        if (++calls[id]%64==0 && (id==1||id==2)) ++dsp_pcs[id-1][pc&65535];
    }
    ~scope(){if(id>=0)nanoseconds[id]+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();}
};
inline std::string report() {
    static const char *names[]={"68020","master DSP","slave DSP","C74 MCU","video","packet","geometry","scene","text"};
    std::ostringstream out;out<<std::fixed<<std::setprecision(3)<<"frames="<<frames<<"\n";
    for(int i=0;i<9;i++)out<<names[i]<<": "<<double(nanoseconds[i])/1e6/std::max(uint64_t(1),frames)<<" ms/frame, calls="<<calls[i]<<"\n";
    for(int d=0;d<2;d++){
        std::vector<std::pair<uint32_t,unsigned>> ranked;
        for(unsigned pc=0;pc<65536;pc++)if(dsp_pcs[d][pc])ranked.emplace_back(dsp_pcs[d][pc],pc);
        std::sort(ranked.rbegin(),ranked.rend());out<<names[d+1]<<" entry PC samples:";
        for(unsigned i=0;i<std::min(size_t(12),ranked.size());i++)out<<" "<<std::hex<<ranked[i].second<<std::dec<<"="<<ranked[i].first;
        out<<"\n";
    }
    return out.str();
}
}
