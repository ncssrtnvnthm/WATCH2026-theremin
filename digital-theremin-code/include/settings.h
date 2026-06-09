#ifndef SETTINGS_H
#define SETTINGS_H

// Audio PWM output on GPIO5+6 (PWM2 + PWM1 per schematic)
#define soundIO1 6
#define soundIO2 5

// I2C1 for VL53L0X sensors and OLED (GPIO18=SDA, GPIO19=SCL)
#define I2CInst i2c1
#define I2C_SDA_PIN      18
#define I2C_SCL_PIN      19
#define I2C_BAUDRATE     400000

// VL53L0X sensor addresses after XSHUT reassignment
#define I2C_TOF_VOL_ADDR  0x29
#define I2C_TOF_FREQ_ADDR 0x30

// XSHUT control for volume sensor
#define XSHUT_VOL_PIN     20

// Push buttons (internal pull-up, short to GND when pressed)
#define BUTT1_PIN 16
#define BUTT2_PIN 17
#define BUTTON_DEBOUNCE_MS 200

// OLED display (SSD1306, 128x64, I2C)
#define OLED_ADDR  0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// System clock (250 MHz overclock)
#define PICOCLOCK 250000

#endif
