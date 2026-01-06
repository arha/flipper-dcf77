#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdlib.h>
#include <applications/services/gui/modules/submenu.h>
#include <applications/services/gui/modules/variable_item_list.h>
#include <applications/services/gui/modules/widget.h>
#include <applications/services/storage/storage.h>
#include <dolphin/dolphin.h>
#include <toolbox/saved_struct.h>
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
    Dcf77ViewLfSettings,
    Dcf77ViewSubGhzSettings,
    Dcf77ViewDebugSettings,
    Dcf77ViewAbout,
    Dcf77ViewEgg,
};

enum {
    Dcf77MenuItemStart,
    Dcf77MenuItemLfSettings,
    Dcf77MenuItemSubGhzSettings,
    Dcf77MenuItemDebugSettings,
    Dcf77MenuItemAbout,
};

enum {
    Dcf77LfSettingTransmit,
    Dcf77LfSettingFrequency,
};

enum {
    Dcf77SubGhzSettingSignal,
};

enum {
    Dcf77DebugSettingSound,
};

typedef struct {
    uint32_t lf_freq;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_signal_mode;
    uint8_t sound_enabled;
} Dcf77SavedSettings;

#define DCF77_SETTINGS_DIR EXT_PATH("apps_data/dcf77")
#define DCF77_SETTINGS_PATH EXT_PATH("apps_data/dcf77/settings.bin")
#define DCF77_SETTINGS_MAGIC 0x44
#define DCF77_SETTINGS_VERSION 3

typedef struct {
    AppFSM* app_fsm;
} Dcf77TxViewModel;

static const char about_text[] =
    "Designed by arha, to debug my radioclocks that never seem to work just right.\n"
    "https://github.com/arha\n";

static const char egg_text[] = "wow you found an easter egg\n";

static void app_sound_set(AppFSM* app_fsm, bool enabled) {
    if(!enabled) {
        if(app_fsm->speaker_active && furi_hal_speaker_is_mine()) {
            furi_hal_speaker_stop();
            furi_hal_speaker_release();
        }
        app_fsm->speaker_active = false;
        return;
    }

    if(!furi_hal_speaker_is_mine()) {
        if(!furi_hal_speaker_acquire(30)) {
            return;
        }
    }

    furi_hal_speaker_start(440.0f, 0.6f);
    app_fsm->speaker_active = true;
}

static bool app_settings_save(const AppFSM* app_fsm) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool dir_ready =
        storage_common_exists(storage, DCF77_SETTINGS_DIR) ||
        (storage_simply_mkdir(storage, DCF77_SETTINGS_DIR) == true);
    furi_record_close(RECORD_STORAGE);

    if(!dir_ready) {
        return false;
    }

    const Dcf77SavedSettings settings = {
        .lf_freq = app_fsm->lf_freq,
        .lf_transmit_enabled = app_fsm->lf_transmit_enabled ? 1 : 0,
        .subghz_signal_mode = app_fsm->subghz_signal_mode,
        .sound_enabled = app_fsm->sound_enabled ? 1 : 0,
    };

    return saved_struct_save(
        DCF77_SETTINGS_PATH,
        &settings,
        sizeof(settings),
        DCF77_SETTINGS_MAGIC,
        DCF77_SETTINGS_VERSION);
}

static void app_settings_load(AppFSM* app_fsm) {
    Dcf77SavedSettings settings = {
        .lf_freq = LF_FREQ_DEFAULT,
        .lf_transmit_enabled = 1,
        .subghz_signal_mode = SubGhzSignalModeDisabled,
        .sound_enabled = 0,
    };

    if(saved_struct_load(
           DCF77_SETTINGS_PATH,
           &settings,
           sizeof(settings),
           DCF77_SETTINGS_MAGIC,
           DCF77_SETTINGS_VERSION)) {
        if(settings.lf_freq < LF_FREQ_MIN) {
            settings.lf_freq = LF_FREQ_MIN;
        } else if(settings.lf_freq > LF_FREQ_MAX) {
            settings.lf_freq = LF_FREQ_MAX;
        }

        const uint32_t offset = settings.lf_freq - LF_FREQ_MIN;
        settings.lf_freq = LF_FREQ_MIN + ((offset / LF_FREQ_STEP) * LF_FREQ_STEP);
        app_fsm->lf_freq = settings.lf_freq;
        app_fsm->lf_transmit_enabled = settings.lf_transmit_enabled != 0;
        if(settings.subghz_signal_mode > SubGhzSignalModeFsk) {
            settings.subghz_signal_mode = SubGhzSignalModeDisabled;
        }
        app_fsm->subghz_signal_mode = settings.subghz_signal_mode;
        app_fsm->sound_enabled = settings.sound_enabled != 0;
    } else {
        app_fsm->lf_freq = LF_FREQ_DEFAULT;
        app_fsm->lf_transmit_enabled = true;
        app_fsm->subghz_signal_mode = SubGhzSignalModeDisabled;
        app_fsm->sound_enabled = false;
    }
}

