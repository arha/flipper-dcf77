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

static void dcf77_logic_swap_message(AppFSM* app_fsm) {
    if(app_fsm->buffer_swap_pending) {
        memcpy(app_fsm->dcf77_message, app_fsm->next_message, sizeof(app_fsm->dcf77_message));
        app_fsm->buffer_swap_pending = false;
    }
}

void dcf77_logic_init(AppFSM* app_fsm) {
    app_fsm->bit_number = 0;
    app_fsm->bit_value = 0;
    app_fsm->baseband_counter = 0;
    app_fsm->buffer_swap_pending = false;
    app_fsm->output_state = false;
    app_fsm->last_second = 61;
    app_fsm->last_output = false;
}

void dcf77_logic_refresh_message(AppFSM* app_fsm, const DateTime* dt) {
    app_fsm->bit_number = dt->second;

    app_fsm->tx_hour = dt->hour;
    app_fsm->tx_minute = dt->minute;
    app_fsm->tx_day = dt->day;
    app_fsm->tx_month = dt->month;
    app_fsm->tx_year = dt->year % 100;
    app_fsm->tx_dow = dt->weekday;

    set_dcf_message(
        app_fsm->next_message,
        app_fsm->tx_minute,
        app_fsm->tx_hour,
        app_fsm->tx_day,
        app_fsm->tx_month,
        app_fsm->tx_year,
        app_fsm->tx_dow,
        false,
        false,
        false,
        false,
        0x0000);

    app_fsm->buffer_swap_pending = true;
    dcf77_logic_swap_message(app_fsm);
}

bool dcf77_logic_tick(AppFSM* app_fsm, const DateTime* dt, bool* output_changed) {
    bool output = true;

    app_fsm->bit_number = dt->second;
    app_fsm->bit_value = dcf77_get_message_bit(app_fsm->dcf77_message, app_fsm->bit_number);

    if(dt->second != app_fsm->last_second) {
        app_fsm->baseband_counter = 0;
    } else {
        app_fsm->baseband_counter++;
    }

    if(dt->second == 0 && app_fsm->baseband_counter == 3) {
        dcf77_logic_refresh_message(app_fsm, dt);
    }

    if(dt->second != 59) {
        if(app_fsm->bit_value == 0 && app_fsm->baseband_counter > TIME_ZERO) {
            output = false;
        } else if(app_fsm->bit_value == 1 && app_fsm->baseband_counter > TIME_ONE) {
            output = false;
        }
    }

    *output_changed = (app_fsm->last_output != output);
    app_fsm->output_state = output;
    app_fsm->last_second = dt->second;
    app_fsm->last_output = output;

    return output;
}
