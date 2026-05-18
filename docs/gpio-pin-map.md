# GPIO Pin Map

This is a proposed safe starting map for a generic ESP32-S3 development board.
Verify the exact board schematic before wiring. Some ESP32-S3 boards do not
expose every GPIO, and some pins may be connected to onboard flash, PSRAM, RGB
LEDs, buttons, USB, or UART.

## Avoided Pins

Avoid these pins unless the board schematic says they are safe:

- `GPIO0`: boot/download strapping button on many boards.
- `GPIO3`, `GPIO45`, `GPIO46`: strapping-related pins on ESP32-S3.
- `GPIO19`, `GPIO20`: commonly used for native USB D-/D+.
- `GPIO43`, `GPIO44`: commonly used for UART logging/programming.
- `GPIO48`: commonly used for onboard RGB LED on some ESP32-S3 boards.
- Any board-specific flash/PSRAM pins.

## Proposed Bus Summary

| Function | ESP32-S3 GPIO | Notes |
|---|---:|---|
| UI SPI SCLK | `GPIO12` | Shared by LCD, touch, and SD. |
| UI SPI MOSI | `GPIO11` | Shared by LCD, touch, and SD. |
| UI SPI MISO | `GPIO13` | Required for touch and SD; optional for LCD readback. |
| FLIR SPI SCLK | `GPIO36` | Dedicated Lepton VoSPI clock. |
| FLIR SPI MISO | `GPIO37` | Lepton VoSPI data into ESP32-S3. |
| FLIR SPI MOSI | `GPIO35` | Optional/unused by VoSPI, but wire if breakout expects SPI MOSI. |
| FLIR SPI CS | `GPIO34` | Dedicated Lepton chip-select. |
| FLIR I2C SDA | `GPIO8` | CCI/control bus. |
| FLIR I2C SCL | `GPIO18` | CCI/control bus. |

## Display, Touch, And SD Wiring

Assumed display module: 2.8 inch 240x320 SPI TFT with ILI9341 LCD, XPT2046
touch, and microSD support.

| Module signal | ESP32-S3 GPIO | Direction | Notes |
|---|---:|---|---|
| VCC | 3.3V or module-supported 5V | Power | Prefer 3.3V if supported. Use 5V only if the display module explicitly supports it. |
| GND | GND | Power | Common ground with ESP32-S3 and FLIR. |
| LCD SCK / T_CLK / SD_SCK | `GPIO12` | ESP32-S3 to module | Shared UI SPI clock. |
| LCD SDI / T_DIN / SD_MOSI | `GPIO11` | ESP32-S3 to module | Shared UI SPI MOSI. |
| LCD SDO / T_DO / SD_MISO | `GPIO13` | Module to ESP32-S3 | Shared UI SPI MISO. |
| LCD CS | `GPIO10` | ESP32-S3 to LCD | LCD chip-select. |
| LCD DC / RS | `GPIO9` | ESP32-S3 to LCD | Data/command select. |
| LCD RESET | `GPIO14` | ESP32-S3 to LCD | LCD reset. Can be tied to ESP32 reset if pins are scarce. |
| LCD LED / BL | `GPIO21` | ESP32-S3 to display | PWM backlight control if module supports GPIO-level backlight control. |
| Touch T_CS | `GPIO15` | ESP32-S3 to touch | XPT2046 chip-select. |
| Touch T_IRQ | `GPIO16` | Touch to ESP32-S3 | Optional touch interrupt; can poll if not wired. |
| SD CS | `GPIO17` | ESP32-S3 to SD | microSD chip-select. |

## FLIR Lepton Wiring

Assumed FLIR breakout exposes SPI/VoSPI and I2C/CCI at 3.3V logic.

| FLIR breakout signal | ESP32-S3 GPIO | Direction | Notes |
|---|---:|---|---|
| VIN / VCC | 3.3V | Power | Confirm breakout voltage requirements. |
| GND | GND | Power | Common ground. |
| SPI SCK | `GPIO36` | ESP32-S3 to FLIR | Dedicated VoSPI clock. |
| SPI MISO / VoSPI data | `GPIO37` | FLIR to ESP32-S3 | Thermal packet stream. |
| SPI MOSI | `GPIO35` | ESP32-S3 to FLIR | Optional for many VoSPI breakouts; safe to allocate. |
| SPI CS | `GPIO34` | ESP32-S3 to FLIR | Dedicated chip-select. |
| CCI SDA | `GPIO8` | Bidirectional | I2C data, use pullups if breakout lacks them. |
| CCI SCL | `GPIO18` | ESP32-S3 to FLIR | I2C clock, use pullups if breakout lacks them. |
| RESET / RST / EN | `GPIO38` | ESP32-S3 to FLIR | Optional reset/enable line if available. |
| PWR_EN | `GPIO39` | ESP32-S3 to FLIR | Optional power-enable line if available. |

## Alternate Pin Strategy

If `GPIO34` through `GPIO39` are unavailable on the chosen board, keep the same
bus separation but move the FLIR bus to another free set of GPIOs. ESP32-S3 can
route SPI signals through the GPIO matrix, but high-speed Lepton capture is more
sensitive than the LCD UI bus. Prioritize short wiring and stable pins for:

1. FLIR `SCK`
2. FLIR `MISO`
3. FLIR `CS`
4. FLIR I2C `SDA/SCL`