static void app_apply_rf_settings(AppFSM* app_fsm) {
    if(!app_fsm->tx_active) {
        return;
    }

    if(app_fsm->subghz_signal_mode == SubGhzSignalModeFsk) {
        dcf77_lf_deinit(app_fsm);
    } else if(app_fsm->lf_transmit_enabled) {
        dcf77_lf_set_frequency(app_fsm, app_fsm->lf_freq);
    } else {
        dcf77_lf_deinit(app_fsm);
    }

    dcf77_subghz_set_mode(app_fsm, app_fsm->subghz_signal_mode);
}

static uint8_t app_get_lf_freq_index(uint32_t freq) {
    if(freq <= LF_FREQ_MIN) {
        return 0;
    }

    if(freq >= LF_FREQ_MAX) {
        return (LF_FREQ_MAX - LF_FREQ_MIN) / LF_FREQ_STEP;
    }

    return (freq - LF_FREQ_MIN) / LF_FREQ_STEP;
}

static void app_update_lf_freq_text(AppFSM* app_fsm) {
    snprintf(app_fsm->lf_freq_text, sizeof(app_fsm->lf_freq_text), "%lu", (unsigned long)app_fsm->lf_freq);
}

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

static void app_switch_to_lf_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenLfSettings;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewLfSettings);
}

static void app_switch_to_subghz_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenSubGhzSettings;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewSubGhzSettings);
}

static void app_switch_to_debug_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenDebugSettings;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewDebugSettings);
}

