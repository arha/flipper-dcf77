#include "dcf77_logic.h"
#include "dcf77_app.h"
#include "wwvb_util.h"

const char* dcf77_logic_get_pulse_label(RadioClockPulse pulse) {
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

uint8_t dcf77_get_message_bit(const uint8_t* message, uint8_t bit) {
    if(bit == 59 || bit == 0) {
        return 0;
    }

    const uint8_t byte_index = bit / 8;
    const uint8_t bit_index = bit % 8;
    return !!(message[byte_index] & (1 << (7 - bit_index)));
}

static void dcf77_logic_set_tx_fields(AppFSM* app_fsm, const DateTime* dt) {
    app_fsm->tx_hour = dt->hour;
    app_fsm->tx_minute = dt->minute;
    app_fsm->tx_day = dt->day;
    app_fsm->tx_month = dt->month;
    app_fsm->tx_year = dt->year % 100;
    app_fsm->tx_dow = dt->weekday;
    app_fsm->tx_day_of_year = wwvb_day_of_year(dt->year, dt->month, dt->day);
}

static void dcf77_logic_compute_dcf77_pulses(
    const uint8_t* message,
    RadioClockPulse* pulses_out,
    uint16_t* ticks_out) {
    for(uint8_t second = 0; second < DCF77_SECONDS_PER_MINUTE; second++) {
        if(second == 59) {
            pulses_out[second] = RadioClockPulseMarker;
            ticks_out[second] = DCF77_FULL_SECOND_TICKS;
        } else {
            const bool bit = dcf77_get_message_bit(message, second);
            pulses_out[second] = bit ? RadioClockPulseOne : RadioClockPulseZero;
            ticks_out[second] = bit ? DCF77_ONE_HIGH_TICKS : DCF77_ZERO_HIGH_TICKS;
        }
    }
}

static uint16_t dcf77_logic_pulse_to_high_ticks(RadioClockSignal signal, RadioClockPulse pulse) {
    if(signal == RadioClockSignalWwvb) {
        switch(pulse) {
        case RadioClockPulseOne:
            return (DCF77_LPTIM_HZ * 5U) / 10U;
        case RadioClockPulseMarker:
            return (DCF77_LPTIM_HZ * 2U) / 10U;
        case RadioClockPulseZero:
        default:
            return (DCF77_LPTIM_HZ * 8U) / 10U;
        }
    }

    switch(pulse) {
    case RadioClockPulseOne:
        return DCF77_ONE_HIGH_TICKS;
    case RadioClockPulseMarker:
        return DCF77_FULL_SECOND_TICKS;
    case RadioClockPulseZero:
    default:
        return DCF77_ZERO_HIGH_TICKS;
    }
}

static void dcf77_logic_compute_generic_high_ticks(
    RadioClockSignal signal,
    const RadioClockPulse* pulses,
    uint16_t* ticks_out) {
    for(uint8_t second = 0; second < DCF77_SECONDS_PER_MINUTE; second++) {
        ticks_out[second] = dcf77_logic_pulse_to_high_ticks(signal, pulses[second]);
    }
}

static uint8_t dcf77_logic_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t result = days[month - 1U];

    if(month == 2U) {
        const bool leap = ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
        if(leap) {
            result = 29U;
        }
    }

    return result;
}

static uint8_t dcf77_logic_last_sunday(uint16_t year, uint8_t month) {
    DateTime dt = {
        .year = year,
        .month = month,
        .day = dcf77_logic_days_in_month(year, month),
        .hour = 0,
        .minute = 0,
        .second = 0,
    };
    datetime_timestamp_to_datetime(datetime_datetime_to_timestamp(&dt), &dt);
    return dt.day - (dt.weekday % 7U);
}

static bool dcf77_logic_is_dst(const DateTime* dt) {
    const uint16_t year = dt->year;

    if(dt->month < 3U || dt->month > 10U) {
        return false;
    }

    if(dt->month > 3U && dt->month < 10U) {
        return true;
    }

    const uint8_t switch_day = dcf77_logic_last_sunday(year, dt->month);

    if(dt->month == 3U) {
        if(dt->day > switch_day) {
            return true;
        }
        if(dt->day < switch_day) {
            return false;
        }
        return dt->hour >= 3U;
    }

    if(dt->day < switch_day) {
        return true;
    }
    if(dt->day > switch_day) {
        return false;
    }
    return dt->hour < 4U;
}

static void dcf77_logic_prepare_dcf77_buffers(
    AppFSM* app_fsm,
    const DateTime* dt,
    uint8_t* message_out,
    RadioClockPulse* pulses_out,
    uint16_t* ticks_out) {
    UNUSED(app_fsm);
    const bool is_dst = dcf77_logic_is_dst(dt);

    set_dcf_message(
        message_out,
        dt->minute,
        dt->hour,
        dt->day,
        dt->month,
        dt->year % 100,
        dt->weekday,
        is_dst,
        false,
        false,
        false,
        0x0000);

    dcf77_logic_compute_dcf77_pulses(message_out, pulses_out, ticks_out);
}

