// license:BSD-3-Clause
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

// Solid spans: count tile coverage using range differences rather than storing
// an intermediate 64-bit reference for every covered tile. Preserve draw order.
template<class Rows, class Span>
unsigned starblade_pack_solid(const Rows &source, unsigned width, unsigned height,
    std::vector<uint32_t> &counts, std::vector<uint32_t> &offsets,
    std::vector<uint32_t> &bins, std::vector<Span> &spans)
{
    const unsigned columns=(width+31)/32, tiles=columns*height;
    counts.assign(tiles,0); offsets.resize(tiles); bins.resize(tiles*2);
    spans.clear(); spans.reserve(source.size()+2048);
    for(const auto &row:source) {
        spans.push_back(row.pixels);
        if(row.y>=height || row.pixels.x0>=row.pixels.x1 || row.pixels.x0>=width) continue;
        const unsigned left=row.pixels.x0/32;
        const unsigned right=(std::min(row.pixels.x1,width)-1)/32;
        ++counts[row.y*columns+left];
        if(right+1<columns) --counts[row.y*columns+right+1];
    }
    unsigned total=tiles*2;
    for(unsigned y=0;y<height;++y) {
        unsigned coverage=0;
        for(unsigned x=0;x<columns;++x) {
            const unsigned tile=y*columns+x;
            coverage+=counts[tile];
            offsets[tile]=bins[tile*2]=total; bins[tile*2+1]=coverage;
            total+=coverage;
        }
    }
    bins.resize((total+2047)/2048*2048);
    for(unsigned i=0;i<source.size();++i) {
        const auto &row=source[i];
        if(row.y>=height || row.pixels.x0>=row.pixels.x1 || row.pixels.x0>=width) continue;
        const unsigned left=row.pixels.x0/32;
        const unsigned right=(std::min(row.pixels.x1,width)-1)/32;
        for(unsigned x=left;x<=right;++x) bins[offsets[row.y*columns+x]++]=i;
    }
    return total-tiles*2;
}
