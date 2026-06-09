#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void init_display(void);
void display_update(uint16_t dist_vol, uint16_t dist_freq,
                    uint16_t frequency, uint8_t volume, uint8_t muted);

#ifdef __cplusplus
}
#endif

#endif
