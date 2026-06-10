/*
 * Digital Theremin
 * Dual VL53L0X + 16-voice synth + SSD1306 display
 * Optimized: fixed-point, double-buffered DMA, sensor recovery
 * 
 * A WATCH 2016 Project
 * 
 * Inspired by Luminiferous Theremin created by Derek Woodroffe
 * https://github.com/ExtremeElectronics/Theremin
 */

#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include "settings.h"
#include "sound.h"
#include "display.h"

#define MIN_DISTANCE 30
#define MAX_DISTANCE 450
#define MIN_FREQUENCY  65
#define MAX_FREQUENCY 2000

static VL53L0X sensor_freq;
static VL53L0X sensor_vol;

static int32_t freq_fp = MIN_FREQUENCY << 8;
static uint8_t volume = 128;
static WaveType wave = WAVE_SINE;
static EffectType effect = FX_NORMAL;
static uint8_t muted = 1;

uint8_t dual_sensor = 0;

static uint32_t last_b1_ms, last_b2_ms;
static uint8_t last_b1, last_b2;

static void check_buttons(void) {
    uint32_t now = millis();
    uint8_t b1 = !gpio_get(BUTT1_PIN);
    uint8_t b2 = !gpio_get(BUTT2_PIN);

    if (b1 != last_b1 && (now - last_b1_ms) > BUTTON_DEBOUNCE_MS) {
        last_b1_ms = now;
        if (b1) { wave = (WaveType)((wave + 1) % WAVE_COUNT); set_wave(wave); }
    }
    last_b1 = b1;

    if (b2 != last_b2 && (now - last_b2_ms) > BUTTON_DEBOUNCE_MS) {
        last_b2_ms = now;
        if (b2) { effect = (EffectType)((effect + 1) % FX_COUNT); set_effect(effect); }
    }
    last_b2 = b2;
}

static void restart_sensors(void) {
    sensor_freq.startContinuous(50);
    if (dual_sensor) sensor_vol.startContinuous(50);
}

void setup() {
    set_sys_clock_khz(PICOCLOCK, false);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); delay(250);
    digitalWrite(LED_BUILTIN, LOW);

    Wire1.setSDA(I2C_SDA_PIN);
    Wire1.setSCL(I2C_SCL_PIN);
    Wire1.setClock(I2C_BAUDRATE);
    Wire1.begin();

    init_sound();
    init_display();

    gpio_init(XSHUT_VOL_PIN);
    gpio_set_dir(XSHUT_VOL_PIN, GPIO_OUT);
    digitalWrite(XSHUT_VOL_PIN, LOW);

    gpio_init(BUTT1_PIN);
    gpio_set_dir(BUTT1_PIN, GPIO_IN);
    gpio_pull_up(BUTT1_PIN);
    gpio_init(BUTT2_PIN);
    gpio_set_dir(BUTT2_PIN, GPIO_IN);
    gpio_pull_up(BUTT2_PIN);

    delay(10);

    sensor_freq.setBus(&Wire1);
    sensor_freq.setTimeout(100);
    if (!sensor_freq.init()) while (1) { tight_loop_contents(); }
    sensor_freq.setAddress(I2C_TOF_FREQ_ADDR);

    digitalWrite(XSHUT_VOL_PIN, HIGH);
    delay(10);

    Wire1.beginTransmission(I2C_TOF_VOL_ADDR);
    if (Wire1.endTransmission() == 0) {
        sensor_vol.setBus(&Wire1);
        sensor_vol.setTimeout(100);
        if (sensor_vol.init()) {
            dual_sensor = 1;
            sensor_vol.startContinuous(50);
        }
    }

    sensor_freq.startContinuous(50);

    set_freq((uint16_t)(freq_fp >> 8));
    set_volume(255);
    set_wave(wave);
    set_effect(effect);
    DoMute(1);
    display_update(0, 0, (uint16_t)(freq_fp >> 8), 0, 1);
}

void loop() {
    static uint16_t dist_freq = 0, dist_vol = 0;
    static uint16_t last_disp_vol, last_disp_freq, last_disp_f;
    static uint8_t  last_disp_muted = 0xFF;
    static uint32_t last_display, last_loop_ms;
    static uint32_t freq_fails = 0, vol_fails = 0;

    /* ---- I2C watchdog ---- */
    uint32_t now = millis();
    if (last_loop_ms && (now - last_loop_ms) > 1000) {
        restart_sensors();
        freq_fails = vol_fails = 0;
    }
    last_loop_ms = now;

    /* ---- FREQ sensor ---- */
    uint16_t df = sensor_freq.readRangeContinuousMillimeters();
    if (!sensor_freq.timeoutOccurred() && df < 4096) {
        dist_freq = df;
        freq_fails = 0;
    } else {
        if (++freq_fails > 50) {
            sensor_freq.startContinuous(50);
            freq_fails = 0;
        }
    }

    /* ---- VOL sensor ---- */
    if (dual_sensor) {
        uint16_t dv = sensor_vol.readRangeContinuousMillimeters();
        if (!sensor_vol.timeoutOccurred() && dv < 4096) {
            dist_vol = dv;
            vol_fails = 0;
        } else {
            if (++vol_fails > 50) {
                sensor_vol.startContinuous(50);
                vol_fails = 0;
            }
        }
    } else {
        dist_vol = dist_freq;
    }

    check_buttons();

    /* ---- frequency (fixed-point) ---- */
    if (dist_freq >= MIN_DISTANCE && dist_freq <= MAX_DISTANCE) {
        int32_t ratio = ((int32_t)(MAX_DISTANCE - dist_freq) << 8) / (MAX_DISTANCE - MIN_DISTANCE);
        int32_t target = (MIN_FREQUENCY << 8) + (ratio * (MAX_FREQUENCY - MIN_FREQUENCY));
        freq_fp += (target - freq_fp) >> 1;
    }

    uint16_t out_freq = apply_effect((uint16_t)(freq_fp >> 8));
    set_freq(out_freq);

    /* ---- volume (fixed-point) ---- */
    if (dist_vol >= MIN_DISTANCE && dist_vol <= MAX_DISTANCE) {
        int32_t vratio = ((int32_t)(MAX_DISTANCE - dist_vol) << 8) / (MAX_DISTANCE - MIN_DISTANCE);
        volume = (uint8_t)((vratio * 255) >> 8);
        set_volume(volume);
        if (muted) { muted = 0; DoMute(0); }
    } else {
        if (!muted) { muted = 1; DoMute(1); }
    }

    /* ---- display: only flush when values change ---- */
    if (dist_vol   != last_disp_vol   ||
        dist_freq  != last_disp_freq  ||
        out_freq   != last_disp_f     ||
        muted      != last_disp_muted ||
        (now - last_display) > 1000) {
        last_disp_vol   = dist_vol;
        last_disp_freq  = dist_freq;
        last_disp_f     = out_freq;
        last_disp_muted = muted;
        last_display    = now;
        display_update(dist_vol, dist_freq, out_freq, volume, muted);
    }
}
