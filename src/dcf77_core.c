#include <applications/services/storage/storage.h>
#include <dolphin/dolphin.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <toolbox/saved_struct.h>

#include "dcf77_app.h"
#include "dcf77_gui.h"
#include "dcf77_hw.h"
#include "dcf77_logic.h"
#include "dcf77_main.h"
#include "dcf77_scenes.h"

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

typedef struct {
    uint32_t lf_freq;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_signal_mode;
    uint8_t sound_enabled;
} Dcf77SavedSettingsV3;

typedef struct {
    uint8_t current_signal;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_signal_mode;
    uint8_t sound_enabled;
    uint32_t signal_freqs[RadioClockSignalCount];
} Dcf77SavedSettings;

#define DCF77_SETTINGS_DIR EXT_PATH("apps_data/dcf77")
#define DCF77_SETTINGS_PATH EXT_PATH("apps_data/dcf77/settings.bin")
#define DCF77_SETTINGS_MAGIC 0x44
#define DCF77_SETTINGS_VERSION 4

static void dcf77_app_init_signal_defaults(AppFSM* app_fsm) {
    for(size_t i = 0; i < RadioClockSignalCount; i++) {
        app_fsm->signal_freqs[i] = radio_clock_signal_get_default_freq((RadioClockSignal)i);
    }
}

void dcf77_app_sound_set(AppFSM* app_fsm, bool enabled) {
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

bool dcf77_app_settings_save(const AppFSM* app_fsm) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool dir_ready =
        storage_common_exists(storage, DCF77_SETTINGS_DIR) ||
        (storage_simply_mkdir(storage, DCF77_SETTINGS_DIR) == true);
    furi_record_close(RECORD_STORAGE);

    if(!dir_ready) {
        return false;
    }

    const Dcf77SavedSettings settings = {
        .current_signal = app_fsm->current_signal,
        .lf_transmit_enabled = app_fsm->lf_transmit_enabled ? 1 : 0,
        .subghz_signal_mode = app_fsm->subghz_signal_mode,
        .sound_enabled = app_fsm->sound_enabled ? 1 : 0,
        .signal_freqs =
            {
                app_fsm->signal_freqs[0],
                app_fsm->signal_freqs[1],
                app_fsm->signal_freqs[2],
                app_fsm->signal_freqs[3],
                app_fsm->signal_freqs[4],
                app_fsm->signal_freqs[5],
                app_fsm->signal_freqs[6],
            },
    };

    return saved_struct_save(
        DCF77_SETTINGS_PATH,
        &settings,
        sizeof(settings),
        DCF77_SETTINGS_MAGIC,
        DCF77_SETTINGS_VERSION);
}

void dcf77_app_set_signal_frequency(AppFSM* app_fsm, uint32_t freq) {
    if(freq < LF_FREQ_MIN) {
        freq = LF_FREQ_MIN;
    } else if(freq > LF_FREQ_MAX) {
        freq = LF_FREQ_MAX;
    }

    const uint32_t offset = freq - LF_FREQ_MIN;
    freq = LF_FREQ_MIN + ((offset / LF_FREQ_STEP) * LF_FREQ_STEP);

    app_fsm->lf_freq = freq;
    app_fsm->signal_freqs[app_fsm->current_signal] = freq;
    dcf77_app_update_lf_freq_text(app_fsm);
}

void dcf77_app_set_signal(AppFSM* app_fsm, RadioClockSignal signal) {
    if(signal >= RadioClockSignalCount) {
        signal = RadioClockSignalDcf77;
    }

    app_fsm->current_signal = signal;
    dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[signal]);
}

void dcf77_app_restore_default_frequency(AppFSM* app_fsm) {
    dcf77_app_set_signal_frequency(
        app_fsm, radio_clock_signal_get_default_freq(app_fsm->current_signal));
}

bool dcf77_app_signal_can_run(const AppFSM* app_fsm) {
    return radio_clock_signal_can_run(app_fsm->current_signal);
}

