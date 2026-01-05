#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdlib.h>
#include <applications/services/gui/modules/submenu.h>
#include <applications/services/gui/modules/widget.h>
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

enum {
    Dcf77ViewStartup,
    Dcf77ViewMenu,
    Dcf77ViewTx,
    Dcf77ViewAbout,
    Dcf77ViewEgg,
};

enum {
    Dcf77MenuItemStart,
    Dcf77MenuItemAbout,
};

typedef struct {
    AppFSM* app_fsm;
} Dcf77TxViewModel;

static const char about_text[] =
    "Designed by arha, to debug my radioclocks that never seem to work just right.\n"
    "https://github.com/arha\n";

static const char egg_text[] = "wow you found an easter egg\n";

static void tx_render_callback(Canvas* const canvas, void* model) {
    Dcf77TxViewModel* tx_model = model;
    AppFSM* app_fsm = tx_model->app_fsm;
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

static void app_switch_to_menu(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenMenu;
    submenu_set_selected_item(app_fsm->submenu, Dcf77MenuItemStart);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewMenu);
}

static void app_switch_to_about(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenAbout;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewAbout);
}

static void app_switch_to_egg(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenEgg;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewEgg);
}

static void app_start_tx(AppFSM* app_fsm) {
    if(app_fsm->tx_active) {
        return;
    }

    DateTime dt;
    dcf77_wait_for_next_second(&dt);

    DateTime current_minute = dt;
    current_minute.second = 0;
    dcf77_logic_prepare_minute(app_fsm, &current_minute, false);

    DateTime next_minute = current_minute;
    uint32_t next_timestamp = datetime_datetime_to_timestamp(&next_minute) + 60U;
    datetime_timestamp_to_datetime(next_timestamp, &next_minute);
    dcf77_logic_prepare_minute(app_fsm, &next_minute, true);
    dcf77_logic_sync_to_second(app_fsm, &dt);

    dcf77_lf_set_frequency(app_fsm, app_fsm->lf_freq);
    dcf77_timing_start(app_fsm);
    app_fsm->tx_active = true;
    app_fsm->screen = AppScreenTx;
    app_fsm->last_tx_refresh_tick = furi_get_tick();
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewTx);
}

static void app_stop_tx(AppFSM* app_fsm) {
    if(!app_fsm->tx_active) {
        return;
    }

    dcf77_timing_stop(app_fsm);
    dcf77_lf_deinit(app_fsm);
    dcf77_subghz_deinit(app_fsm);
    dcf77_output_gpio_set(false);
    dcf77_hw_indicator_set(false);
    app_fsm->output_state = false;
    app_fsm->output_dirty = false;
    app_fsm->subghz_dirty = false;
    app_fsm->tx_active = false;
    app_switch_to_menu(app_fsm);
}

static uint32_t tx_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    notification_message_block(app_fsm->notification, &seq_c_minor);
    app_stop_tx(app_fsm);
    return Dcf77ViewMenu;
}

static uint32_t text_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    if(app_fsm->screen == AppScreenEgg) {
        app_switch_to_about(app_fsm);
        return Dcf77ViewAbout;
    }

    app_switch_to_menu(app_fsm);
    return Dcf77ViewMenu;
}

static void about_button_callback(GuiButtonType result, InputType type, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(type != InputTypePress) {
        return;
    }

    if(result == GuiButtonTypeLeft) {
        app_switch_to_menu(app_fsm);
    }
}

static void egg_button_callback(GuiButtonType result, InputType type, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(type != InputTypePress) {
        return;
    }

    if(result == GuiButtonTypeLeft) {
        app_switch_to_about(app_fsm);
    }
}

static bool about_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(event->type != InputTypePress) {
        return false;
    }

    switch(event->key) {
    case InputKeyLeft:
    case InputKeyBack:
        app_switch_to_menu(app_fsm);
        return true;
    case InputKeyRight:
        app_switch_to_egg(app_fsm);
        return true;
    default:
        return false;
    }
}

