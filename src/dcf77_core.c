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

#define DCF77_SAVED_SIGNAL_COUNT_V7 7U

typedef struct {
    uint8_t current_signal;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_signal_mode;
    uint8_t sound_enabled;
    uint32_t signal_freqs[DCF77_SAVED_SIGNAL_COUNT_V7];
} Dcf77SavedSettingsV4;

typedef struct {
    uint32_t lf_freq;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_signal_mode;
    uint8_t sound_enabled;
} Dcf77SavedSettingsV3;

typedef struct {
    uint8_t current_signal;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_transmit_enabled;
    uint8_t subghz_fsk_tone_index;
    uint8_t speaker_value_index;
    uint8_t subghz_band_count;
    uint16_t reserved;
    uint32_t signal_freqs[DCF77_SAVED_SIGNAL_COUNT_V7];
    uint32_t subghz_selected_band_start;
    uint32_t subghz_band_starts[DCF77_SUBGHZ_MAX_BANDS];
    uint32_t subghz_band_freqs[DCF77_SUBGHZ_MAX_BANDS];
} Dcf77SavedSettingsV5;

typedef struct {
    uint8_t current_signal;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_signal_mode;
    uint8_t subghz_fsk_tone_index;
    uint8_t speaker_value_index;
    uint8_t subghz_band_count;
    uint16_t reserved;
    uint32_t signal_freqs[DCF77_SAVED_SIGNAL_COUNT_V7];
    uint32_t subghz_selected_band_start;
    uint32_t subghz_band_starts[DCF77_SUBGHZ_MAX_BANDS];
    uint32_t subghz_band_freqs[DCF77_SUBGHZ_MAX_BANDS];
} Dcf77SavedSettingsV6;

typedef struct {
    uint8_t current_signal;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_signal_mode;
    uint8_t subghz_fsk_tone_index;
    uint8_t speaker_value_index;
    uint8_t subghz_band_count;
    uint8_t gpio_baseband_pin_number;
    uint8_t gpio_rf_pin_number;
    uint8_t gpio_rf_duty_cycle;
    uint8_t led_color_index;
    uint8_t screen_mode;
    uint8_t reserved0;
    uint32_t signal_freqs[DCF77_SAVED_SIGNAL_COUNT_V7];
    uint32_t subghz_selected_band_start;
    uint32_t subghz_band_starts[DCF77_SUBGHZ_MAX_BANDS];
    uint32_t subghz_band_freqs[DCF77_SUBGHZ_MAX_BANDS];
} Dcf77SavedSettingsV7;

typedef struct {
    uint8_t current_signal;
    uint8_t lf_transmit_enabled;
    uint8_t subghz_signal_mode;
    uint8_t subghz_fsk_tone_index;
    uint8_t speaker_value_index;
    uint8_t subghz_band_count;
    uint8_t gpio_baseband_pin_number;
    uint8_t gpio_rf_pin_number;
    uint8_t gpio_rf_duty_cycle;
    uint8_t led_color_index;
    uint8_t screen_mode;
    uint8_t reserved0;
    uint32_t signal_freqs[RadioClockSignalCount];
    uint32_t subghz_selected_band_start;
    uint32_t subghz_band_starts[DCF77_SUBGHZ_MAX_BANDS];
    uint32_t subghz_band_freqs[DCF77_SUBGHZ_MAX_BANDS];
} Dcf77SavedSettings;

#define DCF77_SETTINGS_DIR EXT_PATH("apps_data/dcf77")
#define DCF77_SETTINGS_PATH EXT_PATH("apps_data/dcf77/settings.bin")
#define DCF77_SETTINGS_MAGIC 0x44
#define DCF77_SETTINGS_VERSION 8
#define DCF77_SUBGHZ_LEGACY_FREQ 433670000U
#define DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX 12U
#define DCF77_SUBGHZ_DEFAULT_SPEAKER_VALUE_INDEX 25U

static void dcf77_app_init_signal_defaults(AppFSM* app_fsm) {
    for(size_t i = 0; i < RadioClockSignalCount; i++) {
        app_fsm->signal_freqs[i] = radio_clock_signal_get_default_freq((RadioClockSignal)i);
    }
}

