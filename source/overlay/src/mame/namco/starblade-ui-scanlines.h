// license:BSD-3-Clause
#pragma once

// ST2 foreground artwork: text, reticle, warning text and cockpit instruments.
// The priority check excludes scene sprites that reuse these tile numbers.
inline bool starblade_ui_tile(u32 code, int priority)
{
    return priority == 3 && (code <= 0xaf || (code >= 0x100 && code <= 0x10f)
        || (code >= 0x200 && code <= 0x21c) || (code >= 0x340 && code <= 0x3c6));
}

// 0: scene, 1: HUD light/scanlines, 2: solid damage flash (ST2 tile).
inline u8 starblade_effect_tile(u32 code, int priority)
{
    return priority == 3 && code == 0x27a ? 2 : starblade_ui_tile(code, priority) ? 1 : 0;
}

// Scene sprites use the extended native coordinate field. Title star tiles
// are tagged separately so their 4:3 motion can also be projected into the
// widescreen margins. Fixed HUD and the damage flash stay on the original surface.
// 0: original surface, 1: wide surface, 2: title stars with wide projection,
// 3: scope artwork on the original surface plus the visible side margins.
inline u8 starblade_scene_tile(u32 code, int priority, int color)
{
    // The green targeting ring uses 0x100..0x10c. Its four-column sprite can
    // start outside the arcade clip, independently of the blue 0x35e..0x365
    // HUD lines. Restrict the exception to the ring's foreground palette.
    if (priority == 3 && color == 0 && code >= 0x100 && code <= 0x10c) return 3;
    if (priority == 3 && code >= 0x35e && code <= 0x365) return 3;
    if (starblade_effect_tile(code, priority) != 0) return 0;
    return priority == 2 && color == 0 && code >= 0x286 && code <= 0x28d ? 2 : 1;
}

inline rgb_t starblade_scanline_color(rgb_t color, unsigned strength)
{
    // Weak/medium/strong retain 75/55/35 percent of each colour channel.
    const unsigned gain = strength == 0 ? 100 : strength == 1 ? 75 : strength == 2 ? 55 : 35;
    return rgb_t(color.r() * gain / 100, color.g() * gain / 100, color.b() * gain / 100);
}
