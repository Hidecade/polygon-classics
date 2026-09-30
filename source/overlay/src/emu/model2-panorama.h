// license:BSD-3-Clause
// Daytona uses the same eight-strip tile format as Virtua Racing.
#pragma once
#include "model1-panorama.h"
namespace model2_gpu {
struct daytona_panorama {
    const uint8_t *rom;
    size_t bytes;
    model1_hd::vr_panorama course(int bank) const {
        constexpr size_t bases[]={0x742c0,0x7a520,0x811f8};
        constexpr int heights[]={49,54,43};
        return {rom,bytes,bases[bank],heights[bank]};
    }
    template<class ReadTile> int identify(int scroll,model1_hd::vr_panorama_tracker &tracker,ReadTile read) const {
        const int previous=tracker.bank;
        for(int attempt=0;attempt<3;++attempt) {
            const int bank=((previous<0 ? 0 : previous)+attempt)%3;
            const int phase=course(bank).phase(scroll,read);
            if(phase>=0) { tracker.bank=bank;return phase; }
        }
        return -1;
    }
};
}
