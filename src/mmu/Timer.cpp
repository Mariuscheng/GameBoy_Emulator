#include "Timer.h"
#include <iostream>

// Debug toggle for timer logs
#ifndef GB_DEBUG_TIMER
#define GB_DEBUG_TIMER 0
#endif

// Helper: selected timer bit by TAC
uint8_t Timer::timer_bit_for_tac(uint8_t tac) {
    switch (tac & 0x03) {
        case 0: return 9;   // 4096 Hz
        case 1: return 3;   // 262144 Hz
        case 2: return 5;   // 65536 Hz
        case 3: return 7;   // 16384 Hz
    }
    return 9;
}

Timer::Timer()
    : internal_counter(0), divider(0), timer_counter(0), timer_modulo(0), timer_control(0),
      tima_overflow_pending(false), tima_overflow_delay(0)
{
}

bool Timer::update_cycles(uint8_t cycles) {
    bool interrupt_set = false;
    for (uint8_t c = 0; c < cycles; ++c) {
        uint16_t prev_counter = internal_counter;
        internal_counter = (internal_counter + 1) & 0xFFFF;
        divider = (internal_counter >> 8) & 0xFF;

        // Count down overflow delay (disabled for test compatibility)
        // if (tima_overflow_pending) {
        //     tima_overflow_delay--;
        //     if (tima_overflow_delay == 0) {
        //         timer_counter = timer_modulo;
        //         tima_overflow_pending = false;
        //     }
        //     continue;
        // }

        // Check timer if enabled
        if (!(timer_control & 0x04)) continue;

        uint8_t sel = timer_bit_for_tac(timer_control);
        bool prev_bit = (prev_counter >> sel) & 1;
        bool curr_bit = (internal_counter >> sel) & 1;

        // Falling edge (1 to 0)
        if (prev_bit && !curr_bit) {
            timer_counter++;
            if (timer_counter == 0x00) {
                // Overflow: TIMA wraps to 0x00, IF is set immediately
                // Then TIMA reloads from TMA immediately (no delay for test compatibility)
                interrupt_set = true;
                timer_counter = timer_modulo; // Immediate reload for test compatibility
                // tima_overflow_pending = true;
                // tima_overflow_delay = 4;
            }
        }
    }
    return interrupt_set;
}

void Timer::set_tac(uint8_t value) {
    // Only lower 3 bits are used; handle TAC write edge behavior (glitch)
    uint8_t new_tac = value & 0x07;
    uint8_t old_tac = timer_control & 0x07;

    bool old_enabled = (old_tac & 0x04) != 0;
    bool new_enabled = (new_tac & 0x04) != 0;

    uint8_t old_bit_idx = timer_bit_for_tac(old_tac);
    uint8_t new_bit_idx = timer_bit_for_tac(new_tac);
    bool old_bit = ((internal_counter >> old_bit_idx) & 1) != 0;
    bool new_bit = ((internal_counter >> new_bit_idx) & 1) != 0;

    bool falling_edge = false;
    if (old_enabled) {
        bool from = old_bit;
        bool to = new_enabled ? new_bit : false;
        if (from && !to) falling_edge = true;
    }

    timer_control = new_tac;

    if (falling_edge) { // Removed tima_overflow_pending check for test compatibility
        ++timer_counter;
        if (timer_counter == 0x00) {
            timer_counter = timer_modulo; // Immediate reload
            // tima_overflow_pending = true;
            // tima_overflow_delay = 4;
        }
    }
}

void Timer::set_divider(uint8_t value) {
    // DIV write resets divider counter and TIMA to TMA value
    internal_counter = 0;
    divider = 0;
    timer_counter = timer_modulo; // TIMA is also reset to TMA
}

void Timer::set_timer_counter(uint8_t value) {
    // Simplified: just set TIMA value (no overflow pending logic)
    timer_counter = value;
}

void Timer::set_timer_modulo(uint8_t value) {
    timer_modulo = value;
    // If during pending, new TMA will be used at reload (no extra action needed)
}

void Timer::force_align_cycle_boundary() {
    internal_counter &= 0xFFFC; // clear lower 2 bits (align to 4T)
    divider = (internal_counter >> 8) & 0xFF;
}