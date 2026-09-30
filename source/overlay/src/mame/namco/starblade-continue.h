// license:BSD-3-Clause
#pragma once

// The cabinet's Start line also skips the briefing. Once gameplay HUD has
// appeared, later Start edges consume the per-run continue allowance. Counting
// them even if the ROM leaves HUD art on a continue screen closes that loophole.
struct starblade_continue_state {
    unsigned used = 0;
    bool started = false;
    bool hud_seen = false;
    bool previous_pressed = false;
    bool blocked_press = false;
    bool title_active = false;

    u8 read(u8 raw, bool title, bool hud) {
        const bool pressed = (raw & 0xc0) != 0xc0;
        if (title && !title_active) {
            used = 0;
            started = false;
            hud_seen = false;
        }
        title_active = title;
        if (!title && hud) {
            started = true;
            hud_seen = true;
        }
        if (pressed && !previous_pressed) {
            blocked_press = false;
            if (!started) started = true;
            else if (hud_seen && !title) {
                if (used >= 5) blocked_press = true;
                else ++used;
            }
        }
        if (!pressed) blocked_press = false;
        previous_pressed = pressed;
        return blocked_press ? (raw | 0xc0) : raw;
    }
};
