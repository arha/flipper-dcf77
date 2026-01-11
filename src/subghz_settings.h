#ifndef DCF77_SUBGHZ_SETTINGS_H
#define DCF77_SUBGHZ_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DCF77_SUBGHZ_MAX_BANDS 8U
#define DCF77_SUBGHZ_STEP_HZ 10000U

typedef struct {
    uint32_t start_hz;
    uint32_t end_hz;
} Dcf77SubGhzBand;

size_t dcf77_subghz_note_count(void);
uint32_t dcf77_subghz_note_freq_millihz(size_t index);
float dcf77_subghz_note_freq_hz(size_t index);
uint32_t dcf77_subghz_note_half_period_us(size_t index);
void dcf77_subghz_format_note_text(char* buffer, size_t buffer_size, size_t index);
void dcf77_subghz_format_band_text(char* buffer, size_t buffer_size, uint32_t band_start_hz);
void dcf77_subghz_format_frequency_text(char* buffer, size_t buffer_size, uint32_t freq_hz);
void dcf77_subghz_format_frequency_khz_text(char* buffer, size_t buffer_size, uint32_t freq_hz);
uint32_t dcf77_subghz_clamp_frequency(uint32_t freq_hz, const Dcf77SubGhzBand* band);
uint32_t
    dcf77_subghz_step_frequency(uint32_t freq_hz, const Dcf77SubGhzBand* band, int8_t direction);

#endif