static void dcf77_app_settings_load(AppFSM* app_fsm) {
    Dcf77SavedSettings settings = {
        .current_signal = RadioClockSignalDcf77,
        .lf_transmit_enabled = 1,
        .subghz_signal_mode = SubGhzSignalModeDisabled,
        .sound_enabled = 0,
    };
    Dcf77SavedSettingsV3 legacy_settings = {
        .lf_freq = LF_FREQ_DEFAULT,
        .lf_transmit_enabled = 1,
        .subghz_signal_mode = SubGhzSignalModeDisabled,
        .sound_enabled = 0,
    };

    dcf77_app_init_signal_defaults(app_fsm);

    if(saved_struct_load(
           DCF77_SETTINGS_PATH,
           &settings,
           sizeof(settings),
           DCF77_SETTINGS_MAGIC,
           DCF77_SETTINGS_VERSION)) {
        for(size_t i = 0; i < RadioClockSignalCount; i++) {
            app_fsm->signal_freqs[i] = settings.signal_freqs[i];
        }
        if(settings.current_signal >= RadioClockSignalCount) {
            settings.current_signal = RadioClockSignalDcf77;
        }
        app_fsm->current_signal = (RadioClockSignal)settings.current_signal;
        if(radio_clock_visible_signal_index(app_fsm->current_signal) >=
           radio_clock_visible_signal_count()) {
            app_fsm->current_signal = RadioClockSignalDcf77;
        }
        app_fsm->lf_transmit_enabled = settings.lf_transmit_enabled != 0;
        if(settings.subghz_signal_mode > SubGhzSignalModeFskFull) {
            settings.subghz_signal_mode = SubGhzSignalModeDisabled;
        }
        app_fsm->subghz_signal_mode = settings.subghz_signal_mode;
        app_fsm->sound_enabled = settings.sound_enabled != 0;
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[app_fsm->current_signal]);
    } else if(saved_struct_load(
                  DCF77_SETTINGS_PATH,
                  &legacy_settings,
                  sizeof(legacy_settings),
                  DCF77_SETTINGS_MAGIC,
                  3)) {
        app_fsm->current_signal = RadioClockSignalDcf77;
        app_fsm->signal_freqs[RadioClockSignalDcf77] = legacy_settings.lf_freq;
        app_fsm->signal_freqs[RadioClockSignalTest] = legacy_settings.lf_freq;
        app_fsm->lf_transmit_enabled = legacy_settings.lf_transmit_enabled != 0;
        if(legacy_settings.subghz_signal_mode > SubGhzSignalModeFskFull) {
            legacy_settings.subghz_signal_mode = SubGhzSignalModeDisabled;
        }
        app_fsm->subghz_signal_mode = legacy_settings.subghz_signal_mode;
        app_fsm->sound_enabled = legacy_settings.sound_enabled != 0;
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[RadioClockSignalDcf77]);
    } else {
        app_fsm->current_signal = RadioClockSignalDcf77;
        app_fsm->lf_transmit_enabled = true;
        app_fsm->subghz_signal_mode = SubGhzSignalModeDisabled;
        app_fsm->sound_enabled = false;
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[RadioClockSignalDcf77]);
    }
}

void dcf77_app_apply_rf_settings(AppFSM* app_fsm) {
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

uint8_t dcf77_app_get_lf_freq_index(uint32_t freq) {
    if(freq <= LF_FREQ_MIN) {
        return 0;
    }

    if(freq >= LF_FREQ_MAX) {
        return (LF_FREQ_MAX - LF_FREQ_MIN) / LF_FREQ_STEP;
    }

    return (freq - LF_FREQ_MIN) / LF_FREQ_STEP;
}

void dcf77_app_update_lf_freq_text(AppFSM* app_fsm) {
    snprintf(
        app_fsm->lf_freq_text,
        sizeof(app_fsm->lf_freq_text),
        "%lu",
        (unsigned long)app_fsm->lf_freq);
}

static void dcf77_app_init(AppFSM* app_fsm) {
    memset(app_fsm, 0, sizeof(*app_fsm));

    dcf77_app_settings_load(app_fsm);
    app_fsm->screen = AppScreenStartup;
    app_fsm->startup_deadline_tick =
        furi_get_tick() + ((STARTUP_SCREEN_MS * furi_kernel_get_tick_frequency()) / 1000U);

    dcf77_logic_init(app_fsm);
    dcf77_output_gpio_init();

    app_fsm->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app_fsm->view_dispatcher, app_fsm);
    view_dispatcher_set_navigation_event_callback(
        app_fsm->view_dispatcher, dcf77_navigation_callback);
    view_dispatcher_set_tick_event_callback(app_fsm->view_dispatcher, dcf77_tick_callback, 1);

    dcf77_gui_init(app_fsm);
}

static void dcf77_app_deinit(AppFSM* app_fsm) {
    dcf77_app_stop_tx(app_fsm);
    dcf77_gui_deinit(app_fsm);
    dcf77_output_gpio_deinit();
    dcf77_hw_indicator_set(false);
    dcf77_app_sound_set(app_fsm, false);
}

int32_t dcf77_app_run(void* p) {
    UNUSED(p);

    AppFSM* app_fsm = malloc(sizeof(AppFSM));
    dcf77_app_init(app_fsm);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app_fsm->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    app_fsm->notification = notification;
    notification_message_block(notification, &sequence_display_backlight_enforce_on);

    dolphin_deed(DolphinDeedPluginGameStart);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewStartup);
    view_dispatcher_run(app_fsm->view_dispatcher);
    dcf77_app_deinit(app_fsm);
    notification_message_block(notification, &sequence_display_backlight_enforce_auto);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app_fsm);

    return 0;
}
