#pragma once
// VR drive-board commands, as documented by the Virtua Racing mapping in
// Boomslangnz/FFBArcadePlugin (Game Files/MAMESupermodel.cpp).
// Only discrete shake/roll commands map to phone haptics. Steering springs,
// clutch, initialization and unknown commands must not create fake impacts.
inline float vrHapticIntensity(unsigned command) {
    switch (command) {
        case 0x40: case 0x46: case 0x4a: return 0.7f;
        case 0x50: case 0x5f: case 0x60: case 0x6f: return 0.8f;
        default: return 0;
    }
}
