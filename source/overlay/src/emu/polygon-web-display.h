#pragma once
#include <cstdlib>
#include <cmath>
#include <emscripten.h>
#include "model1-hd.h"
#include "model2-gpu.h"
#include "system22-gpu.h"
inline void polygon_web_display_init() {
 const int requested=EM_ASM_INT({return globalThis.polygonRenderWidth || 1920;});
 const int width=requested==1440 || requested==2580 ? requested : 1920;
 if(width==1440) setenv("STARBLADE_FOUR_THREE","1",1);
 if(width==2580) setenv("STARBLADE_ULTRAWIDE","1",1);
 model1_hd::width=width;
 model1_hd::offset_x=(width-1440)/2.f;
 model1_hd::extra=int(std::ceil(model1_hd::offset_x/model1_hd::scale_x));
 model1_hd::background_width=496+model1_hd::extra*2;model2_gpu::width=width;system22_gpu::width=width;
}