static bool egg_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(event->type == InputTypePress &&
       (event->key == InputKeyLeft || event->key == InputKeyBack)) {
        app_switch_to_about(app_fsm);
        return true;
    }

    return false;
}

static bool tx_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(event->type != InputTypePress) {
        return false;
    }

    switch(event->key) {
    case InputKeyUp:
        app_fsm->last_key = KeyUp;
        app_fsm->lf_freq = app_fsm->lf_freq == LF_FREQ_LOW ? LF_FREQ_HIGH : LF_FREQ_LOW;
        if(!app_fsm->subghz_enabled) {
            dcf77_lf_set_frequency(app_fsm, app_fsm->lf_freq);
        }
        return true;
    case InputKeyDown:
        app_fsm->last_key = KeyDown;
        return true;
    case InputKeyRight:
        app_fsm->last_key = KeyRight;
        dcf77_subghz_toggle(app_fsm);
        return true;
    case InputKeyLeft:
        app_fsm->last_key = KeyLeft;
        return true;
    case InputKeyOk:
        app_fsm->last_key = KeyOK;
        app_fsm->counter = -5;
        return true;
    case InputKeyBack:
        notification_message_block(app_fsm->notification, &seq_c_minor);
        app_stop_tx(app_fsm);
        return true;
    default:
        return false;
    }
}

static void menu_callback(void* ctx, uint32_t index) {
    AppFSM* app_fsm = ctx;

    switch(index) {
    case Dcf77MenuItemStart:
        app_start_tx(app_fsm);
        break;
    case Dcf77MenuItemAbout:
        app_switch_to_about(app_fsm);
        break;
    default:
        break;
    }
}

static bool navigation_callback(void* ctx) {
    AppFSM* app_fsm = ctx;

    if(app_fsm->screen == AppScreenTx) {
        app_stop_tx(app_fsm);
        return true;
    }

    view_dispatcher_stop(app_fsm->view_dispatcher);
    return true;
}

static void tick_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    const uint32_t now = furi_get_tick();

    if(app_fsm->screen == AppScreenStartup && now >= app_fsm->startup_deadline_tick) {
        app_switch_to_menu(app_fsm);
    }

    if(app_fsm->tx_active && app_fsm->output_dirty) {
        const bool output = app_fsm->output_state;
        app_fsm->output_dirty = false;
        dcf77_hw_indicator_set(output);
        if(app_fsm->subghz_enabled) {
            app_fsm->subghz_dirty = true;
        }
    }

    if(app_fsm->tx_active && app_fsm->subghz_dirty) {
        dcf77_subghz_apply_output(app_fsm);
    }

    if(app_fsm->screen == AppScreenTx && now - app_fsm->last_tx_refresh_tick >= 50U) {
        app_fsm->last_tx_refresh_tick = now;
        with_view_model(
            app_fsm->tx_view,
            Dcf77TxViewModel * tx_model,
            {
                UNUSED(tx_model);
            },
            true);
    }
}

