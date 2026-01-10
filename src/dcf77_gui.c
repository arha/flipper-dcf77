#include "dcf77_gui.h"

#include "dcf77_logic.h"
#include "dcf77_scenes.h"

static const char about_text[] =
    "Designed by arha, to debug my radioclocks that never seem to work just right.\n"
    "https://github.com/arha\n";

static const char egg_text[] = "wow you found an easter egg\n";

static void dcf77_tx_render_callback(Canvas* const canvas, void* model) {
    Dcf77TxViewModel* tx_model = model;
    AppFSM* app_fsm = tx_model->app_fsm;
    char buffer[64];

    if(app_fsm->current_signal == RadioClockSignalTest) {
        canvas_draw_frame(canvas, 0, 0, 128, 64);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "Test");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            buffer, sizeof(buffer), "Pulse 0.1s every 0.5s");
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, buffer);
        snprintf(buffer, sizeof(buffer), "LF %lu Hz", (unsigned long)app_fsm->lf_freq);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, buffer);
        snprintf(
            buffer,
            sizeof(buffer),
            "TX %s",
            app_fsm->lf_transmit_enabled ? "enabled" : "disabled");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, buffer);
        return;
    }

    if(app_fsm->current_signal == RadioClockSignalWwvb) {
        canvas_draw_frame(canvas, 0, 0, 128, 64);
        canvas_set_font(canvas, FontPrimary);
        snprintf(
            buffer,
            sizeof(buffer),
            "%02d:%02d doy %03u",
            app_fsm->tx_hour,
            app_fsm->tx_minute,
            app_fsm->tx_day_of_year);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, buffer);
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            buffer,
            sizeof(buffer),
            "sec %02u pulse %s",
            app_fsm->bit_number,
            dcf77_logic_get_pulse_label((RadioClockPulse)app_fsm->bit_value));
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignBottom, buffer);
        snprintf(buffer, sizeof(buffer), "LF %lu Hz", (unsigned long)app_fsm->lf_freq);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, buffer);
        snprintf(
            buffer,
            sizeof(buffer),
            "SubGHz %s",
            app_fsm->subghz_signal_mode == SubGhzSignalModeDisabled ? "off" : "on");
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, buffer);
        return;
    }

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

