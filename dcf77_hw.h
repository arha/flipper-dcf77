#ifndef DCF77_HW_H
#define DCF77_HW_H

typedef struct AppFSM AppFSM;

#include <stdint.h>
#include <stdbool.h>
#include <datetime/datetime.h>

void dcf77_hw_indicator_set(bool status);

void dcf77_output_gpio_init(void);
void dcf77_output_gpio_set(bool status);
void dcf77_output_gpio_deinit(void);

bool dcf77_wait_for_next_second(DateTime* dt);
void dcf77_timing_start(AppFSM* app_fsm);
void dcf77_timing_stop(AppFSM* app_fsm);

void dcf77_lf_set_frequency(AppFSM* app_fsm, uint32_t freq);
void dcf77_lf_apply_output(const AppFSM* app_fsm);
void dcf77_lf_deinit(AppFSM* app_fsm);

void dcf77_subghz_toggle(AppFSM* app_fsm);
void dcf77_subghz_apply_output(AppFSM* app_fsm);
void dcf77_subghz_deinit(AppFSM* app_fsm);

void dcf77_subghz_ook_toggle(AppFSM* app_fsm);
void dcf77_subghz_ook_apply_output(const AppFSM* app_fsm);

#endif
