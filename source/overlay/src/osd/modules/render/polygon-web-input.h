// Browser adapter for the native touch, motion and cabinet feedback mappings.
#pragma once
#include "VRFeedback.hpp"
#include "RallyFeedback.hpp"
#include "RidgeFeedback.hpp"
EM_JS(void, web_input_sample, (float *out), {
 const s=globalThis.polygonInput||{};
 const v=[s.active?1:0,s.x??.5,s.y??.5,s.fire?1:0,s.ground?1:0,s.brake?1:0];
 HEAPF32.set(v,out>>>2);
});
EM_JS(void, web_feedback, (float strength), { globalThis.parent.postMessage({type:'haptic',strength},location.origin); });
EM_JS(void, web_racing_phase, (int active), {globalThis.polygonRacing=!!active;});
inline void polygon_web_input(running_machine &m) {
 if(m.phase()!=machine_phase::RUNNING)return;
 static std::vector<ioport_field*> owned;
 float s[6];web_input_sample(s);
 for(auto *f:owned)f->clear_value();owned.clear();
 auto set=[&](const char *tag,unsigned mask,int value){auto *p=m.root_device().ioport(tag);auto *f=p?p->field(mask):nullptr;if(f){f->set_value(value);owned.push_back(f);}};
 const std::string name=m.system().name;
 const bool vr=name=="vr",rally=name=="srallyc",daytona=name=="daytona",ridge=name=="ridgerac"||name=="ridgera2"||name=="raverace";
 if(s[0]) {
  const float x=std::clamp(s[1],0.f,1.f),y=std::clamp(s[2],0.f,1.f);
  if(vr)set(":WHEEL",255,std::lround(x*255));
  else if(rally)set(":STEER",255,std::lround(x*255));
  else if(daytona)set(":STEER",255,0x20+std::lround(x*0xc0));
  else if(ridge)set(":ADC.0",0xfff,0x280+std::lround(x*0xb00));
  else {
   set(":AN1",255,1+std::lround(x*254));set(":AN2",255,name=="solvalou"?1+std::lround((1-y)*254):24+std::lround(y*208));
   set(":MCUH",0x20,s[3]!=0);if(name=="solvalou")set(":MCUH",8,s[4]!=0);
  }
 }
 static RidgeImpact impact;static int speed=-1;static unsigned frame=0;
 bool active=false;
 if(vr && model1_hd::published)active=model1_backdrop::vrRaceHUD(model1_hd::published->hud);
 else if(ridge && system22_gpu::published){
  if(++frame%6==0){speed=ridgeHUDSpeed(*system22_gpu::published);const float h=impact.sample(speed,m.time().as_double(),s[5]!=0);if(h>0)web_feedback(h);}
  active=speed>=0;
 }else if(rally||daytona){
  auto *cpu=m.root_device().subdevice<cpu_device>(":maincpu");if(cpu){auto &mem=cpu->space(AS_PROGRAM);
   active=daytona?mem.read_byte(0x5010a4)==0x16:((mem.read_byte(0x202098)<<16)|(mem.read_byte(0x20209c)<<8)|mem.read_byte(0x2020ac))==0x030503;
  }
 }
 web_racing_phase(active);
 static bool installed=false;static memory_passthrough_handler tap;static bool racing=false;racing=active;
 if(!installed && m.phase()==machine_phase::RUNNING && (vr||rally||daytona)){
  auto *cpu=m.root_device().subdevice<cpu_device>(daytona?":ioboard:iocpu":":maincpu");if(cpu){
   if(vr)tap=cpu->space(AS_PROGRAM).install_write_tap(0x502500,0x5025ff,"web_haptic",[](offs_t,u16 &data,u16 mask){float h=0;if(mask&255)h=vrHapticIntensity(data&255);if(mask&0xff00)h=std::max(h,vrHapticIntensity(data>>8));if(racing&&h>0)web_feedback(h);});
   else if(rally)tap=cpu->space(AS_PROGRAM).install_write_tap(0x01c00008,0x01c0000b,"web_haptic",[](offs_t,u32 &data,u32 mask){if(racing&&(mask&255)){const float h=rallyHapticIntensity(data&255);if(h>0)web_feedback(h);}});
   else tap=cpu->space(AS_PROGRAM).install_write_tap(0x8004,0x8004,"web_haptic",[](offs_t,u8 &data,u8 mask){if(racing&&mask){const float h=daytonaHapticIntensity(data);if(h>0)web_feedback(h);}});
   installed=true;
  }
 }
}
