#include "bpc_util.h"

#include <stdbool.h>
#include <string.h>

#define BPC_FRAME_SECONDS 60U
#define BPC_BLOCK_SECONDS 20U

static RadioClockPulse bpc_digit_to_pulse(uint8_t value) {
    switch(value & 0x03U) {
    case 1U:
        return RadioClockPulsePair01;
    case 2U:
        return RadioClockPulsePair10;
    case 3U:
        return RadioClockPulsePair11;
    case 0U:
    default:
        return RadioClockPulsePair00;
    }
}

static uint8_t bpc_pulse_to_digit(RadioClockPulse pulse) {
    switch(pulse) {
    case RadioClockPulsePair01:
        return 1U;
    case RadioClockPulsePair10:
        return 2U;
    case RadioClockPulsePair11:
        return 3U;
    case RadioClockPulsePair00:
    default:
        return 0U;
    }
}

static void bpc_fill_frame(RadioClockPulse* frame, RadioClockPulse pulse) {
    for(size_t i = 0; i < BPC_FRAME_SECONDS; i++) {
        frame[i] = pulse;
    }
}

static void bpc_set_symbol(RadioClockPulse* frame, uint8_t second, RadioClockPulse pulse) {
    frame[second] = pulse;
}

static void bpc_set_pair_bits(
    RadioClockPulse* frame,
    uint8_t second,
    bool high_bit,
    bool low_bit) {
    bpc_set_symbol(
        frame, second, bpc_digit_to_pulse((uint8_t)((high_bit ? 0x02U : 0U) | (low_bit ? 0x01U : 0U))));
}

static void bpc_set_weighted_pair(
    RadioClockPulse* frame,
    uint8_t second,
    uint16_t value,
    uint16_t high_weight,
    uint16_t low_weight) {
    bpc_set_pair_bits(frame, second, (value & high_weight) != 0U, (value & low_weight) != 0U);
}

static bool bpc_even_parity_bits(const RadioClockPulse* frame, uint8_t start, uint8_t end_inclusive) {
    bool parity = false;

    for(uint8_t second = start; second <= end_inclusive; second++) {
        const uint8_t digit = bpc_pulse_to_digit(frame[second]);
        parity ^= (digit & 0x02U) != 0U;
        parity ^= (digit & 0x01U) != 0U;
    }

    return parity;
}

static void bpc_set_block(
    RadioClockPulse* frame,
    uint8_t start_second,
    uint8_t block_index,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year_since_2000,
    uint8_t weekday) {
    bool p1;
    bool p2;

    bpc_set_symbol(frame, start_second + 0U, RadioClockPulseMarker);
    bpc_set_symbol(frame, start_second + 1U, bpc_digit_to_pulse(block_index));
    bpc_set_symbol(frame, start_second + 2U, RadioClockPulsePair00);
    bpc_set_weighted_pair(frame, start_second + 3U, (uint8_t)(hour % 12U), 8U, 4U);
    bpc_set_weighted_pair(frame, start_second + 4U, (uint8_t)(hour % 12U), 2U, 1U);
    bpc_set_weighted_pair(frame, start_second + 5U, minute, 32U, 16U);
    bpc_set_weighted_pair(frame, start_second + 6U, minute, 8U, 4U);
    bpc_set_weighted_pair(frame, start_second + 7U, minute, 2U, 1U);
    bpc_set_weighted_pair(frame, start_second + 8U, weekday, 0U, 4U);
    bpc_set_weighted_pair(frame, start_second + 9U, weekday, 2U, 1U);

    p1 = bpc_even_parity_bits(frame, start_second + 1U, start_second + 9U);
    bpc_set_pair_bits(frame, start_second + 10U, hour >= 12U, p1);

    bpc_set_symbol(frame, start_second + 11U, RadioClockPulsePair00);
    bpc_set_weighted_pair(frame, start_second + 12U, day, 8U, 4U);
    bpc_set_weighted_pair(frame, start_second + 13U, day, 2U, 1U);
    bpc_set_weighted_pair(frame, start_second + 14U, month, 8U, 4U);
    bpc_set_weighted_pair(frame, start_second + 15U, month, 2U, 1U);
    bpc_set_weighted_pair(frame, start_second + 16U, year_since_2000, 32U, 16U);
    bpc_set_weighted_pair(frame, start_second + 17U, year_since_2000, 8U, 4U);
    bpc_set_weighted_pair(frame, start_second + 18U, year_since_2000, 2U, 1U);

    p2 = bpc_even_parity_bits(frame, start_second + 10U, start_second + 18U);
    bpc_set_pair_bits(frame, start_second + 19U, (year_since_2000 & 64U) != 0U, p2);
}

void set_bpc_timecode(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year_since_2000,
    uint8_t weekday) {
    bpc_fill_frame(dest, RadioClockPulsePair00);

    bpc_set_block(dest, 0U, 0U, minute, hour, day, month, year_since_2000, weekday);
    bpc_set_block(dest, 20U, 1U, minute, hour, day, month, year_since_2000, weekday);
    bpc_set_block(dest, 40U, 2U, minute, hour, day, month, year_since_2000, weekday);
}