static void dcf77_app_restore_saved_signal_freqs(
    AppFSM* app_fsm,
    const uint32_t* saved_signal_freqs,
    size_t saved_count) {
    dcf77_app_init_signal_defaults(app_fsm);

    for(size_t i = 0; i < saved_count && i < RadioClockSignalCount; i++) {
        app_fsm->signal_freqs[i] = saved_signal_freqs[i];
    }
}

static Dcf77SubGhzBand dcf77_app_get_subghz_band(const AppFSM* app_fsm, uint8_t band_index) {
    Dcf77SubGhzBand band = {
        .start_hz = app_fsm->subghz_band_starts[band_index],
        .end_hz = app_fsm->subghz_band_ends[band_index],
    };
    return band;
}

static int32_t dcf77_app_find_subghz_band_by_start(const AppFSM* app_fsm, uint32_t band_start_hz) {
    for(uint8_t i = 0; i < app_fsm->subghz_band_count; i++) {
        if(app_fsm->subghz_band_starts[i] == band_start_hz) {
            return (int32_t)i;
        }
    }

    return -1;
}

static int32_t dcf77_app_find_subghz_band_for_frequency(const AppFSM* app_fsm, uint32_t freq_hz) {
    for(uint8_t i = 0; i < app_fsm->subghz_band_count; i++) {
        if(freq_hz >= app_fsm->subghz_band_starts[i] && freq_hz <= app_fsm->subghz_band_ends[i]) {
            return (int32_t)i;
        }
    }

    return -1;
}

static void dcf77_app_seed_subghz_defaults(AppFSM* app_fsm) {
    const int32_t legacy_band = dcf77_app_find_subghz_band_for_frequency(app_fsm, DCF77_SUBGHZ_LEGACY_FREQ);

    app_fsm->subghz_signal_mode = SubGhzSignalModeDisabled;
    app_fsm->subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
    app_fsm->speaker_value_index = 0;

    for(uint8_t i = 0; i < app_fsm->subghz_band_count; i++) {
        app_fsm->subghz_band_freqs[i] = app_fsm->subghz_band_starts[i];
    }

    if(legacy_band >= 0) {
        app_fsm->subghz_band_index = (uint8_t)legacy_band;
        app_fsm->subghz_band_freqs[legacy_band] = DCF77_SUBGHZ_LEGACY_FREQ;
    } else {
        app_fsm->subghz_band_index = 0;
    }
}

static void dcf77_app_init_subghz_bands(AppFSM* app_fsm) {
    const FuriHalRegion* region = furi_hal_region_get();
    uint8_t band_count = 0;

    if(region != NULL) {
        band_count = region->bands_count;
        if(band_count > DCF77_SUBGHZ_MAX_BANDS) {
            band_count = DCF77_SUBGHZ_MAX_BANDS;
        }

        for(uint8_t i = 0; i < band_count; i++) {
            app_fsm->subghz_band_starts[i] = region->bands[i].start;
            app_fsm->subghz_band_ends[i] = region->bands[i].end;
        }
    }

    if(band_count == 0) {
        band_count = 1;
        app_fsm->subghz_band_starts[0] = DCF77_SUBGHZ_LEGACY_FREQ;
        app_fsm->subghz_band_ends[0] = DCF77_SUBGHZ_LEGACY_FREQ;
    }

    app_fsm->subghz_band_count = band_count;
    dcf77_app_seed_subghz_defaults(app_fsm);
}

uint32_t dcf77_app_get_subghz_frequency(const AppFSM* app_fsm) {
    if(app_fsm->subghz_band_count == 0) {
        return DCF77_SUBGHZ_LEGACY_FREQ;
    }

    return app_fsm->subghz_band_freqs[app_fsm->subghz_band_index];
}

void dcf77_app_update_subghz_texts(AppFSM* app_fsm) {
    dcf77_subghz_format_note_text(
        app_fsm->subghz_tone_text,
        sizeof(app_fsm->subghz_tone_text),
        app_fsm->subghz_fsk_tone_index);
    dcf77_subghz_format_band_text(
        app_fsm->subghz_band_text,
        sizeof(app_fsm->subghz_band_text),
        app_fsm->subghz_band_starts[app_fsm->subghz_band_index]);
    dcf77_subghz_format_frequency_text(
        app_fsm->subghz_frequency_text,
        sizeof(app_fsm->subghz_frequency_text),
        dcf77_app_get_subghz_frequency(app_fsm));
    snprintf(
        app_fsm->subghz_frequency_step_text,
        sizeof(app_fsm->subghz_frequency_step_text),
        "< %s >",
        app_fsm->subghz_frequency_text);
    dcf77_subghz_format_frequency_khz_text(
        app_fsm->subghz_manual_text,
        sizeof(app_fsm->subghz_manual_text),
        dcf77_app_get_subghz_frequency(app_fsm));
}

