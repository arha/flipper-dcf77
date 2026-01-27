#include "bsf_util.h"

#include <stdbool.h>
#include <string.h>

#define BSF_FRAME_SECONDS 60U

static RadioClockPulse bsf_bits_to_pulse(bool high_bit, bool low_bit) {
    if(high_bit) {
        return low_bit ? RadioClockPulsePair11 : RadioClockPulsePair10;
    }

    return low_bit ? RadioClockPulsePair01 : RadioClockPulsePair00;
}

static uint8_t bsf_pulse_to_digit(RadioClockPulse pulse) {
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

static void bsf_set_symbol(RadioClockPulse* frame, uint8_t second, RadioClockPulse pulse) {
    frame[second] = pulse;
}

static void bsf_set_weighted_pair(
    RadioClockPulse* frame,
    uint8_t second,
    uint16_t value,
    uint16_t high_weight,
    uint16_t low_weight) {
    bsf_set_symbol(frame, second, bsf_bits_to_pulse((value & high_weight) != 0U, (value & low_weight) != 0U));
}

static bool bsf_even_parity_bits(const RadioClockPulse* frame, uint8_t start, uint8_t end_inclusive) {
    bool parity = false;

    for(uint8_t second = start; second <= end_inclusive; second++) {
        const uint8_t digit = bsf_pulse_to_digit(frame[second]);
        parity ^= (digit & 0x02U) != 0U;
        parity ^= (digit & 0x01U) != 0U;
    }

    return parity;
}

void set_bsf_timecode(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year,
    uint8_t weekday,
    bool leap_second_warning,
    bool dst_active) {
    memset(dest, RadioClockPulsePair00, sizeof(RadioClockPulse) * BSF_FRAME_SECONDS);

    dest[0] = RadioClockPulseMarker;
    dest[39] = RadioClockPulseMarker;
    dest[59] = RadioClockPulseMarker;

    /* Public-information slots stay zeroed until they are specified. */
    dest[40] = RadioClockPulsePair01;
    bsf_set_weighted_pair(dest, 41U, minute, 32U, 16U);
    bsf_set_weighted_pair(dest, 42U, minute, 8U, 4U);
    bsf_set_weighted_pair(dest, 43U, minute, 2U, 1U);
    bsf_set_weighted_pair(dest, 44U, hour, 16U, 8U);
    bsf_set_weighted_pair(dest, 45U, hour, 4U, 2U);
    bsf_set_symbol(dest, 46U, bsf_bits_to_pulse((hour & 1U) != 0U, bsf_even_parity_bits(dest, 41U, 45U)));
    bsf_set_weighted_pair(dest, 47U, day, 16U, 8U);
    bsf_set_weighted_pair(dest, 48U, day, 4U, 2U);
    bsf_set_symbol(dest, 49U, bsf_bits_to_pulse((day & 1U) != 0U, (weekday & 4U) != 0U));
    bsf_set_weighted_pair(dest, 50U, weekday, 2U, 1U);
    bsf_set_weighted_pair(dest, 51U, month, 8U, 4U);
    bsf_set_weighted_pair(dest, 52U, month, 2U, 1U);
    bsf_set_weighted_pair(dest, 53U, year, 64U, 32U);
    bsf_set_weighted_pair(dest, 54U, year, 16U, 8U);
    bsf_set_weighted_pair(dest, 55U, year, 4U, 2U);
    bsf_set_symbol(dest, 56U, bsf_bits_to_pulse((year & 1U) != 0U, bsf_even_parity_bits(dest, 47U, 55U)));
    bsf_set_symbol(dest, 57U, bsf_bits_to_pulse(false, leap_second_warning));
    bsf_set_symbol(dest, 58U, bsf_bits_to_pulse(false, dst_active));
}
