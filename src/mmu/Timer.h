#pragma once

#include <cstdint>

class Timer {
public:
    Timer();
    ~Timer() = default;

    // Update timer with cycles, return true if timer interrupt should be set
    bool update_cycles(uint8_t cycles);

    // Set TAC register
    void set_tac(uint8_t value);

    // Read registers
    uint8_t get_divider() const { return divider; }
    uint8_t get_timer_counter() const { return timer_counter; }
    uint8_t get_timer_modulo() const { return timer_modulo; }
    uint8_t get_timer_control() const { return timer_control; }
    uint8_t get_cycle_mod4() const { return (internal_counter & 0x3); }

    // Write registers
    void set_divider(uint8_t value);
    void set_timer_counter(uint8_t value);
    void set_timer_modulo(uint8_t value);

    // Special functions
    void force_align_cycle_boundary();

private:
    uint16_t internal_counter;
    uint8_t divider;
    uint8_t timer_counter;
    uint8_t timer_modulo;
    uint8_t timer_control;
    bool tima_overflow_pending;
    uint8_t tima_overflow_delay;

    static uint8_t timer_bit_for_tac(uint8_t tac);
};