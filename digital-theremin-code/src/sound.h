#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WAVE_SINE = 0, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SQUARE2,
    WAVE_SAWTOOTH, WAVE_ORGAN, WAVE_TRUMPET, WAVE_CLARINET,
    WAVE_BASSOON, WAVE_FLUTE, WAVE_SPINET, WAVE_ORGAN2,
    WAVE_HARP, WAVE_BIGLEAD, WAVE_TROMBONE, WAVE_MUSICBOX,
    WAVE_COUNT
} WaveType;

typedef enum {
    FX_NORMAL = 0, FX_NOTE_ONLY, FX_WOBULATE_SLOW, FX_WOBULATE_MED, FX_WOBULATE_FAST,
    FX_MAJOR_SCALE, FX_MINOR_SCALE,
    FX_COUNT
} EffectType;

extern const char* wave_names[WAVE_COUNT];
extern const char* fx_names[FX_COUNT];
extern WaveType current_wave;
extern EffectType current_effect;

void init_sound(void);
void set_freq(float f);
void set_volume(uint8_t v);
void set_wave(WaveType w);
void set_effect(EffectType e);
void DoMute(uint8_t m);
float apply_effect(uint16_t freq);

#ifdef __cplusplus
}
#endif

#endif
