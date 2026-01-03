#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dolphin/dolphin.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "dcf77_app.h"
#include "dcf77_hw.h"
#include "dcf77_logic.h"

const NotificationSequence seq_c_minor = {
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,
    &message_note_ds4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,
    &message_note_g4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

const char* const dcf77_bitnames[] = {
    "Start minute", "Civil 1",   "Civil 2",   "Civil 3",  "Civil 4",
    "Civil 5",      "Civil 6",   "Civil 7",   "Civil 8",  "Civil 9",
    "Civil 10",     "Civil 11",  "Civil 12",  "Civil 13", "Civil 14",
    "Abnormal",     "DST change","UTC+02",    "UTC+01",   "Leap sec",
    "Start time",   "Minutes 1", "Minutes 2", "Minutes 3","Minutes 4",
    "Minutes 5",    "Minutes 6", "Minutes 7", "Minutes P","Hours 1",
    "Hours 2",      "Hours 3",   "Hours 4",   "Hours 5",  "Hours 6",
    "Hours P",      "Day 1",     "Day 2",     "Day 3",    "Day 4",
    "Day 5",        "Day 6",     "Weekday 1", "Weekday 2","Weekday 4",
    "Month 1",      "Month 2",   "Month 3",   "Month 4",  "Month 5",
    "Year 1",       "Year 2",    "Year 3",    "Year 4",   "Year 5",
    "Year 6",       "Year 7",    "Year 8",    "Date P",   "End",
};

static void render_callback(Canvas* const canvas, void* ctx) {
    AppFSM* app_fsm = ctx;
    char buffer[64];
    const uint8_t yoffset = 9;
    const uint8_t bit_number = app_fsm->bit_number;
    const uint8_t bit_value = app_fsm->bit_value;
    uint8_t underline_x = (bit_number / 8) * 12 + 16;
    const uint8_t byte = app_fsm->dcf77_message[bit_number / 8];
    char display_bits[9] = "00000000";

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d %02d.%02d.%02d",
        app_fsm->tx_hour,
        app_fsm->tx_minute,
        app_fsm->tx_day,
        app_fsm->tx_month,
        app_fsm->tx_year);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, buffer);

    if(app_fsm->debug_flag) {
        canvas_draw_str_aligned(canvas, 8, 10, AlignCenter, AlignBottom, "D");
    }

    canvas_set_font(canvas, FontSecondary);
    snprintf(buffer, sizeof(buffer), "%1x.%1x=%01x", bit_number / 8, bit_number % 8, bit_value);
    canvas_draw_str_aligned(canvas, 64, 24 + (9 - yoffset), AlignCenter, AlignBottom, buffer);

    snprintf(buffer, sizeof(buffer), "%s (bit %d)", dcf77_bitnames[bit_number], bit_value);
    canvas_draw_str_aligned(canvas, 64, 34 + (9 - yoffset), AlignCenter, AlignBottom, buffer);

    canvas_set_font(canvas, FontKeyboard);
    snprintf(
        buffer,
        sizeof(buffer),
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        app_fsm->dcf77_message[0],
        app_fsm->dcf77_message[1],
        app_fsm->dcf77_message[2],
        app_fsm->dcf77_message[3],
        app_fsm->dcf77_message[4],
        app_fsm->dcf77_message[5],
        app_fsm->dcf77_message[6],
        app_fsm->dcf77_message[7]);
    canvas_draw_str_aligned(canvas, 64, 44 + (9 - yoffset), AlignCenter, AlignBottom, buffer);

    canvas_draw_line(canvas, underline_x, 45, underline_x + 10, 45);
    for(int i = 0; i < 8; i++) {
        if(byte & (1 << (7 - i))) {
            display_bits[i] = '1';
        }
    }

    snprintf(buffer, sizeof(buffer), "%02x=%s", byte, display_bits);
    canvas_draw_str_aligned(canvas, 64, 54 + (9 - yoffset), AlignCenter, AlignBottom, buffer);
    underline_x = (bit_number % 8) * 6 + 49;
    canvas_draw_line(canvas, underline_x, 55, underline_x + 4, 55);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_assert(event_queue);

    AppEvent event = {.type = EventKeyPress, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void timer_tick_callback(void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_assert(event_queue);

    AppEvent event = {.type = EventTimerTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static void app_init(AppFSM* app_fsm, FuriMessageQueue* event_queue) {
    memset(app_fsm, 0, sizeof(*app_fsm));

    app_fsm->lf_freq = LF_FREQ_LOW;
    app_fsm->_event_queue = event_queue;

    dcf77_logic_init(app_fsm);
    dcf77_output_gpio_init();

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    dcf77_logic_refresh_message(app_fsm, &dt);

    dcf77_lf_set_frequency(app_fsm, app_fsm->lf_freq);

    app_fsm->_timer =
        furi_timer_alloc(timer_tick_callback, FuriTimerTypePeriodic, app_fsm->_event_queue);
    furi_timer_start(app_fsm->_timer, furi_kernel_get_tick_frequency() / TIMER_HZ);
}

static void app_deinit(AppFSM* app_fsm) {
    dcf77_lf_deinit(app_fsm);
    dcf77_subghz_deinit(app_fsm);
    dcf77_output_gpio_deinit();
    dcf77_hw_indicator_set(false);
    furi_timer_free(app_fsm->_timer);
}

static void on_timer_tick(AppFSM* app_fsm) {
    DateTime dt;
    bool output_changed = false;
    furi_hal_rtc_get_datetime(&dt);

    const bool output = dcf77_logic_tick(app_fsm, &dt, &output_changed);
    if(!output_changed) {
        return;
    }

    dcf77_hw_indicator_set(output);
    dcf77_output_gpio_set(output);
    dcf77_lf_apply_output(app_fsm);
    dcf77_subghz_apply_output(app_fsm);
}

int32_t dcf77_app_main(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    AppFSM* app_fsm = malloc(sizeof(AppFSM));
    app_init(app_fsm, event_queue);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, app_fsm);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message_block(notification, &sequence_display_backlight_enforce_on);

    if(furi_hal_speaker_acquire(500)) {
        ;
    }

    dolphin_deed(DolphinDeedPluginGameStart);

    AppEvent event;
    for(bool processing = true; processing;) {
        const FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        if(event_status == FuriStatusOk) {
            if(event.type == EventKeyPress && event.input.type == InputTypePress) {
                switch(event.input.key) {
                case InputKeyUp:
                    app_fsm->last_key = KeyUp;
                    dcf77_lf_set_frequency(
                        app_fsm,
                        app_fsm->lf_freq == LF_FREQ_LOW ? LF_FREQ_HIGH : LF_FREQ_LOW);
                    break;
                case InputKeyDown:
                    app_fsm->last_key = KeyDown;
                    break;
                case InputKeyRight:
                    app_fsm->last_key = KeyRight;
                    dcf77_subghz_toggle(app_fsm);
                    break;
                case InputKeyLeft:
                    app_fsm->last_key = KeyLeft;
                    break;
                case InputKeyOk:
                    app_fsm->last_key = KeyOK;
                    app_fsm->counter = -5;
                    break;
                case InputKeyBack:
                    processing = false;
                    break;
                default:
                    break;
                }
            } else if(event.type == EventTimerTick) {
                FURI_CRITICAL_ENTER();
                on_timer_tick(app_fsm);
                FURI_CRITICAL_EXIT();
            }
        }

        view_port_update(view_port);
    }

    furi_hal_speaker_release();
    notification_message_block(notification, &seq_c_minor);
    app_deinit(app_fsm);
    notification_message_block(notification, &sequence_display_backlight_enforce_auto);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    free(app_fsm);

    return 0;
}
