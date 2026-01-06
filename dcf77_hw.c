#include "dcf77_hw.h"
#include "dcf77_app.h"
#include "dcf77_logic.h"

#include <stm32wbxx_ll_lptim.h>
#include <stm32wbxx_ll_bus.h>
#include <stm32wbxx_ll_rcc.h>

#define DCF77_SCHEDULER LPTIM2

static void dcf77_subghz_init(AppFSM* app_fsm, const uint8_t* preset) {
    if(app_fsm->subghz_ready) {
        return;
    }

    app_fsm->subghz_enabled = false;
    app_fsm->subghz_async_tx = false;
    app_fsm->subghz_output = false;
    app_fsm->subghz_tone_phase = false;

    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(preset);
    furi_hal_subghz_set_frequency_and_path(SUBGHZ_FREQ);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);

    app_fsm->subghz_ready = true;
}

static LevelDuration dcf77_subghz_fsk_yield(void* context) {
    AppFSM* app_fsm = context;

    if(!app_fsm->subghz_enabled) {
        return level_duration_reset();
    }

    if(!app_fsm->output_state) {
        app_fsm->subghz_tone_phase = false;
        app_fsm->subghz_output = false;
        return level_duration_make(false, 10000);
    }

    app_fsm->subghz_tone_phase = !app_fsm->subghz_tone_phase;
    app_fsm->subghz_output = app_fsm->subghz_tone_phase;
    return level_duration_make(app_fsm->subghz_tone_phase, SUBGHZ_FSK_HALF_PERIOD_US);
}

static uint32_t dcf77_scheduler_sanitize_ticks(uint32_t ticks) {
    if(ticks < DCF77_MIN_TIMER_TICKS) {
        return DCF77_MIN_TIMER_TICKS;
    }

    return ticks;
}

static void dcf77_scheduler_reset(void) {
    furi_hal_bus_reset(FuriHalBusLPTIM2);
    NVIC_ClearPendingIRQ(LPTIM2_IRQn);
}

static void dcf77_scheduler_start_ticks(uint32_t ticks) {
    ticks = dcf77_scheduler_sanitize_ticks(ticks) - 1U;

    LL_LPTIM_Enable(DCF77_SCHEDULER);
    while(!LL_LPTIM_IsEnabled(DCF77_SCHEDULER)) {
    }

    LL_LPTIM_ClearFlag_CMPM(DCF77_SCHEDULER);
    LL_LPTIM_ClearFlag_ARRM(DCF77_SCHEDULER);
    LL_LPTIM_EnableIT_CMPM(DCF77_SCHEDULER);
    LL_LPTIM_SetCompare(DCF77_SCHEDULER, ticks - 3U);
    LL_LPTIM_SetAutoReload(DCF77_SCHEDULER, ticks);
    LL_LPTIM_StartCounter(DCF77_SCHEDULER, LL_LPTIM_OPERATING_MODE_ONESHOT);
}

static void dcf77_apply_output_isr(AppFSM* app_fsm, bool output) {
    if(app_fsm->output_state == output) {
        return;
    }

    app_fsm->output_state = output;
    app_fsm->output_dirty = true;

    dcf77_output_gpio_set(output);
    if(app_fsm->lf_ready) {
        if(output) {
            furi_hal_rfid_tim_read_continue();
        } else {
            furi_hal_rfid_tim_read_pause();
        }
    }

}

static void dcf77_prepare_following_minute(AppFSM* app_fsm) {
    DateTime next_minute = app_fsm->current_minute_dt;
    next_minute.second = 0;
    uint32_t next_timestamp = datetime_datetime_to_timestamp(&next_minute) + 60U;
    datetime_timestamp_to_datetime(next_timestamp, &next_minute);
    dcf77_logic_prepare_minute(app_fsm, &next_minute, true);
}

static void dcf77_scheduler_advance_second(AppFSM* app_fsm) {
    app_fsm->scheduler_second++;
    if(app_fsm->scheduler_second >= DCF77_SECONDS_PER_MINUTE) {
        app_fsm->scheduler_second = 0;
        dcf77_logic_activate_next_minute(app_fsm);
        dcf77_prepare_following_minute(app_fsm);
    }

    app_fsm->bit_number = app_fsm->scheduler_second;
    app_fsm->bit_value = dcf77_get_message_bit(app_fsm->dcf77_message, app_fsm->bit_number);
}

static void dcf77_timing_isr(void* context) {
    AppFSM* app_fsm = context;

    if(LL_LPTIM_IsActiveFlag_CMPM(DCF77_SCHEDULER)) {
        LL_LPTIM_ClearFlag_CMPM(DCF77_SCHEDULER);
    }
    if(LL_LPTIM_IsActiveFlag_ARRM(DCF77_SCHEDULER)) {
        LL_LPTIM_ClearFlag_ARRM(DCF77_SCHEDULER);
    }

    if(!app_fsm->scheduler_ready) {
        return;
    }

    if(app_fsm->scheduler_low_phase) {
        app_fsm->scheduler_low_phase = false;
        dcf77_scheduler_advance_second(app_fsm);
        dcf77_apply_output_isr(app_fsm, true);
        dcf77_scheduler_start_ticks(dcf77_logic_get_current_high_ticks(app_fsm));
        return;
    }

    if(app_fsm->scheduler_second == 59) {
        dcf77_scheduler_advance_second(app_fsm);
        dcf77_scheduler_start_ticks(dcf77_logic_get_current_high_ticks(app_fsm));
        return;
    }

    const uint32_t high_ticks = dcf77_logic_get_current_high_ticks(app_fsm);
    app_fsm->scheduler_low_phase = true;
    dcf77_apply_output_isr(app_fsm, false);
    dcf77_scheduler_start_ticks(DCF77_FULL_SECOND_TICKS - high_ticks);
}