static void dcf77_logic_prepare_wwvb_buffers(
    AppFSM* app_fsm,
    const DateTime* dt,
    RadioClockPulse* pulses_out,
    uint16_t* ticks_out) {
    const bool is_dst = dcf77_logic_is_dst(dt);
    const bool leap_year =
        ((dt->year % 4U) == 0U) && (((dt->year % 100U) != 0U) || ((dt->year % 400U) == 0U));

    memset(app_fsm->dcf77_message, 0, sizeof(app_fsm->dcf77_message));
    set_wwvb_am_symbols(
        pulses_out,
        dt->minute,
        dt->hour,
        wwvb_day_of_year(dt->year, dt->month, dt->day),
        dt->year % 100,
        is_dst,
        is_dst,
        leap_year,
        false,
        0);
    dcf77_logic_compute_generic_high_ticks(RadioClockSignalWwvb, pulses_out, ticks_out);
}

void dcf77_logic_init(AppFSM* app_fsm) {
    app_fsm->bit_number = 0;
    app_fsm->bit_value = 0;
    app_fsm->output_state = false;
    app_fsm->output_dirty = false;
    app_fsm->scheduler_second = 0;
    app_fsm->scheduler_low_phase = false;
    app_fsm->scheduler_ready = false;
    app_fsm->scheduler_synced = false;
    app_fsm->startup_marker_wrap_pending = false;
}

void dcf77_logic_prepare_minute(AppFSM* app_fsm, const DateTime* dt, bool as_next_minute) {
    if(as_next_minute) {
        app_fsm->next_minute_dt = *dt;
        if(app_fsm->current_signal == RadioClockSignalWwvb) {
            dcf77_logic_prepare_wwvb_buffers(
                app_fsm, dt, app_fsm->next_pulse_frame, app_fsm->next_second_high_ticks);
        } else {
            dcf77_logic_prepare_dcf77_buffers(
                app_fsm,
                dt,
                app_fsm->next_message,
                app_fsm->next_pulse_frame,
                app_fsm->next_second_high_ticks);
        }
    } else {
        app_fsm->current_minute_dt = *dt;
        if(app_fsm->current_signal == RadioClockSignalWwvb) {
            dcf77_logic_prepare_wwvb_buffers(
                app_fsm, dt, app_fsm->pulse_frame, app_fsm->second_high_ticks);
        } else {
            dcf77_logic_prepare_dcf77_buffers(
                app_fsm,
                dt,
                app_fsm->dcf77_message,
                app_fsm->pulse_frame,
                app_fsm->second_high_ticks);
        }
        dcf77_logic_set_tx_fields(app_fsm, dt);
    }
}

void dcf77_logic_activate_next_minute(AppFSM* app_fsm) {
    memcpy(app_fsm->dcf77_message, app_fsm->next_message, sizeof(app_fsm->dcf77_message));
    memcpy(app_fsm->pulse_frame, app_fsm->next_pulse_frame, sizeof(app_fsm->pulse_frame));
    memcpy(app_fsm->second_high_ticks, app_fsm->next_second_high_ticks, sizeof(app_fsm->second_high_ticks));
    app_fsm->current_minute_dt = app_fsm->next_minute_dt;
    dcf77_logic_set_tx_fields(app_fsm, &app_fsm->current_minute_dt);
}

void dcf77_logic_sync_start(AppFSM* app_fsm, uint8_t start_second, bool startup_marker_wrap_pending) {
    app_fsm->scheduler_second = start_second;
    app_fsm->scheduler_low_phase =
        (app_fsm->pulse_frame[start_second] != RadioClockPulseMarker) ||
        (app_fsm->current_signal == RadioClockSignalWwvb);
    app_fsm->bit_number = start_second;
    app_fsm->bit_value = app_fsm->pulse_frame[app_fsm->bit_number];
    app_fsm->output_state =
        (app_fsm->current_signal == RadioClockSignalDcf77 && start_second == 59) ? true : false;
    app_fsm->output_dirty = true;
    app_fsm->scheduler_ready = true;
    app_fsm->scheduler_synced = true;
    app_fsm->startup_marker_wrap_pending = startup_marker_wrap_pending;
}

void dcf77_logic_sync_to_second(AppFSM* app_fsm, const DateTime* dt) {
    dcf77_logic_sync_start(app_fsm, dt->second, false);
}

uint16_t dcf77_logic_get_current_high_ticks(const AppFSM* app_fsm) {
    return app_fsm->second_high_ticks[app_fsm->scheduler_second];
}
