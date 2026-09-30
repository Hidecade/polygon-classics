// license:BSD-3-Clause
#pragma once
#ifdef SDLMAME_EMSCRIPTEN
#include <emscripten.h>
#include "emu.h"
#include "model1-hd.h"
#include "model2-gpu.h"
#include "system22-gpu.h"
#include "model1-backdrop.h"
#include "machine/eepromser.h"
#include "polygon-web-settings.h"
#include "polygon-web-input.h"
EM_JS(int, polygon_web_kind, (), { return globalThis.polygonWebGPU?.ready ? globalThis.polygonWebGPU.kind : 0; });
EM_JS(void, polygon_web_hide, (), { globalThis.polygonWebGPU?.hide(); });
EM_JS(int, polygon_web_present, (const uint32_t *parts, unsigned count, const uint32_t *settings), {
    const p=Array.from(HEAPU32.subarray(parts>>>2,(parts>>>2)+count));
    const c=Array.from(HEAPU32.subarray(settings>>>2,(settings>>>2)+8));
    return globalThis.polygonWebGPU?.present(HEAPU8,p,c) ? 1 : 0;
});
inline bool polygon_web_draw(render_primitive_list &list, running_machine &machine) {
    polygon_web_input(machine);
    // The cabinet uses 4:3 projection in every browser display mode. FHD and
    // Ultra Wide add side geometry; they must not also select WIDE in the ROM.
    static bool vr_monitor_ready=false;
    if(!vr_monitor_ready && machine.phase()==machine_phase::RUNNING && std::string(machine.system().name)=="vr") {
        auto *eeprom=machine.root_device().subdevice<eeprom_serial_93cxx_device>(":ioboard:eeprom");
        if(eeprom) {
            std::array<uint16_t,64> words;
            for(unsigned i=0;i<64;++i)words[i]=eeprom->internal_read(i);
            const auto before=words;
            if(polygon_web_settings::vrMonitor43(words)) {
                vr_monitor_ready=true;
                for(unsigned i:{4u,5u,62u})eeprom->internal_write(i,words[i]);
                osd_printf_info("VR cabinet MONITOR: 4:3 NORMAL\n");
                if(words!=before)machine.schedule_soft_reset();
            }
        }
    }
    // The original DAYTONA factory EEPROM selects MASTER and waits for peers.
    // This browser release is single-player; retain all other cabinet settings.
    static bool cabinet_ready=false;
    if(!cabinet_ready && std::string(machine.system().name)=="daytona") {
        auto *eeprom=machine.root_device().subdevice<eeprom_serial_93cxx_device>(":ioboard:eeprom");
        if(eeprom) {
            std::array<uint16_t,64> words;
            for(unsigned i=0;i<64;++i)words[i]=eeprom->internal_read(i);
            const auto before=words;
            if(polygon_web_settings::standalone(words)) {
                cabinet_ready=true;
                if(words!=before) {
                    for(unsigned i:{4u,5u,62u})eeprom->internal_write(i,words[i]);
                    machine.schedule_soft_reset();
                }
            }
        }
    }
    const int kind=polygon_web_kind();
    model1_hd::enabled=kind==1;
    system22_gpu::enabled=system22_gpu::available=kind==2;
    model2_gpu::enabled=model2_gpu::available=kind==3;
    if(!kind) {polygon_web_hide();return false;}
    bool screen=false,simple=true;
    for(const auto &p:list) {
        if(PRIMFLAG_GET_SCREENTEX(p.flags) && !screen) {screen=true;}
        else if(screen || p.texture.base || p.color.r!=0 || p.color.g!=0 || p.color.b!=0) simple=false;
    }
    if(!simple || !screen) {
        model1_hd::enabled=false;system22_gpu::available=false;model2_gpu::available=false;
        polygon_web_hide();return false;
    }
    std::vector<uint32_t> parts;
    const auto add=[&](const auto &v){parts.push_back(uintptr_t(v.data()));parts.push_back(v.size()*sizeof(v[0]));};
    uint32_t config[8]={1920,0,0,0,0,0,0,0};
    if(kind==1 && model1_hd::published) {
        const auto &f=*model1_hd::published;
        const bool plate=model1_backdrop::titlePlate(f.hud);
        const bool race=(f.tile_registers[6]&0xe000)==0x2000;
        const bool title=!race && model1_backdrop::horizontallyUniform(f.background,f.tile_width);
        config[1]=f.tile_width;config[2]=title?3:((race||title||plate)?0:2);config[3]=plate?4:1;
        config[4]=race||title||plate;
        add(f.below);add(f.above);add(f.background);add(f.hud);
    } else if(kind==2 && system22_gpu::published && system22_gpu::published->textures) {
        const auto &f=*system22_gpu::published;const auto &a=*f.textures;
        add(f.vertices);add(f.materials);add(f.palette);add(a.map);add(a.attributes);add(a.texels);add(a.lookup);
        add(f.hud);add(a.gamma);
        parts.push_back(uintptr_t(&f.mix));parts.push_back(sizeof(f.mix));config[6]=f.mix.background;
    } else if(kind==3 && model2_gpu::published && model2_gpu::published->polygons) {
        const auto &f=*model2_gpu::published;const auto &p=*f.polygons;
        config[1]=f.tile_width;config[2]=(f.tile_width-496)/2;config[5]=p.materials.size();
        add(p.vertices);add(p.materials);add(f.textures);add(f.palette);add(f.colorxlat);add(f.luma);add(f.gamma);add(f.background);add(f.hud);
    } else {polygon_web_hide();return false;}
    return polygon_web_present(parts.data(),parts.size(),config)!=0;
}
#endif