static void app_start_tx(AppFSM* app_fsm) {
    if(app_fsm->tx_active) {
        return;
    }

    DateTime dt;
    dcf77_wait_for_next_second(&dt);
    app_fsm->tx_start_second_offset = dt.second;

    DateTime current_minute = dt;
    current_minute.second = 0;
    dcf77_logic_prepare_minute(app_fsm, &current_minute, false);

    DateTime next_minute = current_minute;
    uint32_t next_timestamp = datetime_datetime_to_timestamp(&next_minute) + 60U;
    datetime_timestamp_to_datetime(next_timestamp, &next_minute);
    dcf77_logic_prepare_minute(app_fsm, &next_minute, true);

    app_fsm->tx_active = true;
#if DEBUG_SYNC_FRAME
    dcf77_debug_sync_frame(app_fsm);
#endif
#if SYNC_ON_START
    dcf77_logic_sync_start(app_fsm, 50, true);
#else
    dcf77_logic_sync_to_second(app_fsm, &dt);
#endif
    dcf77_timing_start(app_fsm);
    app_apply_rf_settings(app_fsm);
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
    app_sound_set(app_fsm, false);
    app_fsm->output_state = false;
    app_fsm->output_dirty = false;
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

static void lf_transmit_change_callback(VariableItem* item) {
    AppFSM* app_fsm = variable_item_get_context(item);
    const uint8_t value_index = variable_item_get_current_value_index(item);

    app_fsm->lf_transmit_enabled = value_index == 0;
    variable_item_set_current_value_text(item, app_fsm->lf_transmit_enabled ? "Yes" : "No");
    app_apply_rf_settings(app_fsm);
    app_settings_save(app_fsm);
}

static void lf_frequency_change_callback(VariableItem* item) {
    AppFSM* app_fsm = variable_item_get_context(item);
    const uint8_t value_index = variable_item_get_current_value_index(item);

    app_fsm->lf_freq = LF_FREQ_MIN + ((uint32_t)value_index * LF_FREQ_STEP);
    app_update_lf_freq_text(app_fsm);
    variable_item_set_current_value_text(item, app_fsm->lf_freq_text);
    app_apply_rf_settings(app_fsm);
    app_settings_save(app_fsm);
}

static void subghz_signal_change_callback(VariableItem* item) {
    static const char* const labels[] = {"disabled", "OOK", "FSK"};
    AppFSM* app_fsm = variable_item_get_context(item);
    const uint8_t value_index = variable_item_get_current_value_index(item);

    app_fsm->subghz_signal_mode = (SubGhzSignalMode)value_index;
    variable_item_set_current_value_text(item, labels[value_index]);
    app_apply_rf_settings(app_fsm);
    app_settings_save(app_fsm);
}

static void debug_sound_change_callback(VariableItem* item) {
    AppFSM* app_fsm = variable_item_get_context(item);
    const uint8_t value_index = variable_item_get_current_value_index(item);

    app_fsm->sound_enabled = value_index == 0;
    variable_item_set_current_value_text(item, app_fsm->sound_enabled ? "Yes" : "No");
    if(!app_fsm->sound_enabled) {
        app_sound_set(app_fsm, false);
    }
    app_settings_save(app_fsm);
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
    case InputKeyDown:
        app_fsm->last_key = KeyDown;
        return true;
    case InputKeyRight:
        app_fsm->last_key = KeyRight;
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
    case Dcf77MenuItemLfSettings:
        app_switch_to_lf_settings(app_fsm);
        break;
    case Dcf77MenuItemSubGhzSettings:
        app_switch_to_subghz_settings(app_fsm);
        break;
    case Dcf77MenuItemDebugSettings:
        app_switch_to_debug_settings(app_fsm);
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

    if(app_fsm->screen == AppScreenLfSettings) {
        app_switch_to_menu(app_fsm);
        return true;
    }

    if(app_fsm->screen == AppScreenSubGhzSettings) {
        app_switch_to_menu(app_fsm);
        return true;
    }

    if(app_fsm->screen == AppScreenDebugSettings) {
        app_switch_to_menu(app_fsm);
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
        app_sound_set(app_fsm, app_fsm->sound_enabled && output);
        dcf77_subghz_sync_output(app_fsm);
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

    app_settings_load(app_fsm);
    app_update_lf_freq_text(app_fsm);
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
    submenu_add_item(app_fsm->submenu, "LF", Dcf77MenuItemLfSettings, menu_callback, app_fsm);
    submenu_add_item(
        app_fsm->submenu, "SubGHz", Dcf77MenuItemSubGhzSettings, menu_callback, app_fsm);
    submenu_add_item(app_fsm->submenu, "Debug", Dcf77MenuItemDebugSettings, menu_callback, app_fsm);
    submenu_add_item(app_fsm->submenu, "About", Dcf77MenuItemAbout, menu_callback, app_fsm);
    view_dispatcher_add_view(app_fsm->view_dispatcher, Dcf77ViewMenu, submenu_get_view(app_fsm->submenu));

    app_fsm->lf_settings = variable_item_list_alloc();
    VariableItem* item = variable_item_list_add(
        app_fsm->lf_settings, "LF transmit", 2, lf_transmit_change_callback, app_fsm);
    variable_item_set_current_value_index(item, app_fsm->lf_transmit_enabled ? 0 : 1);
    variable_item_set_current_value_text(item, app_fsm->lf_transmit_enabled ? "Yes" : "No");
    item = variable_item_list_add(
        app_fsm->lf_settings,
        "kHz",
        ((LF_FREQ_MAX - LF_FREQ_MIN) / LF_FREQ_STEP) + 1,
        lf_frequency_change_callback,
        app_fsm);
    variable_item_set_current_value_index(item, app_get_lf_freq_index(app_fsm->lf_freq));
    variable_item_set_current_value_text(item, app_fsm->lf_freq_text);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewLfSettings,
        variable_item_list_get_view(app_fsm->lf_settings));

    app_fsm->subghz_settings = variable_item_list_alloc();
    item = variable_item_list_add(
        app_fsm->subghz_settings, "signal", 3, subghz_signal_change_callback, app_fsm);
    variable_item_set_current_value_index(item, app_fsm->subghz_signal_mode);
    switch(app_fsm->subghz_signal_mode) {
    case SubGhzSignalModeOok:
        variable_item_set_current_value_text(item, "OOK");
        break;
    case SubGhzSignalModeFsk:
        variable_item_set_current_value_text(item, "FSK");
        break;
    case SubGhzSignalModeDisabled:
    default:
        variable_item_set_current_value_text(item, "disabled");
        break;
    }
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewSubGhzSettings,
        variable_item_list_get_view(app_fsm->subghz_settings));

    app_fsm->debug_settings = variable_item_list_alloc();
    item = variable_item_list_add(
        app_fsm->debug_settings, "Sound", 2, debug_sound_change_callback, app_fsm);
    variable_item_set_current_value_index(item, app_fsm->sound_enabled ? 0 : 1);
    variable_item_set_current_value_text(item, app_fsm->sound_enabled ? "Yes" : "No");
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewDebugSettings,
        variable_item_list_get_view(app_fsm->debug_settings));

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
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewDebugSettings);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewSubGhzSettings);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewLfSettings);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewTx);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewMenu);
    view_dispatcher_remove_view(app_fsm->view_dispatcher, Dcf77ViewStartup);
    widget_free(app_fsm->egg_widget);
    widget_free(app_fsm->about_widget);
    view_free(app_fsm->tx_view);
    variable_item_list_free(app_fsm->debug_settings);
    variable_item_list_free(app_fsm->subghz_settings);
    variable_item_list_free(app_fsm->lf_settings);
    submenu_free(app_fsm->submenu);
    widget_free(app_fsm->startup_widget);
    view_dispatcher_free(app_fsm->view_dispatcher);
    dcf77_output_gpio_deinit();
    dcf77_hw_indicator_set(false);
    app_sound_set(app_fsm, false);
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
