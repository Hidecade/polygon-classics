#pragma once
// SEGA Rally drive command force bands, independently mapped to phone intensity.
// Reference: Boomslangnz/FFBArcadePlugin, Game Files/MAMESupermodel.cpp,
// SrallyActive. Commands outside these bands include stop/init/control signals.
// This maps cabinet steering force to haptics, not a physical motor simulation.
inline float rallyHapticIntensity(unsigned command) {
    if (command >= 0x80 && command < 0x9f) return (command - 0x7f) / 31.f;
    if (command >= 0xc0 && command < 0xdf) return (command - 0xbf) / 31.f;
    return 0;
}

// Daytona discrete uncentering/roll commands (FFBArcadePlugin M2PatternActive).
// Spring/clutch and initialization commands do not generate phone impacts.
inline float daytonaHapticIntensity(unsigned command) {
    if (command>=0x40 && command<=0x47) return (command-0x3f)/8.f;
    if (command>=0x50 && command<=0x57) return (command-0x4f)/8.f;
    if (command>=0x60 && command<=0x67) return (command-0x5f)/8.f;
    return 0;
}
