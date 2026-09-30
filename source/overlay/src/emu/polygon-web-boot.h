// Browser boot acceleration; readiness detectors shared with the native core.
#pragma once
#include "emu.h"
#include "video.h"
#include "sound.h"
#include "model1-hd.h"
#include "model2-gpu.h"
#include "system22-gpu.h"
#include "polygon-web-boot-state.h"
#include <emscripten.h>
EM_JS(void, web_boot_notify, (int loading, int timeout, double elapsed), {
 globalThis.polygonBoot={loading:!!loading,timeout:!!timeout,elapsed};
 parent.postMessage({type:'board-loading',loading:!!loading,timeout:!!timeout,elapsed},location.origin);
});
struct polygon_web_boot {
 bool started=false,done=false,videoEnabled=false;
 bool racing=false,ridge=false,rally=false,daytona=false,solvalou=false;
 int videoPhases=0,savedFrameskip=0;bool savedThrottled=true,savedMute=false;
 double wallStarted=0,emulatedStarted=0,lastProbe=-1;
    bool ready(running_machine *machine) {
        // Racing games start submitting a 3D scene after their board tests.
        if (racing) return model1_hd::published &&
            (!model1_hd::published->below.empty() || !model1_hd::published->above.empty());
        if (ridge) return system22_gpu::published && !system22_gpu::published->vertices.empty();
        if (rally || daytona) {
            const auto &p = model2_gpu::published;
            if (!p || !p->polygons || p->tile_width < 496) return false;
            if (!p->polygons->vertices.empty()) return true;
            // The full-colour title precedes the first demo. POST text is
            // monochrome; inspect the native central area, excluding wide edges.
            unsigned colour = 0;
            const unsigned margin = (p->tile_width - 496) / 2;
            if (p->background.size() < p->tile_width * 384) return false;
            for (unsigned y = 0; y < 384; y += 4)
                for (unsigned x = margin; x < margin + 496; x += 4) {
                    const auto pixel = p->background[y * p->tile_width + x];
                    const int r = (pixel >> 16) & 255, g = (pixel >> 8) & 255, b = pixel & 255;
                    if (std::max({r,g,b}) > 96 && std::max({r,g,b}) - std::min({r,g,b}) > 64)
                        ++colour;
                }
            return colour > (496 * 384 / 16) / 50;
        }
        auto *cpu = machine->root_device().subdevice<cpu_device>(":maincpu");
        if (!cpu) return false;
        auto &space = cpu->space(AS_PROGRAM);
        if (solvalou) {
            const bool enabled = (space.read_word(0x760000) & 0x40) != 0;
            if (enabled && !videoEnabled) ++videoPhases;
            videoEnabled = enabled;
            return videoPhases >= 3;
        }
        // Same ROM sprite marker as the desktop native-loading.lua detector.
        for (unsigned index = 0; index < 256; ++index) {
            const auto entry = space.read_word(0x702000 + index * 2);
            const auto sprite = 0x700000 + (entry & 255) * 16;
            const auto formatBase = 0x704000 + (space.read_word(sprite) & 0x7ff) * 8;
            const auto offset = space.read_word(sprite + 2);
            const auto tileIndex = space.read_word(formatBase);
            const auto format = space.read_word(formatBase + 2);
            const unsigned columns = ((format >> 4) & 15) ? ((format >> 4) & 15) : 16;
            const unsigned rows = (format & 15) ? (format & 15) : 16;
            for (unsigned n = 0; n < columns * rows; ++n) {
                const auto tile = space.read_word(0x708000 + (tileIndex + n) * 2);
                const unsigned code = (tile + offset) & 0xffff;
                if (!(tile & 0x8000) && code >= 0x286 && code <= 0x28d) return true;
            }
            if (entry & 0x100) break;
        }
        return false;
    }

 void update(running_machine &m){
  if(done || m.phase()!=machine_phase::RUNNING || m.paused())return;
  if(!started){
   const std::string name=m.system().name;
   racing=name=="vr";ridge=name=="ridgerac"||name=="ridgera2"||name=="raverace";
   rally=name=="srallyc";daytona=name=="daytona";solvalou=name=="solvalou";
   savedFrameskip=m.video().frameskip();savedThrottled=m.video().throttled();savedMute=m.sound().system_mute();
   m.video().set_frameskip(10);m.video().set_throttled(false);m.sound().system_mute(true);
   emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 0);
   wallStarted=emscripten_get_now();emulatedStarted=m.time().as_double();started=true;web_boot_notify(1,0,0);
  }
  const double elapsed=m.time().as_double()-emulatedStarted;
  const bool timedout=elapsed>=35 || emscripten_get_now()-wallStarted>=45000;
  // Sample the Solvalou enable edges every frame; other markers need only 10 Hz.
  bool complete=false;
  if(solvalou || elapsed-lastProbe>=0.1){lastProbe=elapsed;complete=ready(&m);}
  if(complete || timedout){
   done=true;polygon_web_boot_active=false;
   emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
   m.video().set_frameskip(savedFrameskip);m.video().set_throttled(savedThrottled);m.sound().system_mute(savedMute);
   web_boot_notify(0,timedout&&!complete,elapsed);
  }
 }
};
inline void polygon_web_boot_update(running_machine &machine){static polygon_web_boot boot;boot.update(machine);}
