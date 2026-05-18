#pragma once

#include <Arduino.h>

namespace Pins {

// Shared UI SPI bus for ILI9341 LCD, XPT2046 touch, and future microSD.
static constexpr int UI_SPI_SCLK = 12;
static constexpr int UI_SPI_MOSI = 11;
static constexpr int UI_SPI_MISO = 13;

// ILI9341 LCD controls.
static constexpr int LCD_CS = 10;
static constexpr int LCD_DC = 9;
static constexpr int LCD_RST = 14;
static constexpr int LCD_BACKLIGHT = 21;

// XPT2046 touch controller.
static constexpr int TOUCH_CS = 15;
static constexpr int TOUCH_IRQ = 16;

// Optional microSD chip-select on the same UI SPI bus.
static constexpr int SD_CS = 17;

// Reserved for the future FLIR Lepton module.
static constexpr int FLIR_SPI_SCLK = 36;
static constexpr int FLIR_SPI_MOSI = 35;
static constexpr int FLIR_SPI_MISO = 37;
static constexpr int FLIR_SPI_CS = 39;
static constexpr int FLIR_CCI_SDA = 8;
static constexpr int FLIR_CCI_SCL = 18;
static constexpr int FLIR_RESET = 2;
static constexpr int FLIR_POWER_ENABLE = 1;

}  // namespace Pins
