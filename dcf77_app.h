#ifndef __ARHA_FLIPPERAPP_DEMO
#define __ARHA_FLIPPERAPP_DEMO

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_speaker.h>
#include <gui/gui.h>
#include <input/input.h>

#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "dcf77_util.h"

// the TAG is used for displaying a relevant prefix in logs. update it.
#define TAG "__ARHA_FLIPPERAPP"
#define TIMER_HZ 30
#define TIME_ZERO 24
#define TIME_ONE 27
#define LF_FREQ_LOW 77500
#define LF_FREQ_HIGH (77500 * 2)
#define SUBGHZ_FREQ 433670000
#define SUBGHZ_FSK_HALF_PERIOD_US 2273
#define OUTPUT_PIN &gpio_ext_pc3
// #define TIME_ZERO 15
// #define TIME_ONE 5

typedef enum {
    KeyNone,
    KeyUp,
    KeyRight,
    KeyDown,
    KeyLeft,
    KeyOK
} KeyCode;


typedef enum {
    EventTimerTick,
    EventKeyPress,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

typedef struct AppFSM {
    uint16_t len;
    KeyCode last_key;

    FuriTimer* _timer;
    FuriMessageQueue* _event_queue;

    int counter;
    uint32_t lf_freq;

    uint8_t bit_number; // 0 - 59
    uint8_t bit_value;  // 0 or 1 for actual bits, 2 for end-of-minute marker
    uint8_t baseband_counter;   // 0 - 20, so we can generate 800 and 900 ms wide pulses (bit0 = 800ms = 16; bit1 = 900ms = 18; bit2 = 1000ms = 20)
    bool output_state;
    uint8_t last_second;
    bool last_output;
    uint8_t dcf77_message[8]; // these are 8 bytes which encode, LSB, every bit in the DCF77 message. see set_dcf_message()
    uint8_t next_message[8];

    bool buffer_swap_pending;
    bool debug_flag;
    bool lf_ready;
    bool subghz_ready;
    bool subghz_enabled;
    bool subghz_async_tx;
    bool subghz_output;
    bool subghz_tone_phase;

    uint8_t tx_minute;
    uint8_t tx_hour;
    uint8_t tx_day;
    uint8_t tx_month;
    uint8_t tx_year;
    uint8_t tx_dow;
} AppFSM;

extern const NotificationSequence seq_c_minor;
extern const char* const dcf77_bitnames[];

#endif
