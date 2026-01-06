#include "dcf77_logic.h"
#include "dcf77_app.h"

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
}

static void dcf77_logic_compute_high_ticks(const uint8_t* message, uint16_t* ticks_out) {
    for(uint8_t second = 0; second < DCF77_SECONDS_PER_MINUTE; second++) {
        if(second == 59) {
            ticks_out[second] = DCF77_FULL_SECOND_TICKS;
        } else {
            ticks_out[second] =
                dcf77_get_message_bit(message, second) ? DCF77_ONE_HIGH_TICKS : DCF77_ZERO_HIGH_TICKS;
        }
    }
}

static void dcf77_logic_prepare_buffers(
    AppFSM* app_fsm,
    const DateTime* dt,
    uint8_t* message_out,
    uint16_t* ticks_out) {
    UNUSED(app_fsm);
    set_dcf_message(
        message_out,
        dt->minute,
        dt->hour,
        dt->day,
        dt->month,
        dt->year % 100,
        dt->weekday,
        false,
        false,
        false,
        false,
        0x0000);

    dcf77_logic_compute_high_ticks(message_out, ticks_out);
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
}

void dcf77_logic_prepare_minute(AppFSM* app_fsm, const DateTime* dt, bool as_next_minute) {
    if(as_next_minute) {
        app_fsm->next_minute_dt = *dt;
        dcf77_logic_prepare_buffers(
            app_fsm, dt, app_fsm->next_message, app_fsm->next_second_high_ticks);
    } else {
        app_fsm->current_minute_dt = *dt;
        dcf77_logic_prepare_buffers(
            app_fsm, dt, app_fsm->dcf77_message, app_fsm->second_high_ticks);
        dcf77_logic_set_tx_fields(app_fsm, dt);
    }
}

void dcf77_logic_activate_next_minute(AppFSM* app_fsm) {
    memcpy(app_fsm->dcf77_message, app_fsm->next_message, sizeof(app_fsm->dcf77_message));
    memcpy(app_fsm->second_high_ticks, app_fsm->next_second_high_ticks, sizeof(app_fsm->second_high_ticks));
    app_fsm->current_minute_dt = app_fsm->next_minute_dt;
    dcf77_logic_set_tx_fields(app_fsm, &app_fsm->current_minute_dt);
}

void dcf77_logic_sync_to_second(AppFSM* app_fsm, const DateTime* dt) {
    app_fsm->scheduler_second = dt->second;
    app_fsm->scheduler_low_phase = (dt->second != 59);
    app_fsm->bit_number = dt->second;
    app_fsm->bit_value = dcf77_get_message_bit(app_fsm->dcf77_message, app_fsm->bit_number);
    app_fsm->output_state = (dt->second == 59);
    app_fsm->output_dirty = true;
    app_fsm->scheduler_ready = true;
    app_fsm->scheduler_synced = true;
}

uint16_t dcf77_logic_get_current_high_ticks(const AppFSM* app_fsm) {
    return app_fsm->second_high_ticks[app_fsm->scheduler_second];
}