static void dcf77_app_validate_gpio_baseband_pin(AppFSM* app_fsm) {
    if(!dcf77_debug_gpio_baseband_pin_valid(app_fsm->gpio_baseband_pin_number) ||
       (dcf77_app_gpio_rf_enabled(app_fsm) &&
        app_fsm->gpio_baseband_pin_number == app_fsm->gpio_rf_pin_number)) {
        app_fsm->gpio_baseband_pin_number = dcf77_debug_gpio_baseband_default_pin();
    }
}

static void dcf77_app_validate_gpio_rf_pin(AppFSM* app_fsm) {
    if(!dcf77_debug_gpio_rf_pin_valid(app_fsm->gpio_rf_pin_number)) {
        app_fsm->gpio_rf_pin_number = dcf77_debug_gpio_rf_default_pin();
    }
}

void dcf77_app_update_debug_texts(AppFSM* app_fsm) {
    dcf77_app_validate_gpio_rf_pin(app_fsm);
    dcf77_app_validate_gpio_baseband_pin(app_fsm);
    app_fsm->gpio_rf_duty_cycle = dcf77_debug_gpio_rf_duty_normalize(app_fsm->gpio_rf_duty_cycle);

    if(app_fsm->led_color_index >= dcf77_debug_led_color_count()) {
        app_fsm->led_color_index = Dcf77LedColorRed;
    }

    if(app_fsm->screen_mode >= dcf77_debug_screen_mode_count()) {
        app_fsm->screen_mode = Dcf77ScreenModeDebug;
    }

    dcf77_debug_format_pin_text(
        app_fsm->gpio_baseband_text,
        sizeof(app_fsm->gpio_baseband_text),
        app_fsm->gpio_baseband_pin_number,
        true);
    dcf77_debug_format_pin_text(
        app_fsm->gpio_rf_text,
        sizeof(app_fsm->gpio_rf_text),
        app_fsm->gpio_rf_pin_number,
        true);
    dcf77_debug_format_gpio_rf_duty_text(
        app_fsm->gpio_rf_duty_text,
        sizeof(app_fsm->gpio_rf_duty_text),
        app_fsm->gpio_rf_duty_cycle);
    snprintf(
        app_fsm->led_text,
        sizeof(app_fsm->led_text),
        "%s",
        dcf77_debug_led_color_label(dcf77_debug_led_color_index(app_fsm->led_color_index)));
    snprintf(
        app_fsm->screen_text,
        sizeof(app_fsm->screen_text),
        "%s",
        dcf77_debug_screen_mode_label(app_fsm->screen_mode));

    if(app_fsm->speaker_value_index == 0) {
        snprintf(app_fsm->speaker_text, sizeof(app_fsm->speaker_text), "No");
    } else {
        dcf77_subghz_format_note_text(
            app_fsm->speaker_text,
            sizeof(app_fsm->speaker_text),
            app_fsm->speaker_value_index - 1U);
    }
}

void dcf77_app_set_gpio_baseband_pin(AppFSM* app_fsm, uint8_t pin_number) {
    app_fsm->gpio_baseband_pin_number = pin_number;
    dcf77_app_update_debug_texts(app_fsm);
}

void dcf77_app_set_gpio_rf_pin(AppFSM* app_fsm, uint8_t pin_number) {
    app_fsm->gpio_rf_pin_number = pin_number;

    if(app_fsm->gpio_rf_pin_number != dcf77_debug_gpio_rf_default_pin() &&
       app_fsm->gpio_rf_pin_number == app_fsm->gpio_baseband_pin_number) {
        app_fsm->gpio_baseband_pin_number = dcf77_debug_gpio_baseband_default_pin();
    }

    dcf77_app_update_debug_texts(app_fsm);
}

void dcf77_app_set_gpio_rf_duty_cycle(AppFSM* app_fsm, uint8_t duty_cycle) {
    app_fsm->gpio_rf_duty_cycle = duty_cycle;
    dcf77_app_update_debug_texts(app_fsm);
}

