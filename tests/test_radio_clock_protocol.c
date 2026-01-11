#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/dcf77_util.h"
#include "../src/radio_clock_protocol.h"

static char pulse_to_char(RadioClockPulse pulse) {
    switch(pulse) {
    case RadioClockPulseOne:
        return '1';
    case RadioClockPulseMarker:
        return 'M';
    case RadioClockPulseZero:
    default:
        return '0';
    }
}

static void frame_to_string(const RadioClockPulse* frame, char* out) {
    for(size_t i = 0; i < RADIO_CLOCK_FRAME_SECONDS; i++) {
        out[i] = pulse_to_char(frame[i]);
    }
    out[RADIO_CLOCK_FRAME_SECONDS] = '\0';
}

static void test_dcf77_protocol_matches_known_frame(void) {
    RadioClockProtocolTime time = {
        .year = 2026,
        .day_of_year = 5,
        .month = 1,
        .day = 5,
        .weekday = 1,
        .hour = 14,
        .minute = 30,
    };
    RadioClockMinuteFrame frame;
    uint8_t expected[8] = {0};

    radio_clock_protocol_prepare_frame(RadioClockSignalDcf77, &frame, &time);
    set_dcf_message(expected, 30, 14, 5, 1, 26, 1, false, false, false, false, 0x0000);

    assert(memcmp(frame.encoded, expected, sizeof(expected)) == 0);
    for(uint8_t second = 0; second < 59U; second++) {
        const RadioClockPulse expected_pulse =
            dcf77_message_get_bit(expected, second) ? RadioClockPulseOne : RadioClockPulseZero;
        assert(frame.pulses[second] == expected_pulse);
    }
    assert(frame.pulses[59] == RadioClockPulseMarker);
}

static void test_wwvb_protocol_uses_flipper_time_with_safe_aux_flags(void) {
    RadioClockProtocolTime time = {
        .year = 2024,
        .day_of_year = 60,
        .month = 2,
        .day = 29,
        .weekday = 4,
        .hour = 23,
        .minute = 59,
    };
    RadioClockMinuteFrame frame;

    radio_clock_protocol_prepare_frame(RadioClockSignalWwvb, &frame, &time);

    assert(frame.pulses[0] == RadioClockPulseMarker);
    assert(frame.pulses[9] == RadioClockPulseMarker);
    assert(frame.pulses[59] == RadioClockPulseMarker);
    assert(frame.pulses[55] == RadioClockPulseOne);
    assert(frame.pulses[56] == RadioClockPulseZero);
    assert(frame.pulses[57] == RadioClockPulseZero);
    assert(frame.pulses[58] == RadioClockPulseZero);
    assert(frame.pulses[36] == RadioClockPulseZero);
    assert(frame.pulses[37] == RadioClockPulseZero);
    assert(frame.pulses[38] == RadioClockPulseZero);
    assert(frame.pulses[40] == RadioClockPulseZero);
    assert(frame.pulses[41] == RadioClockPulseZero);
    assert(frame.pulses[42] == RadioClockPulseZero);
    assert(frame.pulses[43] == RadioClockPulseZero);
}

static void test_wwvb_protocol_generates_safe_aux_frame(void) {
    RadioClockProtocolTime time = {
        .year = 2001,
        .day_of_year = 258,
        .month = 9,
        .day = 15,
        .weekday = 6,
        .hour = 18,
        .minute = 42,
    };
    RadioClockMinuteFrame frame;
    char actual[RADIO_CLOCK_FRAME_SECONDS + 1];

    radio_clock_protocol_prepare_frame(RadioClockSignalWwvb, &frame, &time);
    frame_to_string(frame.pulses, actual);

    assert(
        strcmp(
            actual,
            "M10000010M000101000M001000101M100000000M000000000M000100000M") == 0);
}

static void test_protocol_pulse_timing_and_phase_rules(void) {
    assert(radio_clock_protocol_pulse_to_high_ticks(RadioClockSignalDcf77, RadioClockPulseZero) == 29491U);
    assert(radio_clock_protocol_pulse_to_high_ticks(RadioClockSignalDcf77, RadioClockPulseOne) == 26214U);
    assert(radio_clock_protocol_pulse_to_high_ticks(RadioClockSignalDcf77, RadioClockPulseMarker) == 32768U);
    assert(radio_clock_protocol_pulse_to_high_ticks(RadioClockSignalWwvb, RadioClockPulseZero) == 26214U);
    assert(radio_clock_protocol_pulse_to_high_ticks(RadioClockSignalWwvb, RadioClockPulseOne) == 16384U);
    assert(radio_clock_protocol_pulse_to_high_ticks(RadioClockSignalWwvb, RadioClockPulseMarker) == 6553U);

    assert(radio_clock_protocol_starts_low(RadioClockSignalDcf77, RadioClockPulseZero) == true);
    assert(radio_clock_protocol_starts_low(RadioClockSignalDcf77, RadioClockPulseMarker) == false);
    assert(radio_clock_protocol_starts_low(RadioClockSignalWwvb, RadioClockPulseZero) == true);
    assert(radio_clock_protocol_starts_low(RadioClockSignalWwvb, RadioClockPulseMarker) == true);
}

int main(void) {
    test_dcf77_protocol_matches_known_frame();
    test_wwvb_protocol_uses_flipper_time_with_safe_aux_flags();
    test_wwvb_protocol_generates_safe_aux_frame();
    test_protocol_pulse_timing_and_phase_rules();
    puts("test_radio_clock_protocol: OK");
    return 0;
}
