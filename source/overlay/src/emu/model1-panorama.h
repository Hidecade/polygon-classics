// Virtua Racing's eight 256-pixel sky strips, read from the user's loaded ROM.
#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
namespace model1_hd {
// Preserve the hardware scroll wrap inside the native viewport. Outside it,
// extend the nearest edge within its page instead of wrapping another sky in.
inline int vr_background_row(int y, int scroll) {
    const int edge=y<0 ? 0 : y>383 ? 383 : y;
    const int source=((edge+scroll)&1023)+(y-edge);
    return source<0 ? 0 : source>1023 ? 1023 : source;
}

// Presentation-only state. Tile RAM is updated in strips; an incomplete strip
// must not switch the side panels back to the temporary 512-pixel window.
struct vr_panorama_tracker {
    int position=-1, last_scroll=0, bank=-1;
    int update(int observed,int scroll,bool active) {
        if(!active) { position=-1;last_scroll=0;bank=-1;return -1; }
        if(observed>=0) position=observed&2047;
        else if(position>=0) {
            // Recover the signed camera movement across the 512-pixel ring wrap.
            const int delta=((last_scroll-scroll+256)&511)-256;
            position=(position+delta)&2047;
        }
        last_scroll=scroll;
        return position;
    }
};
struct vr_panorama {
    const uint8_t *rom;
    size_t bytes;
    static constexpr size_t first=0x1315ed4, stride=0x50c;
    static constexpr int columns=256, rows=20, top=48;
    size_t base=first;
    int height=rows;
    // All courses occur in attract mode too. Medium includes the sea/islands;
    // expert has a taller mountain backdrop. Each uses eight 32-tile strips.
    vr_panorama course(int bank) const {
        return {rom,bytes,bank==1 ? size_t(0x13c0094) : bank==2 ? size_t(0x121ddb2) : first,
                bank==1 ? 48 : bank==2 ? 23 : rows};
    }
    size_t strip_stride() const { return 12+height*64; }
    uint16_t word(size_t offset) const { return rom[offset]|(uint16_t(rom[offset+1])<<8); }
    bool valid() const {
        // Model 2 uses the same strip format with up to 54 rows.
        if (!rom || height<20 || height>64 || base>bytes || 8*strip_stride()>bytes-base) return false;
        for(int i=0;i<8;++i) {
            size_t p=base+i*strip_stride();
            if(word(p)!=0 || word(p+2)!=1 || word(p+4)!=height || word(p+6)!=0 || word(p+8)!=32 || word(p+10)!=0) return false;
        }
        return true;
    }
    uint16_t tile(int x,int y) const {
        x&=255;
        return word(base+(x/32)*strip_stride()+12+(y*32+(x&31))*2);
    }
    // The hardware tilemap is a 512-pixel ring buffer containing only two
    // of eight strips. Match the visible window, not the ring's unused seam.
    template<class ReadTile> int phase(int hscroll,ReadTile read) const {
        if (!valid() || (hscroll&0x8000)) return -1;
        const int start=(-hscroll)&511, count=(496+(start&7)+7)/8;
        for(int candidate=0;candidate<columns;++candidate) {
            bool match=true;
            for(int row : {8,12,16,20}) {
                for(int i=0;i<count;++i) {
                    if(read(row,((start/8)+i)&63)!=tile(candidate+i,row-6)) { match=false;break; }
                }
                if(!match) break;
            }
            if(match) return candidate*8+(start&7);
        }
        return -1;
    }
    template<class ReadTile> int identify(int hscroll,vr_panorama_tracker &tracker,ReadTile read) const {
        // Prioritize the current course through partial ring-buffer updates.
        const int previous=tracker.bank;
        for(int attempt=0;attempt<3;++attempt) {
            const int bank=((previous<0 ? 0 : previous)+attempt)%3;
            const int observed=course(bank).phase(hscroll,read);
            if(observed>=0) { tracker.bank=bank;return observed; }
        }
        // A few stale tiles during DMA must not force scroll-only prediction
        // across an attract-mode camera cut. Recover an absolute ROM position
        // only when almost the entire visible window agrees and the runner-up
        // is clearly worse, including candidates from the other two courses.
        if (hscroll&0x8000) return -1;
        const int start=(-hscroll)&511, count=(496+(start&7)+7)/8;
        int best=0, runner=0, bestPhase=-1, bestBank=-1;
        for (int bank=0;bank<3;++bank) {
            const auto map=course(bank);
            if (!map.valid()) continue;
            for (int candidate=0;candidate<columns;++candidate) {
                int score=0;
                for (int row : {8,12,16,20})
                    for (int i=0;i<count;++i)
                        score += read(row,((start/8)+i)&63)==map.tile(candidate+i,row-6);
                if (score>best) { runner=best;best=score;bestPhase=candidate*8+(start&7);bestBank=bank; }
                else if (score>runner) runner=score;
            }
        }
        if (best>=count*4*7/8 && best-runner>=count/2) {
            tracker.bank=bestBank;
            return bestPhase;
        }
        return -1;
    }
};
}
