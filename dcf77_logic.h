#ifndef DCF77_LOGIC_H
#define DCF77_LOGIC_H

typedef struct AppFSM AppFSM;

#include <datetime/datetime.h>
#include <stdint.h>
#include <stdbool.h>

uint8_t dcf77_get_message_bit(const uint8_t* message, uint8_t bit);
void dcf77_logic_init(AppFSM* app_fsm);
void dcf77_logic_prepare_minute(AppFSM* app_fsm, const DateTime* dt, bool as_next_minute);
void dcf77_logic_activate_next_minute(AppFSM* app_fsm);
void dcf77_logic_sync_to_second(AppFSM* app_fsm, const DateTime* dt);
uint16_t dcf77_logic_get_current_high_ticks(const AppFSM* app_fsm);

#endif
