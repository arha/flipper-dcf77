#ifndef DCF77_LOGIC_H
#define DCF77_LOGIC_H

typedef struct AppFSM AppFSM;

#include <datetime/datetime.h>
#include <stdint.h>
#include <stdbool.h>

uint8_t dcf77_get_message_bit(const uint8_t* message, uint8_t bit);
void dcf77_logic_init(AppFSM* app_fsm);
void dcf77_logic_refresh_message(AppFSM* app_fsm, const DateTime* dt);
bool dcf77_logic_tick(AppFSM* app_fsm, const DateTime* dt, bool* output_changed);

#endif
