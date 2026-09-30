// license:BSD-3-Clause
#pragma once
#include <array>
#include <cstdint>
namespace polygon_web_settings {
inline uint16_t crc(const std::array<uint16_t,64> &words) {
 uint16_t value=0;
 for(unsigned i=5;i<64;++i)for(int shift:{8,0}) {
  value^=uint16_t((words[i]>>shift)&255)<<8;
  for(int n=0;n<8;++n)value=uint16_t((value<<1)^((value&0x8000)?0x1021:0));
 }
 return uint16_t((value<<8)|(value>>8));
}
inline bool standalone(std::array<uint16_t,64> &words) {
 if(words[0]!=0x5345 || words[1]!=0x4741 || words[4]!=crc(words))return false;
 const auto role=words[5]&255;if(role>2)return false;
 words[5]&=0xff00;words[62]=uint16_t(words[62]-role*256);words[4]=crc(words);return true;
}
// VR GAME SYSTEM / MONITOR. Match the native configureVRMonitor43 transition:
// 16:9 WIDE -> 4:3 NORMAL, with additive checksum and CRC updated together.
inline bool vrMonitor43(std::array<uint16_t,64> &words) {
 if(words[0]!=0x5345 || words[1]!=0x4741 || words[4]!=crc(words))return false;
 const auto monitor=words[5]&255;if(monitor>1)return false;
 if(monitor==1){words[5]&=0xff00;words[62]=uint16_t(words[62]+0x100);words[4]=crc(words);}
 return true;
}

}