void dcf77_app_set_led_color(AppFSM* app_fsm, Dcf77LedColor color) {
    app_fsm->led_color_index = color;
    dcf77_app_update_debug_texts(app_fsm);
}

void dcf77_app_set_screen_mode(AppFSM* app_fsm, Dcf77ScreenMode mode) {
    app_fsm->screen_mode = mode;
    dcf77_app_update_debug_texts(app_fsm);
}

bool dcf77_app_gpio_rf_enabled(const AppFSM* app_fsm) {
    return app_fsm->gpio_rf_pin_number != dcf77_debug_gpio_rf_default_pin();
}

void dcf77_app_sound_set(AppFSM* app_fsm, bool enabled) {
    const bool speaker_enabled = enabled && app_fsm->speaker_value_index != 0;

    if(!speaker_enabled) {
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

    furi_hal_speaker_start(
        dcf77_subghz_note_freq_hz(app_fsm->speaker_value_index - 1U),
        0.6f);
    app_fsm->speaker_active = true;
}

void dcf77_app_set_subghz_signal_mode(AppFSM* app_fsm, SubGhzSignalMode mode) {
    if(mode > SubGhzSignalModeFskFull) {
        mode = SubGhzSignalModeDisabled;
    }

    app_fsm->subghz_signal_mode = mode;
    dcf77_app_update_subghz_texts(app_fsm);
}

void dcf77_app_set_subghz_fsk_tone(AppFSM* app_fsm, uint8_t tone_index) {
    if(tone_index >= dcf77_subghz_note_count()) {
        tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
    }

    app_fsm->subghz_fsk_tone_index = tone_index;
    dcf77_app_update_subghz_texts(app_fsm);
}

void dcf77_app_set_subghz_band(AppFSM* app_fsm, uint8_t band_index) {
    Dcf77SubGhzBand band;

    if(app_fsm->subghz_band_count == 0) {
        return;
    }

    if(band_index >= app_fsm->subghz_band_count) {
        band_index = app_fsm->subghz_band_count - 1U;
    }

    app_fsm->subghz_band_index = band_index;
    band = dcf77_app_get_subghz_band(app_fsm, band_index);
    app_fsm->subghz_band_freqs[band_index] =
        dcf77_subghz_clamp_frequency(app_fsm->subghz_band_freqs[band_index], &band);
    dcf77_app_update_subghz_texts(app_fsm);
}

void dcf77_app_set_subghz_frequency(AppFSM* app_fsm, uint32_t freq_hz) {
    const Dcf77SubGhzBand band = dcf77_app_get_subghz_band(app_fsm, app_fsm->subghz_band_index);

    app_fsm->subghz_band_freqs[app_fsm->subghz_band_index] =
        dcf77_subghz_clamp_frequency(freq_hz, &band);
    dcf77_app_update_subghz_texts(app_fsm);
}

void dcf77_app_step_subghz_frequency(AppFSM* app_fsm, int8_t direction) {
    const Dcf77SubGhzBand band = dcf77_app_get_subghz_band(app_fsm, app_fsm->subghz_band_index);

    app_fsm->subghz_band_freqs[app_fsm->subghz_band_index] = dcf77_subghz_step_frequency(
        app_fsm->subghz_band_freqs[app_fsm->subghz_band_index], &band, direction);
    dcf77_app_update_subghz_texts(app_fsm);
}

void dcf77_app_set_speaker_value_index(AppFSM* app_fsm, uint8_t value_index) {
    if(value_index > dcf77_subghz_note_count()) {
        value_index = 0;
    }

    app_fsm->speaker_value_index = value_index;
    dcf77_app_update_debug_texts(app_fsm);
}

static void dcf77_app_restore_saved_subghz_frequencies(
    AppFSM* app_fsm,
    const uint32_t* saved_band_starts,
    const uint32_t* saved_band_freqs,
    uint8_t saved_band_count,
    uint32_t selected_band_start) {
    int32_t selected_band = dcf77_app_find_subghz_band_by_start(app_fsm, selected_band_start);

    for(uint8_t i = 0; i < saved_band_count && i < DCF77_SUBGHZ_MAX_BANDS; i++) {
        const int32_t band_index = dcf77_app_find_subghz_band_by_start(app_fsm, saved_band_starts[i]);

        if(band_index >= 0) {
            const Dcf77SubGhzBand band = dcf77_app_get_subghz_band(app_fsm, (uint8_t)band_index);
            app_fsm->subghz_band_freqs[band_index] =
                dcf77_subghz_clamp_frequency(saved_band_freqs[i], &band);
        }
    }

    if(selected_band < 0) {
        selected_band = dcf77_app_find_subghz_band_for_frequency(app_fsm, DCF77_SUBGHZ_LEGACY_FREQ);
    }

    if(selected_band < 0) {
        selected_band = 0;
    }

    dcf77_app_set_subghz_band(app_fsm, (uint8_t)selected_band);
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

    Dcf77SavedSettings settings = {
        .current_signal = app_fsm->current_signal,
        .lf_transmit_enabled = app_fsm->lf_transmit_enabled ? 1 : 0,
        .subghz_signal_mode = app_fsm->subghz_signal_mode,
        .subghz_fsk_tone_index = app_fsm->subghz_fsk_tone_index,
        .speaker_value_index = app_fsm->speaker_value_index,
        .subghz_band_count = app_fsm->subghz_band_count,
        .gpio_baseband_pin_number = app_fsm->gpio_baseband_pin_number,
        .gpio_rf_pin_number = app_fsm->gpio_rf_pin_number,
        .gpio_rf_duty_cycle = app_fsm->gpio_rf_duty_cycle,
        .led_color_index = app_fsm->led_color_index,
        .screen_mode = app_fsm->screen_mode,
        .subghz_selected_band_start = app_fsm->subghz_band_starts[app_fsm->subghz_band_index],
    };

    for(size_t i = 0; i < RadioClockSignalCount; i++) {
        settings.signal_freqs[i] = app_fsm->signal_freqs[i];
    }

    for(uint8_t i = 0; i < app_fsm->subghz_band_count && i < DCF77_SUBGHZ_MAX_BANDS; i++) {
        settings.subghz_band_starts[i] = app_fsm->subghz_band_starts[i];
        settings.subghz_band_freqs[i] = app_fsm->subghz_band_freqs[i];
    }

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
        .subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX,
        .speaker_value_index = 0,
        .subghz_band_count = 0,
        .gpio_baseband_pin_number = GPIO_BASEBAND_PIN_DEFAULT,
        .gpio_rf_pin_number = GPIO_RF_PIN_NONE,
        .gpio_rf_duty_cycle = GPIO_RF_DUTY_CYCLE_DEFAULT,
        .led_color_index = Dcf77LedColorRed,
        .screen_mode = Dcf77ScreenModeDebug,
    };
    Dcf77SavedSettingsV7 settings_v7 = {
        .current_signal = RadioClockSignalDcf77,
        .lf_transmit_enabled = 1,
        .subghz_signal_mode = SubGhzSignalModeDisabled,
        .subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX,
        .speaker_value_index = 0,
        .subghz_band_count = 0,
        .gpio_baseband_pin_number = GPIO_BASEBAND_PIN_DEFAULT,
        .gpio_rf_pin_number = GPIO_RF_PIN_NONE,
        .gpio_rf_duty_cycle = GPIO_RF_DUTY_CYCLE_DEFAULT,
        .led_color_index = Dcf77LedColorRed,
        .screen_mode = Dcf77ScreenModeDebug,
    };
    Dcf77SavedSettingsV6 settings_v6 = {
        .current_signal = RadioClockSignalDcf77,
        .lf_transmit_enabled = 1,
        .subghz_signal_mode = SubGhzSignalModeDisabled,
        .subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX,
        .speaker_value_index = 0,
        .subghz_band_count = 0,
    };
    Dcf77SavedSettingsV5 settings_v5 = {
        .current_signal = RadioClockSignalDcf77,
        .lf_transmit_enabled = 1,
        .subghz_transmit_enabled = 0,
        .subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX,
        .speaker_value_index = 0,
        .subghz_band_count = 0,
    };
    Dcf77SavedSettingsV4 settings_v4 = {
        .current_signal = RadioClockSignalDcf77,
        .lf_transmit_enabled = 1,
        .subghz_signal_mode = 0,
        .sound_enabled = 0,
    };
    Dcf77SavedSettingsV3 legacy_settings = {
        .lf_freq = LF_FREQ_DEFAULT,
        .lf_transmit_enabled = 1,
        .subghz_signal_mode = 0,
        .sound_enabled = 0,
    };

    dcf77_app_init_signal_defaults(app_fsm);
    dcf77_app_init_subghz_bands(app_fsm);
    app_fsm->gpio_baseband_pin_number = GPIO_BASEBAND_PIN_DEFAULT;
    app_fsm->gpio_rf_pin_number = GPIO_RF_PIN_NONE;
    app_fsm->gpio_rf_duty_cycle = GPIO_RF_DUTY_CYCLE_DEFAULT;
    app_fsm->led_color_index = Dcf77LedColorRed;
    app_fsm->screen_mode = Dcf77ScreenModeDebug;

    if(saved_struct_load(
           DCF77_SETTINGS_PATH,
           &settings,
           sizeof(settings),
           DCF77_SETTINGS_MAGIC,
           DCF77_SETTINGS_VERSION)) {
        dcf77_app_restore_saved_signal_freqs(
            app_fsm, settings.signal_freqs, RadioClockSignalCount);
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
        app_fsm->subghz_signal_mode = (SubGhzSignalMode)settings.subghz_signal_mode;
        if(settings.subghz_fsk_tone_index >= dcf77_subghz_note_count()) {
            settings.subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
        }
        app_fsm->subghz_fsk_tone_index = settings.subghz_fsk_tone_index;
        if(settings.speaker_value_index > dcf77_subghz_note_count()) {
            settings.speaker_value_index = 0;
        }
        app_fsm->speaker_value_index = settings.speaker_value_index;
        app_fsm->gpio_baseband_pin_number = settings.gpio_baseband_pin_number;
        app_fsm->gpio_rf_pin_number = settings.gpio_rf_pin_number;
        app_fsm->gpio_rf_duty_cycle = settings.gpio_rf_duty_cycle;
        app_fsm->led_color_index = settings.led_color_index;
        app_fsm->screen_mode = settings.screen_mode;
        dcf77_app_restore_saved_subghz_frequencies(
            app_fsm,
            settings.subghz_band_starts,
            settings.subghz_band_freqs,
            settings.subghz_band_count,
            settings.subghz_selected_band_start);
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[app_fsm->current_signal]);
    } else if(saved_struct_load(
                  DCF77_SETTINGS_PATH,
                  &settings_v7,
                  sizeof(settings_v7),
                  DCF77_SETTINGS_MAGIC,
                  7)) {
        dcf77_app_restore_saved_signal_freqs(
            app_fsm, settings_v7.signal_freqs, DCF77_SAVED_SIGNAL_COUNT_V7);
        if(settings_v7.current_signal >= RadioClockSignalCount) {
            settings_v7.current_signal = RadioClockSignalDcf77;
        }
        app_fsm->current_signal = (RadioClockSignal)settings_v7.current_signal;
        if(radio_clock_visible_signal_index(app_fsm->current_signal) >=
           radio_clock_visible_signal_count()) {
            app_fsm->current_signal = RadioClockSignalDcf77;
        }
        app_fsm->lf_transmit_enabled = settings_v7.lf_transmit_enabled != 0;
        if(settings_v7.subghz_signal_mode > SubGhzSignalModeFskFull) {
            settings_v7.subghz_signal_mode = SubGhzSignalModeDisabled;
        }
        app_fsm->subghz_signal_mode = (SubGhzSignalMode)settings_v7.subghz_signal_mode;
        if(settings_v7.subghz_fsk_tone_index >= dcf77_subghz_note_count()) {
            settings_v7.subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
        }
        app_fsm->subghz_fsk_tone_index = settings_v7.subghz_fsk_tone_index;
        if(settings_v7.speaker_value_index > dcf77_subghz_note_count()) {
            settings_v7.speaker_value_index = 0;
        }
        app_fsm->speaker_value_index = settings_v7.speaker_value_index;
        app_fsm->gpio_baseband_pin_number = settings_v7.gpio_baseband_pin_number;
        app_fsm->gpio_rf_pin_number = settings_v7.gpio_rf_pin_number;
        app_fsm->gpio_rf_duty_cycle = settings_v7.gpio_rf_duty_cycle;
        app_fsm->led_color_index = settings_v7.led_color_index;
        app_fsm->screen_mode = settings_v7.screen_mode;
        dcf77_app_restore_saved_subghz_frequencies(
            app_fsm,
            settings_v7.subghz_band_starts,
            settings_v7.subghz_band_freqs,
            settings_v7.subghz_band_count,
            settings_v7.subghz_selected_band_start);
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[app_fsm->current_signal]);
    } else if(saved_struct_load(
                  DCF77_SETTINGS_PATH,
                  &settings_v6,
                  sizeof(settings_v6),
                  DCF77_SETTINGS_MAGIC,
                  6)) {
        dcf77_app_restore_saved_signal_freqs(
            app_fsm, settings_v6.signal_freqs, DCF77_SAVED_SIGNAL_COUNT_V7);
        if(settings_v6.current_signal >= RadioClockSignalCount) {
            settings_v6.current_signal = RadioClockSignalDcf77;
        }
        app_fsm->current_signal = (RadioClockSignal)settings_v6.current_signal;
        if(radio_clock_visible_signal_index(app_fsm->current_signal) >=
           radio_clock_visible_signal_count()) {
            app_fsm->current_signal = RadioClockSignalDcf77;
        }
        app_fsm->lf_transmit_enabled = settings_v6.lf_transmit_enabled != 0;
        if(settings_v6.subghz_signal_mode > SubGhzSignalModeFskFull) {
            settings_v6.subghz_signal_mode = SubGhzSignalModeDisabled;
        }
        app_fsm->subghz_signal_mode = (SubGhzSignalMode)settings_v6.subghz_signal_mode;
        if(settings_v6.subghz_fsk_tone_index >= dcf77_subghz_note_count()) {
            settings_v6.subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
        }
        app_fsm->subghz_fsk_tone_index = settings_v6.subghz_fsk_tone_index;
        if(settings_v6.speaker_value_index > dcf77_subghz_note_count()) {
            settings_v6.speaker_value_index = 0;
        }
        app_fsm->speaker_value_index = settings_v6.speaker_value_index;
        dcf77_app_restore_saved_subghz_frequencies(
            app_fsm,
            settings_v6.subghz_band_starts,
            settings_v6.subghz_band_freqs,
            settings_v6.subghz_band_count,
            settings_v6.subghz_selected_band_start);
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[app_fsm->current_signal]);
    } else if(saved_struct_load(
                  DCF77_SETTINGS_PATH,
                  &settings_v5,
                  sizeof(settings_v5),
                  DCF77_SETTINGS_MAGIC,
                  5)) {
        dcf77_app_restore_saved_signal_freqs(
            app_fsm, settings_v5.signal_freqs, DCF77_SAVED_SIGNAL_COUNT_V7);
        if(settings_v5.current_signal >= RadioClockSignalCount) {
            settings_v5.current_signal = RadioClockSignalDcf77;
        }
        app_fsm->current_signal = (RadioClockSignal)settings_v5.current_signal;
        if(radio_clock_visible_signal_index(app_fsm->current_signal) >=
           radio_clock_visible_signal_count()) {
            app_fsm->current_signal = RadioClockSignalDcf77;
        }
        app_fsm->lf_transmit_enabled = settings_v5.lf_transmit_enabled != 0;
        app_fsm->subghz_signal_mode =
            settings_v5.subghz_transmit_enabled ? SubGhzSignalModeFsk : SubGhzSignalModeDisabled;
        if(settings_v5.subghz_fsk_tone_index >= dcf77_subghz_note_count()) {
            settings_v5.subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
        }
        app_fsm->subghz_fsk_tone_index = settings_v5.subghz_fsk_tone_index;
        if(settings_v5.speaker_value_index > dcf77_subghz_note_count()) {
            settings_v5.speaker_value_index = 0;
        }
        app_fsm->speaker_value_index = settings_v5.speaker_value_index;
        dcf77_app_restore_saved_subghz_frequencies(
            app_fsm,
            settings_v5.subghz_band_starts,
            settings_v5.subghz_band_freqs,
            settings_v5.subghz_band_count,
            settings_v5.subghz_selected_band_start);
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[app_fsm->current_signal]);
    } else if(saved_struct_load(
                  DCF77_SETTINGS_PATH,
                  &settings_v4,
                  sizeof(settings_v4),
                  DCF77_SETTINGS_MAGIC,
                  4)) {
        dcf77_app_restore_saved_signal_freqs(
            app_fsm, settings_v4.signal_freqs, DCF77_SAVED_SIGNAL_COUNT_V7);
        if(settings_v4.current_signal >= RadioClockSignalCount) {
            settings_v4.current_signal = RadioClockSignalDcf77;
        }
        app_fsm->current_signal = (RadioClockSignal)settings_v4.current_signal;
        if(radio_clock_visible_signal_index(app_fsm->current_signal) >=
            radio_clock_visible_signal_count()) {
            app_fsm->current_signal = RadioClockSignalDcf77;
        }
        app_fsm->lf_transmit_enabled = settings_v4.lf_transmit_enabled != 0;
        if(settings_v4.subghz_signal_mode > SubGhzSignalModeFskFull) {
            settings_v4.subghz_signal_mode = SubGhzSignalModeDisabled;
        }
        app_fsm->subghz_signal_mode = (SubGhzSignalMode)settings_v4.subghz_signal_mode;
        app_fsm->subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
        app_fsm->speaker_value_index =
            settings_v4.sound_enabled ? DCF77_SUBGHZ_DEFAULT_SPEAKER_VALUE_INDEX : 0;
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
        app_fsm->subghz_signal_mode = (SubGhzSignalMode)legacy_settings.subghz_signal_mode;
        app_fsm->subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
        app_fsm->speaker_value_index =
            legacy_settings.sound_enabled ? DCF77_SUBGHZ_DEFAULT_SPEAKER_VALUE_INDEX : 0;
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[RadioClockSignalDcf77]);
    } else {
        app_fsm->current_signal = RadioClockSignalDcf77;
        app_fsm->lf_transmit_enabled = true;
        app_fsm->subghz_signal_mode = SubGhzSignalModeDisabled;
        app_fsm->subghz_fsk_tone_index = DCF77_SUBGHZ_DEFAULT_FSK_TONE_INDEX;
        app_fsm->speaker_value_index = 0;
        dcf77_app_set_signal_frequency(app_fsm, app_fsm->signal_freqs[RadioClockSignalDcf77]);
    }

    dcf77_app_update_subghz_texts(app_fsm);
    dcf77_app_set_gpio_baseband_pin(app_fsm, app_fsm->gpio_baseband_pin_number);
    dcf77_app_set_gpio_rf_pin(app_fsm, app_fsm->gpio_rf_pin_number);
    dcf77_app_set_gpio_rf_duty_cycle(app_fsm, app_fsm->gpio_rf_duty_cycle);
    dcf77_app_set_led_color(app_fsm, (Dcf77LedColor)app_fsm->led_color_index);
    dcf77_app_set_screen_mode(app_fsm, (Dcf77ScreenMode)app_fsm->screen_mode);
}

