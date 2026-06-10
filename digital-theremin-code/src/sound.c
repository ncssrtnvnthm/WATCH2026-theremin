/**
 * sound.c - wavetable synth, optimized (no float math in hot paths)
 * Double-buffered DMA, integer math, LUT-based effects
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"

#include "sound.h"
#include "settings.h"
#include "notes.h"

#define WAVTABLESIZE 256

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

union pwm32 {
    uint32_t x;
    struct { uint8_t al; uint8_t ah; uint8_t bl; uint8_t bh; };
};

static uint8_t wavetables[WAVE_COUNT][WAVTABLESIZE];

/* double-buffered DMA output */
static union pwm32 sbuffer_a[WAVTABLESIZE];
static union pwm32 sbuffer_b[WAVTABLESIZE];
static union pwm32 *active_buf = sbuffer_a;
static union pwm32 *build_buf  = sbuffer_b;
static union pwm32 *bufferptr  = sbuffer_a;

static uint32_t dmafreq = 64000;

uint8_t mute = 0;
uint8_t old_mute = 0;
uint16_t old_freq = 0;

WaveType current_wave = WAVE_SINE;
EffectType current_effect = FX_NORMAL;

const char* wave_names[WAVE_COUNT] = {
    "Sine","Square","Triangle","Square2","Sawtooth","Organ",
    "Trumpet","Clarinet","Bassoon","Flute","Spinet","Organ2",
    "Harp","BigLead","Trombone","Musicbox"
};

const char* fx_names[FX_COUNT] = {
    "Normal","NoteOnly","WobSlow","WobMed","WobFast"
};

static uint PWMslice;
static int pwm_dma_chan, ctl_dma_chan, ptimer;
static uint8_t built_volume = 255; /* volume actually in the DMA buffer */

/* ---- wobulate LUT in 16.16 fixed point ---- */
static uint32_t wob_lut[256];
static uint32_t wob_phase_int;

static void build_wob_lut(void) {
    float st = powf(2.0f, 1.0f / 12.0f);
    for (int i = 0; i < 256; i++) {
        float phase = (float)i / 256.0f * 2.0f * M_PI;
        float ratio = powf(st, 0.5f * sinf(phase));
        wob_lut[i] = (uint32_t)(ratio * 65536.0f + 0.5f);
    }
}

/* ---- wavetable generators (init only, float OK) ---- */

static void fill_sine(uint8_t *buf, int n, float amp) {
    for (int i = 0; i < n; i++)
        buf[i] = (uint8_t)(127.5f + amp * 127.5f * sinf((float)i / n * 2.0f * M_PI));
}

static void fill_square(uint8_t *buf, int n, float duty, float amp) {
    int th = (int)(duty * n);
    for (int i = 0; i < n; i++)
        buf[i] = (uint8_t)(127.5f + amp * 127.5f * ((i < th) ? 1.0f : -1.0f));
}

static void fill_triangle(uint8_t *buf, int n, float amp) {
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        buf[i] = (uint8_t)(127.5f + amp * 127.5f * (4.0f * (t < 0.5f ? t : 1.0f - t) - 1.0f));
    }
}

static void fill_sawtooth(uint8_t *buf, int n, float amp) {
    for (int i = 0; i < n; i++)
        buf[i] = (uint8_t)(127.5f + amp * 127.5f * (2.0f * (float)i / n - 1.0f));
}

static void fill_harmonics(uint8_t *buf, int n, float amp,
                           float h1, float h2, float h3, float h4, float h5) {
    for (int i = 0; i < n; i++) {
        float p = (float)i / n * 2.0f * M_PI;
        float v = h1*sinf(p) + h2*sinf(2*p) + h3*sinf(3*p) + h4*sinf(4*p) + h5*sinf(5*p);
        buf[i] = (uint8_t)(127.5f + amp * 127.5f * v);
    }
}

static void generate_wavetables(void) {
    fill_sine(wavetables[WAVE_SINE], WAVTABLESIZE, 1.0f);
    fill_square(wavetables[WAVE_SQUARE], WAVTABLESIZE, 0.5f, 1.0f);
    fill_triangle(wavetables[WAVE_TRIANGLE], WAVTABLESIZE, 1.0f);
    fill_square(wavetables[WAVE_SQUARE2], WAVTABLESIZE, 0.25f, 1.0f);
    fill_sawtooth(wavetables[WAVE_SAWTOOTH], WAVTABLESIZE, 1.0f);
    fill_harmonics(wavetables[WAVE_ORGAN], WAVTABLESIZE, 1.0f, 0.6f,0,0.4f,0,0.2f);
    fill_sawtooth(wavetables[WAVE_TRUMPET], WAVTABLESIZE, 0.7f);
    fill_square(wavetables[WAVE_CLARINET], WAVTABLESIZE, 0.5f, 0.8f);
    fill_harmonics(wavetables[WAVE_BASSOON], WAVTABLESIZE, 0.7f, 0.5f,0,0.3f,0,0);
    fill_sine(wavetables[WAVE_FLUTE], WAVTABLESIZE, 0.8f);
    fill_harmonics(wavetables[WAVE_SPINET], WAVTABLESIZE, 0.6f, 0.5f,0,0.2f,0,0.1f);
    fill_harmonics(wavetables[WAVE_ORGAN2], WAVTABLESIZE, 0.8f, 0.8f,0.4f,0.3f,0.1f,0.05f);
    fill_triangle(wavetables[WAVE_HARP], WAVTABLESIZE, 0.5f);
    fill_sawtooth(wavetables[WAVE_BIGLEAD], WAVTABLESIZE, 0.9f);
    fill_square(wavetables[WAVE_TROMBONE], WAVTABLESIZE, 0.3f, 0.9f);
    fill_harmonics(wavetables[WAVE_MUSICBOX], WAVTABLESIZE, 0.4f, 0.3f,0.1f,0.05f,0,0);
}

