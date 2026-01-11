#ifndef RADIO_CLOCK_PROTOCOL_H
#define RADIO_CLOCK_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "radio_clock.h"
#include "radio_clock_pulse.h"

#define RADIO_CLOCK_FRAME_BYTES 8U
#define RADIO_CLOCK_FRAME_SECONDS 60U

typedef struct {
    uint16_t year;
    uint16_t day_of_year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
} RadioClockProtocolTime;

typedef struct {
    uint8_t encoded[RADIO_CLOCK_FRAME_BYTES];
    RadioClockPulse pulses[RADIO_CLOCK_FRAME_SECONDS];
} RadioClockMinuteFrame;

typedef struct {
    void (*prepare_frame)(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time);
    uint16_t (*pulse_to_high_ticks)(RadioClockPulse pulse);
    bool (*starts_low)(RadioClockPulse pulse);
} RadioClockProtocolOps;

const RadioClockProtocolOps* radio_clock_protocol_get(RadioClockSignal signal);
const char* radio_clock_protocol_pulse_label(RadioClockPulse pulse);
uint16_t radio_clock_protocol_day_of_year(uint16_t year, uint8_t month, uint8_t day);
void radio_clock_protocol_prepare_frame(
    RadioClockSignal signal,
    RadioClockMinuteFrame* frame,
    const RadioClockProtocolTime* time);
void radio_clock_protocol_build_high_ticks(
    RadioClockSignal signal,
    const RadioClockPulse* pulses,
    uint16_t* ticks_out,
    size_t count);
uint16_t radio_clock_protocol_pulse_to_high_ticks(RadioClockSignal signal, RadioClockPulse pulse);
bool radio_clock_protocol_starts_low(RadioClockSignal signal, RadioClockPulse pulse);

#endif
