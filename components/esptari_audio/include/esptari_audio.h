#pragma once

#include <stdint.h>

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits;
} esptari_audio_fmt_t;

void esptari_audio_init(const esptari_audio_fmt_t *fmt);
void esptari_audio_generate_test_tone(void);