/* ---- build DMA buffer (into inactive buffer, integer math) ---- */
static uint8_t rebuild_pending = 0;
static uint8_t rebuild_volume = 255;

static void do_rebuild(void) {
    uint8_t *wt = wavetables[current_wave];
    uint8_t vol = rebuild_volume;
    for (int i = 0; i < WAVTABLESIZE; i++) {
        int16_t s = (int16_t)(wt[i] - 128) * (int16_t)vol / 255;
        uint8_t v = (uint8_t)(128 + s);
        build_buf[i].al = v;
        build_buf[i].ah = 0;
        build_buf[i].bl = v;
        build_buf[i].bh = 0;
    }
    /* atomically swap */
    union pwm32 *tmp = active_buf;
    active_buf = build_buf;
    build_buf  = tmp;
    bufferptr  = active_buf;
    built_volume = vol;
    rebuild_pending = 0;
}

static void trigger_rebuild(uint8_t vol) {
    rebuild_volume = vol;
    rebuild_pending = 1;
}

void init_sound(void) {
    build_wob_lut();
    generate_wavetables();
    do_rebuild();

    gpio_init(soundIO1);
    gpio_set_dir(soundIO1, GPIO_OUT);
    gpio_set_function(soundIO1, GPIO_FUNC_PWM);

    PWMslice = pwm_gpio_to_slice_num(soundIO1);
    pwm_set_clkdiv(PWMslice, 1);
    pwm_set_wrap(PWMslice, 256);
    pwm_set_enabled(PWMslice, true);

    pwm_dma_chan = dma_claim_unused_channel(true);
    ctl_dma_chan = dma_claim_unused_channel(true);

    dma_channel_config ctl_cfg = dma_channel_get_default_config(ctl_dma_chan);
    channel_config_set_transfer_data_size(&ctl_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&ctl_cfg, false);
    channel_config_set_write_increment(&ctl_cfg, false);

    dma_channel_configure(ctl_dma_chan, &ctl_cfg,
                          &dma_hw->ch[pwm_dma_chan].al3_read_addr_trig,
                          &bufferptr, 1, false);

    dma_channel_config pwm_cfg = dma_channel_get_default_config(pwm_dma_chan);
    channel_config_set_transfer_data_size(&pwm_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&pwm_cfg, true);
    channel_config_set_write_increment(&pwm_cfg, false);

    ptimer = dma_claim_unused_timer(true);
    dma_timer_set_fraction(ptimer, 1, 1000);
    channel_config_set_dreq(&pwm_cfg, dma_get_timer_dreq(ptimer));
    channel_config_set_chain_to(&pwm_cfg, ctl_dma_chan);

    dma_channel_configure(pwm_dma_chan, &pwm_cfg,
                          &pwm_hw->slice[PWMslice].cc,
                          active_buf, WAVTABLESIZE, false);

    dma_start_channel_mask(1u << ctl_dma_chan);
}

void set_freq(uint16_t f) {
    if (rebuild_pending) do_rebuild();
    if (f != old_freq) {
        dmafreq = PICOCLOCK * 1000 / 256 / f;
        dma_timer_set_fraction(ptimer, 1, dmafreq);
        old_freq = f;
    }
}

void set_volume(uint8_t v) {
    int diff = abs((int)v - (int)built_volume);
    if (diff >= 4) trigger_rebuild(v);
}

void set_wave(WaveType w) {
    if (w != current_wave) {
        current_wave = w;
        trigger_rebuild(built_volume);
    }
}

void set_effect(EffectType e) {
    current_effect = e;
    wob_phase_int = 0;
}

void DoMute(uint8_t m) {
    mute = m;
    if (mute != old_mute) {
        if (mute) {
            hw_clear_bits(&dma_hw->ch[ctl_dma_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
            hw_clear_bits(&dma_hw->ch[pwm_dma_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        } else {
            hw_set_bits(&dma_hw->ch[ctl_dma_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
            hw_set_bits(&dma_hw->ch[pwm_dma_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
            dma_start_channel_mask(1u << ctl_dma_chan);
        }
    }
    old_mute = mute;
}

uint16_t apply_effect(uint16_t f) {
    switch (current_effect) {
    case FX_NOTE_ONLY:
        return find_nearest_note(f);
    case FX_WOBULATE_SLOW:
    case FX_WOBULATE_MED:
    case FX_WOBULATE_FAST: {
        uint32_t step = (current_effect == FX_WOBULATE_SLOW) ? 857 :
                        (current_effect == FX_WOBULATE_MED) ? 2143 : 3429;
        wob_phase_int += step;
        uint8_t idx = (uint8_t)(wob_phase_int >> 20);
        return (uint16_t)(((uint32_t)f * wob_lut[idx]) >> 16);
    }
    default:
        return f;
    }
}