static void app_init(AppFSM* app_fsm) {
    memset(app_fsm, 0, sizeof(*app_fsm));

    app_fsm->lf_freq = LF_FREQ_LOW;
    app_fsm->screen = AppScreenStartup;
    app_fsm->startup_deadline_tick =
        furi_get_tick() + ((STARTUP_SCREEN_MS * furi_kernel_get_tick_frequency()) / 1000U);

    dcf77_logic_init(app_fsm);
    dcf77_output_gpio_init();

    app_fsm->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app_fsm->view_dispatcher, app_fsm);
    view_dispatcher_set_navigation_event_callback(app_fsm->view_dispatcher, navigation_callback);
    view_dispatcher_set_tick_event_callback(app_fsm->view_dispatcher, tick_callback, 1);

    app_fsm->startup_widget = widget_alloc();
    widget_add_string_multiline_element(
        app_fsm->startup_widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        "DCF77 TX\n\ngithub.com/arha/\nflipper-dcf77");
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewStartup,
        widget_get_view(app_fsm->startup_widget));

    app_fsm->submenu = submenu_alloc();
    submenu_add_item(app_fsm->submenu, "Start", Dcf77MenuItemStart, menu_callback, app_fsm);
    submenu_add_item(app_fsm->submenu, "About", Dcf77MenuItemAbout, menu_callback, app_fsm);
    view_dispatcher_add_view(app_fsm->view_dispatcher, Dcf77ViewMenu, submenu_get_view(app_fsm->submenu));

    app_fsm->tx_view = view_alloc();
    view_allocate_model(app_fsm->tx_view, ViewModelTypeLockFree, sizeof(Dcf77TxViewModel));
    with_view_model(
        app_fsm->tx_view,
        Dcf77TxViewModel * tx_model,
        {
            tx_model->app_fsm = app_fsm;
        },
        false);
    view_set_context(app_fsm->tx_view, app_fsm);
    view_set_draw_callback(app_fsm->tx_view, tx_render_callback);
    view_set_input_callback(app_fsm->tx_view, tx_input_callback);
    view_set_previous_callback(app_fsm->tx_view, tx_previous_callback);
    view_dispatcher_add_view(app_fsm->view_dispatcher, Dcf77ViewTx, app_fsm->tx_view);

    app_fsm->about_widget = widget_alloc();
    widget_add_frame_element(app_fsm->about_widget, 0, 0, 128, 64, 0);
    widget_add_text_scroll_element(app_fsm->about_widget, 4, 4, 120, 46, about_text);
    widget_add_button_element(
        app_fsm->about_widget, GuiButtonTypeLeft, "Back", about_button_callback, app_fsm);
    view_set_context(widget_get_view(app_fsm->about_widget), app_fsm);
    view_set_input_callback(widget_get_view(app_fsm->about_widget), about_input_callback);
    view_set_previous_callback(widget_get_view(app_fsm->about_widget), text_previous_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewAbout, widget_get_view(app_fsm->about_widget));

    app_fsm->egg_widget = widget_alloc();
    widget_add_frame_element(app_fsm->egg_widget, 0, 0, 128, 64, 0);
    widget_add_text_scroll_element(app_fsm->egg_widget, 4, 4, 120, 46, egg_text);
    widget_add_button_element(
        app_fsm->egg_widget, GuiButtonTypeLeft, "Back", egg_button_callback, app_fsm);
    view_set_context(widget_get_view(app_fsm->egg_widget), app_fsm);
    view_set_input_callback(widget_get_view(app_fsm->egg_widget), egg_input_callback);
    view_set_previous_callback(widget_get_view(app_fsm->egg_widget), text_previous_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewEgg, widget_get_view(app_fsm->egg_widget));
}

static void app_deinit(AppFSM* app_fsm) {
    app_stop_tx(app_fsm);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewEgg);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewAbout);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewTx);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewMenu);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewStartup);
    widget_free(app_fsm->egg_widget);
    widget_free(app_fsm->about_widget);
    view_free(app_fsm->tx_view);
    submenu_free(app_fsm->submenu);
    widget_free(app_fsm->startup_widget);
    view_dispatcher_free(app_fsm->view_dispatcher);
    dcf77_output_gpio_deinit();
    dcf77_hw_indicator_set(false);
}

int32_t dcf77_app_main(void* p) {
    UNUSED(p);

    AppFSM* app_fsm = malloc(sizeof(AppFSM));
    app_init(app_fsm);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app_fsm->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    app_fsm->notification = notification;
    notification_message_block(notification, &sequence_display_backlight_enforce_on);

    dolphin_deed(DolphinDeedPluginGameStart);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewStartup);
    view_dispatcher_run(app_fsm->view_dispatcher);
    app_deinit(app_fsm);
    notification_message_block(notification, &sequence_display_backlight_enforce_auto);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app_fsm);

    return 0;
}
