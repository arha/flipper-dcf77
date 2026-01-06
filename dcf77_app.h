#ifndef __ARHA_FLIPPERAPP_DEMO
#define __ARHA_FLIPPERAPP_DEMO

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_speaker.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <input/input.h>
#include <applications/services/gui/modules/submenu.h>
#include <applications/services/gui/modules/variable_item_list.h>
#include <applications/services/gui/modules/widget.h>

#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <datetime/datetime.h>

#include "dcf77_util.h"

// the TAG is used for displaying a relevant prefix in logs. update it.
#define TAG "__ARHA_FLIPPERAPP"
#define DCF77_SECONDS_PER_MINUTE 60
#define DCF77_LPTIM_HZ 32768U
#define DCF77_FULL_SECOND_TICKS DCF77_LPTIM_HZ
#define DCF77_ZERO_HIGH_TICKS ((DCF77_LPTIM_HZ * 9U) / 10U)
#define DCF77_ONE_HIGH_TICKS ((DCF77_LPTIM_HZ * 8U) / 10U)
#define DCF77_MIN_TIMER_TICKS 8U
#define STARTUP_SCREEN_MS 1000U
#define SYNC_ON_START 0
#define DEBUG_SYNC_FRAME 0
#define SUBGHZ_OOK_INVERT 0
#define LF_FREQ_DEFAULT 77500U
#define LF_FREQ_MIN 50000U
#define LF_FREQ_MAX 200000U
#define LF_FREQ_STEP 2500U
#define SUBGHZ_FREQ 433670000
#define SUBGHZ_FSK_HALF_PERIOD_US 2273
#define SUBGHZ_FSK_IDLE_HALF_PERIOD_US 125
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
    EventKeyPress,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

typedef enum {
    AppScreenStartup,
    AppScreenMenu,
    AppScreenTx,
    AppScreenLfSettings,
    AppScreenSubGhzSettings,
    AppScreenAbout,
    AppScreenEgg,
} AppScreen;

typedef enum {
    SubGhzSignalModeDisabled,
    SubGhzSignalModeOok,
    SubGhzSignalModeFsk,
} SubGhzSignalMode;

typedef struct AppFSM {
    uint16_t len;
    KeyCode last_key;

    int counter;
    uint32_t lf_freq;

    uint8_t bit_number; // 0 - 59
    uint8_t bit_value;  // 0 or 1 for actual bits, 2 for end-of-minute marker
    volatile bool output_state;
    volatile bool output_dirty;
    uint8_t dcf77_message[8];
    uint8_t next_message[8];
    uint16_t second_high_ticks[DCF77_SECONDS_PER_MINUTE];
    uint16_t next_second_high_ticks[DCF77_SECONDS_PER_MINUTE];
    DateTime current_minute_dt;
    DateTime next_minute_dt;
    volatile uint8_t scheduler_second;
    volatile bool scheduler_low_phase;
    volatile bool scheduler_ready;
    volatile bool scheduler_synced;
    volatile bool startup_marker_wrap_pending;

    bool debug_flag;
    bool lf_ready;
    bool subghz_ready;
    volatile bool subghz_async_tx;
    volatile bool subghz_output;
    volatile bool subghz_tone_phase;
    SubGhzSignalMode subghz_signal_mode;
    bool tx_active;
    AppScreen screen;
    uint32_t startup_deadline_tick;
    uint32_t last_tx_refresh_tick;

    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    VariableItemList* lf_settings;
    VariableItemList* subghz_settings;
    Widget* startup_widget;
    Widget* about_widget;
    Widget* egg_widget;
    View* tx_view;
    NotificationApp* notification;
    bool lf_transmit_enabled;
    char lf_freq_text[16];

    uint8_t tx_minute;
    uint8_t tx_hour;
    uint8_t tx_day;
    uint8_t tx_month;
    uint8_t tx_year;
    uint8_t tx_dow;
    uint8_t tx_start_second_offset;
} AppFSM;

extern const NotificationSequence seq_c_minor;
extern const char* const dcf77_bitnames[];

#endif