void dcf77_app_apply_rf_settings(AppFSM* app_fsm) {
    if(!app_fsm->tx_active) {
        return;
    }

    if(dcf77_app_gpio_rf_enabled(app_fsm)) {
        dcf77_lf_deinit(app_fsm);
        dcf77_subghz_deinit(app_fsm);
        dcf77_gpio_rf_apply_settings(app_fsm);
        return;
    }

    dcf77_gpio_rf_deinit(app_fsm);

    if(app_fsm->subghz_signal_mode == SubGhzSignalModeFsk) {
        dcf77_lf_deinit(app_fsm);
    } else if(app_fsm->lf_transmit_enabled) {
        dcf77_lf_set_frequency(app_fsm, app_fsm->lf_freq);
    } else {
        dcf77_lf_deinit(app_fsm);
    }

    dcf77_subghz_apply_settings(app_fsm);
}

uint8_t dcf77_app_get_lf_freq_index(uint32_t freq) {
    if(freq <= LF_FREQ_MIN) {
        return 0;
    }

    if(freq >= LF_FREQ_MAX) {
        return DCF77_LF_FREQ_ITEM_VALUES - 1U;
    }

    /* The LF row is stepped by the custom input callback; keep a mid-range index so
       the stock widget continues to show both arrows for all non-edge frequencies. */
    return DCF77_LF_FREQ_ITEM_VALUES / 2U;
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
    dcf77_output_gpio_init(app_fsm);

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
    dcf77_output_gpio_deinit(app_fsm);
    dcf77_hw_indicator_set(app_fsm, false);
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
