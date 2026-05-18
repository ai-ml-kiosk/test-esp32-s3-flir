# test-esp32-s3-flir

Design-doc staging project for porting the GoVision FLIR Lepton viewer logic to
an ESP32-S3 board with a 2.8 inch 240x320 SPI touch display and microSD slot.

Status: design documents only. No firmware code has been generated yet.

## Scope

- Target MCU: ESP32-S3 development board.
- Thermal sensor: FLIR Lepton 2.5 style module or breakout.
- Display: 2.8 inch 240x320 SPI TFT, assumed ILI9341-compatible.
- Touch: assumed XPT2046 resistive touch controller over SPI.
- Storage: microSD on SPI.
- Goal: port the relevant ideas from GoVision `core/thermal.py` and
  `ui/thermal_ui.py` into an embedded ESP32-S3 firmware architecture.

## Document Map

- [Hardware Design](docs/hardware-design.md): electrical assumptions, bus
  allocation, and power notes.
- [GPIO Pin Map](docs/gpio-pin-map.md): proposed ESP32-S3 GPIO assignments for
  display, touch, SD card, and FLIR.
- [Software Architecture](docs/software-architecture.md): firmware component
  plan based on the GoVision thermal capture and viewer logic.
- [Validation Plan](docs/validation-plan.md): bring-up order and tests before
  adding full UI features.
- [Sources](docs/sources.md): external references used for the design notes.

## Important Assumptions

This design assumes a generic ESP32-S3 dev board with enough exposed GPIOs. Many
ESP32-S3 boards reserve or omit some pins, so verify the exact board schematic
before wiring.

The pin map uses separate SPI buses:

- UI SPI bus for ILI9341 LCD, XPT2046 touch, and microSD.
- FLIR VoSPI bus dedicated to Lepton frame capture.

That separation is intentional because Lepton VoSPI timing is sensitive and the
display/SD card can create long SPI transactions.