void dcf77_gui_init(AppFSM* app_fsm) {
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
    submenu_add_item(app_fsm->submenu, "Start", Dcf77MenuItemStart, dcf77_menu_callback, app_fsm);
    submenu_add_item(
        app_fsm->submenu, "Signal & LF", Dcf77MenuItemLfSettings, dcf77_menu_callback, app_fsm);
    submenu_add_item(
        app_fsm->submenu, "SubGHz", Dcf77MenuItemSubGhzSettings, dcf77_menu_callback, app_fsm);
    submenu_add_item(
        app_fsm->submenu, "Debug", Dcf77MenuItemDebugSettings, dcf77_menu_callback, app_fsm);
    submenu_add_item(app_fsm->submenu, "About", Dcf77MenuItemAbout, dcf77_menu_callback, app_fsm);
    view_dispatcher_add_view(app_fsm->view_dispatcher, Dcf77ViewMenu, submenu_get_view(app_fsm->submenu));

    app_fsm->lf_settings = variable_item_list_alloc();
    app_fsm->lf_signal_item = variable_item_list_add(
        app_fsm->lf_settings,
        "Signal",
        radio_clock_visible_signal_count(),
        NULL,
        app_fsm);
    variable_item_set_current_value_index(
        app_fsm->lf_signal_item, radio_clock_visible_signal_index(app_fsm->current_signal));
    variable_item_set_current_value_text(
        app_fsm->lf_signal_item, radio_clock_signal_get_label(app_fsm->current_signal));

    app_fsm->lf_tx_enabled_item = variable_item_list_add(
        app_fsm->lf_settings, "LF TX enabled", 2, dcf77_lf_transmit_change_callback, app_fsm);
    variable_item_set_current_value_index(app_fsm->lf_tx_enabled_item, app_fsm->lf_transmit_enabled ? 1 : 0);
    variable_item_set_current_value_text(
        app_fsm->lf_tx_enabled_item, app_fsm->lf_transmit_enabled ? "Yes" : "No");

    app_fsm->lf_freq_item = variable_item_list_add(
        app_fsm->lf_settings,
        "kHz",
        ((LF_FREQ_MAX - LF_FREQ_MIN) / LF_FREQ_STEP) + 1,
        dcf77_lf_frequency_change_callback,
        app_fsm);
    variable_item_set_current_value_index(
        app_fsm->lf_freq_item, dcf77_app_get_lf_freq_index(app_fsm->lf_freq));
    variable_item_set_current_value_text(app_fsm->lf_freq_item, app_fsm->lf_freq_text);

    app_fsm->lf_default_freq_item = variable_item_list_add(
        app_fsm->lf_settings, "Default frequency", 1, NULL, app_fsm);
    variable_item_set_current_value_text(app_fsm->lf_default_freq_item, "");

    variable_item_list_set_enter_callback(
        app_fsm->lf_settings, dcf77_lf_settings_enter_callback, app_fsm);
    view_set_context(variable_item_list_get_view(app_fsm->lf_settings), app_fsm);
    view_set_input_callback(
        variable_item_list_get_view(app_fsm->lf_settings), dcf77_lf_settings_input_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher,
        Dcf77ViewLfSettings,
        variable_item_list_get_view(app_fsm->lf_settings));

    app_fsm->subghz_settings = variable_item_list_alloc();
    VariableItem* item = variable_item_list_add(
        app_fsm->subghz_settings, "signal", 4, dcf77_subghz_signal_change_callback, app_fsm);
    variable_item_set_current_value_index(item, app_fsm->subghz_signal_mode);
    switch(app_fsm->subghz_signal_mode) {
    case SubGhzSignalModeOok:
        variable_item_set_current_value_text(item, "OOK");
        break;
    case SubGhzSignalModeFsk:
        variable_item_set_current_value_text(item, "FSK");
        break;
    case SubGhzSignalModeFskFull:
        variable_item_set_current_value_text(item, "100%");
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
        app_fsm->debug_settings, "Sound", 2, dcf77_debug_sound_change_callback, app_fsm);
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
    view_set_draw_callback(app_fsm->tx_view, dcf77_tx_render_callback);
    view_set_input_callback(app_fsm->tx_view, dcf77_tx_input_callback);
    view_set_previous_callback(app_fsm->tx_view, dcf77_tx_previous_callback);
    view_dispatcher_add_view(app_fsm->view_dispatcher, Dcf77ViewTx, app_fsm->tx_view);

    app_fsm->about_widget = widget_alloc();
    widget_add_frame_element(app_fsm->about_widget, 0, 0, 128, 64, 0);
    widget_add_text_scroll_element(app_fsm->about_widget, 4, 4, 120, 46, about_text);
    widget_add_button_element(
        app_fsm->about_widget, GuiButtonTypeLeft, "Back", dcf77_about_button_callback, app_fsm);
    view_set_context(widget_get_view(app_fsm->about_widget), app_fsm);
    view_set_input_callback(widget_get_view(app_fsm->about_widget), dcf77_about_input_callback);
    view_set_previous_callback(widget_get_view(app_fsm->about_widget), dcf77_text_previous_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewAbout, widget_get_view(app_fsm->about_widget));

    app_fsm->egg_widget = widget_alloc();
    widget_add_frame_element(app_fsm->egg_widget, 0, 0, 128, 64, 0);
    widget_add_text_scroll_element(app_fsm->egg_widget, 4, 4, 120, 46, egg_text);
    widget_add_button_element(
        app_fsm->egg_widget, GuiButtonTypeLeft, "Back", dcf77_egg_button_callback, app_fsm);
    view_set_context(widget_get_view(app_fsm->egg_widget), app_fsm);
    view_set_input_callback(widget_get_view(app_fsm->egg_widget), dcf77_egg_input_callback);
    view_set_previous_callback(widget_get_view(app_fsm->egg_widget), dcf77_text_previous_callback);
    view_dispatcher_add_view(
        app_fsm->view_dispatcher, Dcf77ViewEgg, widget_get_view(app_fsm->egg_widget));
}

void dcf77_gui_deinit(AppFSM* app_fsm) {
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
}
