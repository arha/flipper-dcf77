#include "dcf77_logic.h"
#include "dcf77_app.h"
#include "radio_clock_protocol.h"

const char* dcf77_logic_get_pulse_label(RadioClockPulse pulse) {
    return radio_clock_protocol_pulse_label(pulse);
}

static void dcf77_logic_set_tx_fields(AppFSM* app_fsm, const DateTime* dt) {
    app_fsm->tx_hour = dt->hour;
    app_fsm->tx_minute = dt->minute;
    app_fsm->tx_day = dt->day;
    app_fsm->tx_month = dt->month;
    app_fsm->tx_year = dt->year % 100;
    app_fsm->tx_dow = dt->weekday;
    app_fsm->tx_day_of_year = radio_clock_protocol_day_of_year(dt->year, dt->month, dt->day);
}

static void dcf77_logic_build_protocol_time(
    const DateTime* dt,
    RadioClockProtocolTime* protocol_time) {
    protocol_time->year = dt->year;
    protocol_time->day_of_year = radio_clock_protocol_day_of_year(dt->year, dt->month, dt->day);
    protocol_time->month = dt->month;
    protocol_time->day = dt->day;
    protocol_time->weekday = dt->weekday;
    protocol_time->hour = dt->hour;
    protocol_time->minute = dt->minute;
}

static void dcf77_logic_prepare_frame_buffers(
    AppFSM* app_fsm,
    const DateTime* dt,
    uint8_t* message_out,
    RadioClockPulse* pulses_out,
    uint16_t* ticks_out) {
    RadioClockProtocolTime protocol_time;
    RadioClockMinuteFrame frame;

    dcf77_logic_build_protocol_time(dt, &protocol_time);
    radio_clock_protocol_prepare_frame(app_fsm->current_signal, &frame, &protocol_time);
    memcpy(message_out, frame.encoded, sizeof(frame.encoded));
    memcpy(pulses_out, frame.pulses, sizeof(frame.pulses));
    radio_clock_protocol_build_high_ticks(
        app_fsm->current_signal, frame.pulses, ticks_out, DCF77_SECONDS_PER_MINUTE);
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
        dcf77_logic_prepare_frame_buffers(
            app_fsm,
            dt,
            app_fsm->next_message,
            app_fsm->next_pulse_frame,
            app_fsm->next_second_high_ticks);
    } else {
        app_fsm->current_minute_dt = *dt;
        dcf77_logic_prepare_frame_buffers(
            app_fsm, dt, app_fsm->dcf77_message, app_fsm->pulse_frame, app_fsm->second_high_ticks);
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
        radio_clock_protocol_starts_low(app_fsm->current_signal, app_fsm->pulse_frame[start_second]);
    app_fsm->bit_number = start_second;
    app_fsm->bit_value = app_fsm->pulse_frame[app_fsm->bit_number];
    app_fsm->output_state = !app_fsm->scheduler_low_phase;
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
