#include "dcf77_scenes.h"

#include <datetime/datetime.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "dcf77_gui.h"
#include "dcf77_hw.h"
#include "dcf77_logic.h"

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

void dcf77_app_switch_to_menu(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenMenu;
    submenu_set_selected_item(app_fsm->submenu, Dcf77MenuItemStart);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewMenu);
}

void dcf77_app_switch_to_about(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenAbout;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewAbout);
}

void dcf77_app_switch_to_egg(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenEgg;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewEgg);
}

void dcf77_app_switch_to_lf_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenLfSettings;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewLfSettings);
}

void dcf77_app_switch_to_subghz_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenSubGhzSettings;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewSubGhzSettings);
}

void dcf77_app_switch_to_debug_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenDebugSettings;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewDebugSettings);
}

void dcf77_app_start_tx(AppFSM* app_fsm) {
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
    dcf77_app_apply_rf_settings(app_fsm);
    app_fsm->screen = AppScreenTx;
    app_fsm->last_tx_refresh_tick = furi_get_tick();
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewTx);
}

void dcf77_app_stop_tx(AppFSM* app_fsm) {
    if(!app_fsm->tx_active) {
        return;
    }

    dcf77_timing_stop(app_fsm);
    dcf77_lf_deinit(app_fsm);
    dcf77_subghz_deinit(app_fsm);
    dcf77_output_gpio_set(false);
    dcf77_hw_indicator_set(false);
    dcf77_app_sound_set(app_fsm, false);
    app_fsm->output_state = false;
    app_fsm->output_dirty = false;
    app_fsm->tx_active = false;
    dcf77_app_switch_to_menu(app_fsm);
}

uint32_t dcf77_tx_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    notification_message_block(app_fsm->notification, &seq_c_minor);
    dcf77_app_stop_tx(app_fsm);
    return Dcf77ViewMenu;
}

uint32_t dcf77_text_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    if(app_fsm->screen == AppScreenEgg) {
        dcf77_app_switch_to_about(app_fsm);
        return Dcf77ViewAbout;
    }

    dcf77_app_switch_to_menu(app_fsm);
    return Dcf77ViewMenu;
}

void dcf77_lf_transmit_change_callback(VariableItem* item) {
    AppFSM* app_fsm = variable_item_get_context(item);
    const uint8_t value_index = variable_item_get_current_value_index(item);

    app_fsm->lf_transmit_enabled = value_index == Dcf77LfSettingTransmit;
    variable_item_set_current_value_text(item, app_fsm->lf_transmit_enabled ? "Yes" : "No");
    dcf77_app_apply_rf_settings(app_fsm);
    dcf77_app_settings_save(app_fsm);
}

void dcf77_lf_frequency_change_callback(VariableItem* item) {
    AppFSM* app_fsm = variable_item_get_context(item);
    const uint8_t value_index = variable_item_get_current_value_index(item);

    app_fsm->lf_freq = LF_FREQ_MIN + ((uint32_t)value_index * LF_FREQ_STEP);
    dcf77_app_update_lf_freq_text(app_fsm);
    variable_item_set_current_value_text(item, app_fsm->lf_freq_text);
    dcf77_app_apply_rf_settings(app_fsm);
    dcf77_app_settings_save(app_fsm);
}

void dcf77_subghz_signal_change_callback(VariableItem* item) {
    static const char* const labels[] = {"disabled", "OOK", "FSK", "FSK 100%"};
    AppFSM* app_fsm = variable_item_get_context(item);
    const uint8_t value_index = variable_item_get_current_value_index(item);

    app_fsm->subghz_signal_mode = (SubGhzSignalMode)value_index;
    variable_item_set_current_value_text(item, labels[value_index]);
    dcf77_app_apply_rf_settings(app_fsm);
    dcf77_app_settings_save(app_fsm);
}

void dcf77_debug_sound_change_callback(VariableItem* item) {
    AppFSM* app_fsm = variable_item_get_context(item);
    const uint8_t value_index = variable_item_get_current_value_index(item);

    app_fsm->sound_enabled = value_index == 0;
    variable_item_set_current_value_text(item, app_fsm->sound_enabled ? "Yes" : "No");
    if(!app_fsm->sound_enabled) {
        dcf77_app_sound_set(app_fsm, false);
    }
    dcf77_app_settings_save(app_fsm);
}

void dcf77_about_button_callback(GuiButtonType result, InputType type, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(type != InputTypePress) {
        return;
    }

    if(result == GuiButtonTypeLeft) {
        dcf77_app_switch_to_menu(app_fsm);
    }
}

void dcf77_egg_button_callback(GuiButtonType result, InputType type, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(type != InputTypePress) {
        return;
    }

    if(result == GuiButtonTypeLeft) {
        dcf77_app_switch_to_about(app_fsm);
    }
}

bool dcf77_about_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(event->type != InputTypePress) {
        return false;
    }

    switch(event->key) {
    case InputKeyLeft:
    case InputKeyBack:
        dcf77_app_switch_to_menu(app_fsm);
        return true;
    case InputKeyRight:
        dcf77_app_switch_to_egg(app_fsm);
        return true;
    default:
        return false;
    }
}

bool dcf77_egg_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(event->type == InputTypePress &&
       (event->key == InputKeyLeft || event->key == InputKeyBack)) {
        dcf77_app_switch_to_about(app_fsm);
        return true;
    }

    return false;
}

bool dcf77_tx_input_callback(InputEvent* event, void* ctx) {
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
        dcf77_app_stop_tx(app_fsm);
        return true;
    default:
        return false;
    }
}

void dcf77_menu_callback(void* ctx, uint32_t index) {
    AppFSM* app_fsm = ctx;

    switch(index) {
    case Dcf77MenuItemStart:
        dcf77_app_start_tx(app_fsm);
        break;
    case Dcf77MenuItemLfSettings:
        dcf77_app_switch_to_lf_settings(app_fsm);
        break;
    case Dcf77MenuItemSubGhzSettings:
        dcf77_app_switch_to_subghz_settings(app_fsm);
        break;
    case Dcf77MenuItemDebugSettings:
        dcf77_app_switch_to_debug_settings(app_fsm);
        break;
    case Dcf77MenuItemAbout:
        dcf77_app_switch_to_about(app_fsm);
        break;
    default:
        break;
    }
}

bool dcf77_navigation_callback(void* ctx) {
    AppFSM* app_fsm = ctx;

    if(app_fsm->screen == AppScreenTx) {
        dcf77_app_stop_tx(app_fsm);
        return true;
    }

    if(app_fsm->screen == AppScreenLfSettings ||
       app_fsm->screen == AppScreenSubGhzSettings ||
       app_fsm->screen == AppScreenDebugSettings) {
        dcf77_app_switch_to_menu(app_fsm);
        return true;
    }

    view_dispatcher_stop(app_fsm->view_dispatcher);
    return true;
}

void dcf77_tick_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    const uint32_t now = furi_get_tick();

    if(app_fsm->screen == AppScreenStartup && now >= app_fsm->startup_deadline_tick) {
        dcf77_app_switch_to_menu(app_fsm);
    }

    if(app_fsm->tx_active && app_fsm->output_dirty) {
        const bool output = app_fsm->output_state;
        app_fsm->output_dirty = false;
        dcf77_hw_indicator_set(output);
        dcf77_app_sound_set(app_fsm, app_fsm->sound_enabled && output);
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
