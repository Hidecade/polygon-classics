// license:BSD-3-Clause
#pragma once
#include <cstdint>

// Active-low cabinet trigger inputs. Sample edges once per emulated frame so
// multiple MCU polls see the same one-frame pulse, without changing bindings.
struct starblade_fire_state
{
    std::uint8_t previous = 0, pulse = 0, repeat = 0;
    std::uint8_t previous_mode = 0xff, previous_rate = 0xff;
    std::uint64_t frame = ~std::uint64_t(0);
    std::uint64_t next_repeat = 0;

    // mode: 0 single shot, 1 hold to fire, 2 automatic.
    // rate: 0 = 30 shots/sec, 2 = 20 shots/sec. Old value 1 uses 30/sec.
    std::uint8_t read(std::uint8_t raw, std::uint64_t current_frame, unsigned mode,
        unsigned rate = 0)
    {
        constexpr std::uint8_t triggers = 0x2b;
        if (rate != 2) rate = 0;
        const std::uint8_t pressed = ~raw & triggers;
        const std::uint8_t active = mode == 2 ? std::uint8_t(pressed | 0x20) : pressed;
        if (frame != current_frame)
        {
            pulse = pressed & ~previous;
            previous = pressed;
            frame = current_frame;
            repeat = pulse;
            if (mode != previous_mode || rate != previous_rate)
                next_repeat = current_frame;
            previous_mode = mode;
            previous_rate = rate;
            if (mode != 0)
            {
                const std::uint64_t interval = rate == 2 ? 3 : 2;
                if (pulse)
                {
                    if (mode == 2) repeat = active;
                    next_repeat = current_frame + interval;
                }
                else if (active && current_frame >= next_repeat)
                {
                    repeat = active;
                    next_repeat = current_frame + interval;
                }
                else if (!active)
                    next_repeat = current_frame;
            }
        }
        if (mode == 0) return (raw | triggers) & ~(pulse & pressed);
        return (raw | triggers) & ~(repeat & active);
    }
};
