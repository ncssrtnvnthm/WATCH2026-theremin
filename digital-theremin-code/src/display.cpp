#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <stdio.h>

#include "display.h"
#include "sound.h"
#include "settings.h"

static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire1, -1);

void init_display(void) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) return;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("  Digital"));
    display.println(F("  Theremin"));
    display.display();
}

void display_update(uint16_t dist_vol, uint16_t dist_freq,
                    uint16_t frequency, uint8_t volume, uint8_t muted) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    char buf[22];

    if (muted) {
        display.setCursor(0, 0);
        display.setTextSize(2);
        display.println(F("MUTED"));
        display.setTextSize(1);
    } else {
        display.setCursor(0, 0);
        snprintf(buf, sizeof(buf), "W: %s", wave_names[current_wave]);
        display.println(buf);

        display.setCursor(0, 10);
        snprintf(buf, sizeof(buf), "VD:%3dmm V:%3d%%", dist_vol, volume * 100 / 255);
        display.println(buf);

        display.setCursor(0, 20);
        snprintf(buf, sizeof(buf), "FD:%3dmm %4dHz", dist_freq, frequency);
        display.println(buf);

        display.setCursor(0, 30);
        snprintf(buf, sizeof(buf), "FX: %s", fx_names[current_effect]);
        display.println(buf);

        int fill_w = (int)((long)128 * volume / 255);
        display.drawRect(0, 44, 128, 8, SSD1306_WHITE);
        display.fillRect(1, 45, fill_w > 2 ? fill_w - 2 : 0, 6, SSD1306_WHITE);
    }

    display.display();
}
