# Hardware Design

## Target Hardware

The intended hardware stack is:

- ESP32-S3 development board.
- FLIR Lepton 2.5 thermal module on breakout board v1.4. This breakout provides
  the required power rails plus SPI/VoSPI and I2C/CCI pins, but does not expose
  separate FLIR `PWR_EN` or `RST` pins.
- 2.8 inch 240x320 SPI TFT display, assumed ILI9341-compatible.
- XPT2046 resistive touch controller, usually sharing the display SPI bus.
- microSD socket, usually sharing SPI with its own chip-select.

## Electrical Rules

- Use 3.3V logic for all ESP32-S3 GPIO, SPI, I2C, reset, and interrupt lines.
- Do not feed 5V logic into ESP32-S3 pins.
- Display VCC may be 5V on some modules if the module includes a regulator, but
  the SPI/touch/SD signal lines must still be 3.3V logic.
- FLIR Lepton 2.5 breakout v1.4 is treated as a 3.3V-class interface in this
  project. Confirm your board markings before applying power.
- All modules must share a common ground.
- Use short wiring for FLIR VoSPI and display SPI, especially SCLK and MISO.

## Bus Allocation

Use two SPI buses:

| Bus | Purpose | Devices | Reason |
|---|---|---|---|
| UI SPI | Display/touch/storage | ILI9341, XPT2046, microSD | These devices can share SCLK/MOSI/MISO with separate chip-select lines. |
| FLIR SPI | Thermal frame capture | FLIR Lepton VoSPI | Keeps Lepton packet timing isolated from LCD and SD transactions. |

Use one I2C bus:

| Bus | Purpose | Devices |
|---|---|---|
| FLIR I2C/CCI | Lepton control/status | FLIR Lepton CCI address, typically `0x2A` |

## Power Budget Notes

- ESP32-S3 Wi-Fi, TFT backlight, SD writes, and the Lepton module can create
  burst current. Use a stable 5V input to the ESP32-S3 board and confirm the
  board's 3.3V regulator can support external modules.
- If the display backlight is bright or warm, drive its LED/backlight pin
  through the module's supported input or an external transistor/MOSFET rather
  than directly from a weak GPIO.
- If thermal frames fail when the display backlight or SD card is active, test
  with the display backlight disabled and SD removed to isolate power dips from
  SPI timing problems.

## Mechanical And Optical Notes

- The Lepton image is 80x60 and low frame-rate compared with the display.
- The UI should scale the thermal image to fit 240x320 while preserving aspect
  ratio or deliberately using a fill mode.
- If a visible camera is added later, treat thermal/visible alignment as a
  per-device calibration problem. For this first ESP32-S3 project, the scope is
  thermal-only display.
