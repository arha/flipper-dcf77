#ifndef RADIO_CLOCK_H
#define RADIO_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RadioClockSignalTest,
    RadioClockSignalDcf77,
    RadioClockSignalWwbb,
    RadioClockSignalBpc,
    RadioClockSignalJjy,
    RadioClockSignalMsf,
    RadioClockSignalRbu,
    RadioClockSignalCount,
} RadioClockSignal;

typedef struct {
    const char* label;
    uint32_t default_freq;
    bool tx_supported;
} RadioClockSignalInfo;

const RadioClockSignalInfo* radio_clock_signal_get_info(RadioClockSignal signal);
const char* radio_clock_signal_get_label(RadioClockSignal signal);
uint32_t radio_clock_signal_get_default_freq(RadioClockSignal signal);
bool radio_clock_signal_can_run(RadioClockSignal signal);

#endif
