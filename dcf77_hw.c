#include "dcf77_hw.h"
#include "dcf77_app.h"

static void dcf77_subghz_init(AppFSM* app_fsm) {
    if(app_fsm->subghz_ready) {
        return;
    }

    app_fsm->subghz_enabled = false;
    app_fsm->subghz_output = false;

    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_270khz_async_regs);
    furi_hal_subghz_set_frequency_and_path(SUBGHZ_FREQ);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);

    app_fsm->subghz_ready = true;
}

void dcf77_hw_indicator_set(bool status) {
    static bool last;

    if(last != status) {
        if(status) {
            furi_hal_light_set(LightRed, 0xFF);
            furi_hal_light_set(LightGreen, 0x1F);
        } else {
            furi_hal_light_set(LightRed | LightGreen | LightBlue, 0);
        }
    }

    last = status;
}

void dcf77_output_gpio_init(void) {
    furi_hal_gpio_write(OUTPUT_PIN, false);
    furi_hal_gpio_init(OUTPUT_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
}

void dcf77_output_gpio_set(bool status) {
    furi_hal_gpio_write(OUTPUT_PIN, status);
}

void dcf77_output_gpio_deinit(void) {
    furi_hal_gpio_init(OUTPUT_PIN, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);
}

void dcf77_lf_set_frequency(AppFSM* app_fsm, uint32_t freq) {
    if(app_fsm->lf_ready) {
        furi_hal_rfid_tim_read_pause();
        furi_hal_rfid_tim_read_stop();
        furi_hal_rfid_pins_reset();
    }

    app_fsm->lf_freq = freq;
    furi_hal_rfid_tim_read_start(app_fsm->lf_freq, 0.5f);
    furi_hal_rfid_tim_read_pause();
    app_fsm->lf_ready = true;

    dcf77_lf_apply_output(app_fsm);
}

void dcf77_lf_apply_output(const AppFSM* app_fsm) {
    if(!app_fsm->lf_ready) {
        return;
    }

    if(app_fsm->output_state) {
        furi_hal_rfid_tim_read_continue();
    } else {
        furi_hal_rfid_tim_read_pause();
    }
}

void dcf77_lf_deinit(AppFSM* app_fsm) {
    if(!app_fsm->lf_ready) {
        return;
    }

    furi_hal_rfid_tim_read_pause();
    furi_hal_rfid_tim_read_stop();
    furi_hal_rfid_pins_reset();
    app_fsm->lf_ready = false;
}

void dcf77_subghz_toggle(AppFSM* app_fsm) {
    if(!app_fsm->subghz_ready) {
        dcf77_subghz_init(app_fsm);
    }

    app_fsm->subghz_enabled = !app_fsm->subghz_enabled;

    if(app_fsm->subghz_enabled) {
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(&gpio_cc1101_g0, app_fsm->output_state);
        if(!furi_hal_subghz_tx()) {
            furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
            app_fsm->subghz_enabled = false;
            app_fsm->subghz_output = false;
        } else {
            app_fsm->subghz_output = app_fsm->output_state;
        }
    } else {
        furi_hal_gpio_write(&gpio_cc1101_g0, false);
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
        furi_hal_subghz_idle();
        app_fsm->subghz_output = false;
    }
}

void dcf77_subghz_apply_output(const AppFSM* app_fsm) {
    if(app_fsm->subghz_enabled) {
        furi_hal_gpio_write(&gpio_cc1101_g0, app_fsm->output_state);
    }
}

void dcf77_subghz_deinit(AppFSM* app_fsm) {
    if(!app_fsm->subghz_ready) {
        return;
    }

    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
}
