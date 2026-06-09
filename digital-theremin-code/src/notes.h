#ifndef NOTES_H
#define NOTES_H

#include <stdint.h>

#define NOTE_COUNT 60

static const uint16_t note_freqs[NOTE_COUNT] = {
     65,  69,  73,  78,  82,  87,  92,  98, 104, 110, 117, 123, // C2-B2
    131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247, // C3-B3
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, // C4-B4
    523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988, // C5-B5
   1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976  // C6-B6
};

static inline uint16_t find_nearest_note(uint16_t freq) {
    if (freq <= note_freqs[0]) return note_freqs[0];
    if (freq >= note_freqs[NOTE_COUNT - 1]) return note_freqs[NOTE_COUNT - 1];
    for (int i = 0; i < NOTE_COUNT - 1; i++) {
        if (freq >= note_freqs[i] && freq < note_freqs[i + 1]) {
            uint16_t diff_lo = freq - note_freqs[i];
            uint16_t diff_hi = note_freqs[i + 1] - freq;
            return (diff_lo <= diff_hi) ? note_freqs[i] : note_freqs[i + 1];
        }
    }
    return freq;
}

#endif
