#include "radio_clock_protocol.h"

#include <string.h>

#include "dcf77_util.h"
#include "wwvb_util.h"

#define RADIO_CLOCK_LPTIM_HZ 32768U
#define RADIO_CLOCK_FULL_SECOND_TICKS RADIO_CLOCK_LPTIM_HZ
#define RADIO_CLOCK_DCF77_ZERO_HIGH_TICKS ((RADIO_CLOCK_LPTIM_HZ * 9U) / 10U)
#define RADIO_CLOCK_DCF77_ONE_HIGH_TICKS ((RADIO_CLOCK_LPTIM_HZ * 8U) / 10U)
#define RADIO_CLOCK_WWVB_ZERO_HIGH_TICKS ((RADIO_CLOCK_LPTIM_HZ * 8U) / 10U)
#define RADIO_CLOCK_WWVB_ONE_HIGH_TICKS ((RADIO_CLOCK_LPTIM_HZ * 5U) / 10U)
#define RADIO_CLOCK_WWVB_MARKER_HIGH_TICKS ((RADIO_CLOCK_LPTIM_HZ * 2U) / 10U)

static uint8_t radio_clock_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t result = days[month - 1U];

    if(month == 2U && wwvb_is_leap_year(year)) {
        result = 29U;
    }

    return result;
}

static uint8_t radio_clock_last_sunday(uint16_t year, uint8_t month) {
    uint8_t day = radio_clock_days_in_month(year, month);
    static const uint8_t month_offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t adjusted_year = year;

    if(month < 3U) {
        adjusted_year--;
    }

    const uint8_t weekday = (uint8_t)(
        (adjusted_year + (adjusted_year / 4U) - (adjusted_year / 100U) +
         (adjusted_year / 400U) + month_offsets[month - 1U] + day) %
        7U);
    return (uint8_t)(day - weekday);
}

static bool dcf77_is_dst(const RadioClockProtocolTime* time) {
    if(time->month < 3U || time->month > 10U) {
        return false;
    }

    if(time->month > 3U && time->month < 10U) {
        return true;
    }

    const uint8_t switch_day = radio_clock_last_sunday(time->year, time->month);

    if(time->month == 3U) {
        if(time->day > switch_day) {
            return true;
        }
        if(time->day < switch_day) {
            return false;
        }
        return time->hour >= 3U;
    }

    if(time->day < switch_day) {
        return true;
    }
    if(time->day > switch_day) {
        return false;
    }
    return time->hour < 4U;
}

static void dcf77_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    memset(frame, 0, sizeof(*frame));

    set_dcf_message(
        frame->encoded,
        time->minute,
        time->hour,
        time->day,
        time->month,
        (uint8_t)(time->year % 100U),
        time->weekday,
        dcf77_is_dst(time),
        false,
        false,
        false,
        0x0000);

    for(uint8_t second = 0; second < RADIO_CLOCK_FRAME_SECONDS; second++) {
        if(second == 59U) {
            frame->pulses[second] = RadioClockPulseMarker;
        } else {
            frame->pulses[second] =
                dcf77_message_get_bit(frame->encoded, second) ? RadioClockPulseOne :
                                                               RadioClockPulseZero;
        }
    }
}

static void wwvb_prepare_frame(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time) {
    memset(frame, 0, sizeof(*frame));

    set_wwvb_am_symbols(
        frame->pulses,
        time->minute,
        time->hour,
        time->day_of_year,
        (uint8_t)(time->year % 100U),
        false,
        false,
        wwvb_is_leap_year(time->year),
        false,
        0);
}

static uint16_t dcf77_pulse_to_high_ticks(RadioClockPulse pulse) {
    switch(pulse) {
    case RadioClockPulseOne:
        return RADIO_CLOCK_DCF77_ONE_HIGH_TICKS;
    case RadioClockPulseMarker:
        return RADIO_CLOCK_FULL_SECOND_TICKS;
    case RadioClockPulseZero:
    default:
        return RADIO_CLOCK_DCF77_ZERO_HIGH_TICKS;
    }
}

static uint16_t wwvb_pulse_to_high_ticks(RadioClockPulse pulse) {
    switch(pulse) {
    case RadioClockPulseOne:
        return RADIO_CLOCK_WWVB_ONE_HIGH_TICKS;
    case RadioClockPulseMarker:
        return RADIO_CLOCK_WWVB_MARKER_HIGH_TICKS;
    case RadioClockPulseZero:
    default:
        return RADIO_CLOCK_WWVB_ZERO_HIGH_TICKS;
    }
}

static bool dcf77_starts_low(RadioClockPulse pulse) {
    return pulse != RadioClockPulseMarker;
}

static bool wwvb_starts_low(RadioClockPulse pulse) {
    (void)pulse;
    return true;
}

static const RadioClockProtocolOps radio_clock_protocol_dcf77 = {
    .prepare_frame = dcf77_prepare_frame,
    .pulse_to_high_ticks = dcf77_pulse_to_high_ticks,
    .starts_low = dcf77_starts_low,
};

static const RadioClockProtocolOps radio_clock_protocol_wwvb = {
    .prepare_frame = wwvb_prepare_frame,
    .pulse_to_high_ticks = wwvb_pulse_to_high_ticks,
    .starts_low = wwvb_starts_low,
};

const RadioClockProtocolOps* radio_clock_protocol_get(RadioClockSignal signal) {
    switch(signal) {
    case RadioClockSignalWwvb:
        return &radio_clock_protocol_wwvb;
    case RadioClockSignalDcf77:
    default:
        return &radio_clock_protocol_dcf77;
    }
}

const char* radio_clock_protocol_pulse_label(RadioClockPulse pulse) {
    switch(pulse) {
    case RadioClockPulseOne:
        return "1";
    case RadioClockPulseMarker:
        return "M";
    case RadioClockPulseZero:
    default:
        return "0";
    }
}

uint16_t radio_clock_protocol_day_of_year(uint16_t year, uint8_t month, uint8_t day) {
    return wwvb_day_of_year(year, month, day);
}

void radio_clock_protocol_prepare_frame(
    RadioClockSignal signal,
    RadioClockMinuteFrame* frame,
    const RadioClockProtocolTime* time) {
    radio_clock_protocol_get(signal)->prepare_frame(frame, time);
}

void radio_clock_protocol_build_high_ticks(
    RadioClockSignal signal,
    const RadioClockPulse* pulses,
    uint16_t* ticks_out,
    size_t count) {
    const RadioClockProtocolOps* protocol = radio_clock_protocol_get(signal);

    for(size_t second = 0; second < count; second++) {
        ticks_out[second] = protocol->pulse_to_high_ticks(pulses[second]);
    }
}

uint16_t radio_clock_protocol_pulse_to_high_ticks(RadioClockSignal signal, RadioClockPulse pulse) {
    return radio_clock_protocol_get(signal)->pulse_to_high_ticks(pulse);
}

bool radio_clock_protocol_starts_low(RadioClockSignal signal, RadioClockPulse pulse) {
    return radio_clock_protocol_get(signal)->starts_low(pulse);
}
