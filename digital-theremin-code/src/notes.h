#ifndef NOTES_H
#define NOTES_H

#define NOTE_COUNT 60
#define SCALE_COUNT 15

static const float note_freqs[NOTE_COUNT] = {
     65.4064f,  69.2957f,  73.4162f,  77.7817f,  82.4069f,  87.3071f,  92.4986f,  97.9989f, 103.8262f, 110.0000f, 116.5409f, 123.4708f, // C2-B2
    130.8128f, 138.5913f, 146.8324f, 155.5635f, 164.8138f, 174.6141f, 184.9972f, 195.9977f, 207.6523f, 220.0000f, 233.0819f, 246.9417f, // C3-B3
    261.6256f, 277.1826f, 293.6648f, 311.1270f, 329.6276f, 349.2282f, 369.9944f, 391.9954f, 415.3047f, 440.0000f, 466.1638f, 493.8833f, // C4-B4
    523.2511f, 554.3653f, 587.3295f, 622.2540f, 659.2551f, 698.4565f, 739.9888f, 783.9909f, 830.6094f, 880.0000f, 932.3275f, 987.7666f, // C5-B5
   1046.5023f,1108.7305f,1174.6591f,1244.5079f,1318.5102f,1396.9129f,1479.9777f,1567.9817f,1661.2188f,1760.0000f,1864.6550f,1975.5332f  // C6-B6
};

static const float major_scale_freqs[SCALE_COUNT] = {
    261.6256f, 293.6648f, 329.6276f, 349.2282f, 391.9954f, 440.0000f, 493.8833f, // C4-B4
    523.2511f, 587.3295f, 659.2551f, 698.4565f, 783.9909f, 880.0000f, 987.7666f, // C5-B5
   1046.5023f                                                                    // C6
};

static const float minor_scale_freqs[SCALE_COUNT] = {
    261.6256f, 293.6648f, 311.1270f, 349.2282f, 391.9954f, 415.3047f, 466.1638f, // C4-Bb4
    523.2511f, 587.3295f, 622.2540f, 698.4565f, 783.9909f, 830.6094f, 932.3275f, // C5-Bb5
   1046.5023f                                                                    // C6
};

static inline float find_nearest_note(float freq) {
    if (freq <= note_freqs[0]) return note_freqs[0];
    if (freq >= note_freqs[NOTE_COUNT - 1]) return note_freqs[NOTE_COUNT - 1];
    for (int i = 0; i < NOTE_COUNT - 1; i++) {
        if (freq >= note_freqs[i] && freq < note_freqs[i + 1]) {
            float diff_lo = freq - note_freqs[i];
            float diff_hi = note_freqs[i + 1] - freq;
            return (diff_lo <= diff_hi) ? note_freqs[i] : note_freqs[i + 1];
        }
    }
    return freq;
}

static inline float find_nearest_in_scale(float freq, const float *scale, int count) {
    if (freq <= scale[0]) return scale[0];
    if (freq >= scale[count - 1]) return scale[count - 1];
    for (int i = 0; i < count - 1; i++) {
        if (freq >= scale[i] && freq < scale[i + 1]) {
            float diff_lo = freq - scale[i];
            float diff_hi = scale[i + 1] - freq;
            return (diff_lo <= diff_hi) ? scale[i] : scale[i + 1];
        }
    }
    return freq;
}

#endif
