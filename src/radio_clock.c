#include "radio_clock.h"

static const RadioClockSignalInfo radio_clock_signal_info[RadioClockSignalCount] = {
    [RadioClockSignalTest] = {
        .label = "Test",
        .default_freq = 77500U,
        .tx_supported = true,
    },
    [RadioClockSignalDcf77] = {
        .label = "DCF77",
        .default_freq = 77500U,
        .tx_supported = true,
    },
    [RadioClockSignalWwbb] = {
        .label = "WWBB",
        .default_freq = 60000U,
        .tx_supported = false,
    },
    [RadioClockSignalBpc] = {
        .label = "BPC",
        .default_freq = 67500U,
        .tx_supported = false,
    },
    [RadioClockSignalJjy] = {
        .label = "JJY",
        .default_freq = 60000U,
        .tx_supported = false,
    },
    [RadioClockSignalMsf] = {
        .label = "MSF",
        .default_freq = 60000U,
        .tx_supported = false,
    },
    [RadioClockSignalRbu] = {
        .label = "RBU",
        .default_freq = 67500U,
        .tx_supported = false,
    },
};

const RadioClockSignalInfo* radio_clock_signal_get_info(RadioClockSignal signal) {
    if(signal >= RadioClockSignalCount) {
        signal = RadioClockSignalDcf77;
    }

    return &radio_clock_signal_info[signal];
}

const char* radio_clock_signal_get_label(RadioClockSignal signal) {
    return radio_clock_signal_get_info(signal)->label;
}

uint32_t radio_clock_signal_get_default_freq(RadioClockSignal signal) {
    return radio_clock_signal_get_info(signal)->default_freq;
}

bool radio_clock_signal_can_run(RadioClockSignal signal) {
    return radio_clock_signal_get_info(signal)->tx_supported;
}