static void dcf77_scheduler_init(AppFSM* app_fsm) {
    furi_hal_bus_enable(FuriHalBusLPTIM2);
    LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM2_CLKSOURCE_LSE);
    LL_APB1_GRP2_EnableClockSleep(LL_APB1_GRP2_PERIPH_LPTIM2);
    furi_hal_interrupt_set_isr_ex(
        FuriHalInterruptIdLpTim2,
        FuriHalInterruptPriorityKamiSama,
        dcf77_timing_isr,
        app_fsm);
    dcf77_scheduler_reset();
}

bool dcf77_wait_for_next_second(DateTime* dt) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    const uint8_t start_second = now.second;

    for(uint16_t i = 0; i < 1500; i++) {
        furi_delay_ms(1);
        furi_hal_rtc_get_datetime(&now);
        if(now.second != start_second) {
            *dt = now;
            return true;
        }
    }

    *dt = now;
    return false;
}

void dcf77_timing_start(AppFSM* app_fsm) {
    dcf77_scheduler_init(app_fsm);
    dcf77_apply_output_isr(app_fsm, true);
    dcf77_scheduler_start_ticks(dcf77_logic_get_current_high_ticks(app_fsm));
}

void dcf77_timing_stop(AppFSM* app_fsm) {
    app_fsm->scheduler_ready = false;
    dcf77_scheduler_reset();
    furi_hal_interrupt_set_isr(FuriHalInterruptIdLpTim2, NULL, NULL);
    furi_hal_bus_disable(FuriHalBusLPTIM2);
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
    app_fsm->lf_ready = true;

    if(app_fsm->output_state) {
        furi_hal_rfid_tim_read_continue();
    } else {
        furi_hal_rfid_tim_read_pause();
    }
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

void dcf77_subghz_ook_toggle(AppFSM* app_fsm) {
    if(!app_fsm->subghz_ready) {
        dcf77_subghz_init(app_fsm, subghz_device_cc1101_preset_ook_270khz_async_regs);
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

void dcf77_subghz_ook_apply_output(const AppFSM* app_fsm) {
    if(app_fsm->subghz_enabled) {
        furi_hal_gpio_write(&gpio_cc1101_g0, app_fsm->output_state);
    }
}

static void dcf77_subghz_fsk_toggle(AppFSM* app_fsm) {
    if(!app_fsm->subghz_ready) {
        dcf77_subghz_init(app_fsm, subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs);
    }

    if(app_fsm->subghz_enabled) {
        app_fsm->subghz_enabled = false;
        if(app_fsm->subghz_async_tx) {
            furi_hal_subghz_stop_async_tx();
        }
        app_fsm->subghz_async_tx = false;
        app_fsm->subghz_tone_phase = false;
        app_fsm->subghz_output = false;
        app_fsm->subghz_dirty = false;
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
        furi_hal_subghz_idle();
        if(app_fsm->lf_transmit_enabled) {
            dcf77_lf_set_frequency(app_fsm, app_fsm->lf_freq);
        } else {
            dcf77_lf_deinit(app_fsm);
        }
        return;
    }

    dcf77_lf_deinit(app_fsm);
    app_fsm->subghz_enabled = true;
    app_fsm->subghz_tone_phase = false;
    app_fsm->subghz_output = false;
    app_fsm->subghz_dirty = true;
}

void dcf77_subghz_toggle(AppFSM* app_fsm) {
    dcf77_subghz_fsk_toggle(app_fsm);
}

void dcf77_subghz_apply_output(AppFSM* app_fsm) {
    if(!app_fsm->subghz_enabled) {
        app_fsm->subghz_dirty = false;
        return;
    }

    if(app_fsm->output_state) {
        if(!app_fsm->subghz_async_tx) {
            app_fsm->subghz_tone_phase = false;
            app_fsm->subghz_output = false;
            app_fsm->subghz_async_tx =
                furi_hal_subghz_start_async_tx(dcf77_subghz_fsk_yield, app_fsm);
            if(!app_fsm->subghz_async_tx) {
                app_fsm->subghz_enabled = false;
                app_fsm->subghz_output = false;
                app_fsm->subghz_dirty = false;
                furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
                furi_hal_subghz_idle();
                if(app_fsm->lf_transmit_enabled) {
                    dcf77_lf_set_frequency(app_fsm, app_fsm->lf_freq);
                } else {
                    dcf77_lf_deinit(app_fsm);
                }
            }
        }
    } else if(app_fsm->subghz_async_tx) {
        furi_hal_subghz_stop_async_tx();
        app_fsm->subghz_async_tx = false;
        app_fsm->subghz_tone_phase = false;
        app_fsm->subghz_output = false;
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
        furi_hal_subghz_idle();
    }
    app_fsm->subghz_dirty = false;
}

void dcf77_subghz_deinit(AppFSM* app_fsm) {
    if(!app_fsm->subghz_ready) {
        return;
    }

    app_fsm->subghz_enabled = false;
    if(app_fsm->subghz_async_tx) {
        furi_hal_subghz_stop_async_tx();
    }
    app_fsm->subghz_async_tx = false;
    app_fsm->subghz_tone_phase = false;
    app_fsm->subghz_output = false;
    app_fsm->subghz_dirty = false;
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
}
