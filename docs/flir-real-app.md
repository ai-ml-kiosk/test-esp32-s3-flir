# FLIR Lepton Real-Time App

## System Architecture Overview

The firmware is split into shared board support, demo code, and real FLIR code:

- `include/pin_config.h`: board pin assignments for LCD, touch, SD, and FLIR.
- `include/display_driver.h` and `src/display_driver.cpp`: ILI9341 LCD plus XPT2046 touch configuration.
- `src/demo/`: simulated thermal UI.
- `src/flir/`: real Lepton acquisition, image processing, rendering, touch capture, and SD saving.
- `src/main.cpp`: selects either demo mode or real FLIR mode at build time with `FLIR_REAL_APP`.

The LCD, XPT2046 touch controller, and microSD card share the UI SPI pins:

```text
UI SCLK GPIO12
UI MOSI GPIO11
UI MISO GPIO13
LCD CS  GPIO10
TOUCH CS GPIO15
SD CS   GPIO17
```

SPI devices avoid bus collisions by using independent chip-select lines. Only one chip select should be low at a time. The firmware sets `LCD_CS`, `TOUCH_CS`, and `SD_CS` high before mounting the SD card, and LovyanGFX is configured with `bus_shared = true` for the LCD and touch controller. The SD code uses the Arduino `SD` library on the same UI SPI pins, so SD writes should happen when the app is not actively pushing pixels or reading touch.

The connected camera is a FLIR Lepton 2.5 on breakout board v1.4. It uses a
separate SPI bus for VoSPI so the high-rate thermal stream does not contend with
LCD rendering:

```text
FLIR CLK/SCLK GPIO4
FLIR MOSI GPIO6
FLIR MISO GPIO5
FLIR CS   GPIO7
CCI SDA   GPIO8
CCI SCL   GPIO18
PWR_EN    not connected on breakout v1.4
RST       not connected on breakout v1.4
```

On many Lepton breakout boards, the pin labeled `CLK` is the VoSPI/SPI clock.
Wire that `CLK` pin to `GPIO4`. Do not confuse it with `SCL`: `SCL` is the
I2C/CCI control clock and should go to `GPIO18`. `SDA` is the I2C/CCI data line
and should go to `GPIO8`.

For this build, wire FLIR `MOSI` to `GPIO6` as a required Lepton SPI signal.
The firmware initializes the dedicated FLIR SPI bus with SCLK, MISO, MOSI, and
CS so the breakout wiring matches the configured bus exactly.

Avoid wiring Lepton VoSPI to `GPIO35`, `GPIO36`, `GPIO37`, or `GPIO39` on this
ESP32-S3 N16R8 board. Those pins can be associated with flash/PSRAM-related
FSPI/sub-SPI functions and may cause boot watchdog resets when externally
driven.

The current code implements VoSPI frame reads. CCI control over I2C is reserved by pin map and can be added later for telemetry mode, FFC, and camera configuration.
Because breakout v1.4 does not expose dedicated power-enable or reset pins, the
firmware leaves `FLIR_POWER_ENABLE` and `FLIR_RESET` disabled in
`include/pin_config.h`.

## Data Pipeline Flowchart

```text
Lepton VoSPI packets
        |
        v
LeptonDriver::readFrame()
  - reads 60 packets
  - rejects discard packets
  - detects packet order loss
  - resyncs on timeout/sync loss
        |
        v
raw 14-bit frame, 80x60 uint16_t
        |
        +--> ImageProcessor::calculateSpotTemperatures()
        |     - min raw / max raw
        |     - center pixel raw
        |     - Celsius conversion
        |
        v
ImageProcessor::automaticGainControl()
  - finds min/max raw values
  - normalizes 14-bit values to 0-255
        |
        v
8-bit frame, 80x60 uint8_t
        |
        v
ImageProcessor::upscaleBilinear()
  - interpolates 80x60 into 240x320
        |
        v
8-bit display frame, 240x320 uint8_t
        |
        +--> CaptureStorage::saveCapture()
        |     - writes raw 14-bit .raw
        |     - writes 8-bit grayscale .bmp
        |
        v
ImageProcessor::applyPalette()
  - Ironbow
  - White Hot
  - Black Hot
  - selectable at runtime with Serial command "p"
        |
        v
RGB565 render buffer, 240x320 uint16_t
        |
        v
Display::pushImage()
        |
        v
ILI9341 LCD with min/max/center overlay
```

## Build Modes

Real FLIR mode is now the project default:

```bash
pio run
```

Upload real FLIR mode:

```bash
pio run --target upload
```

The simulated demo is still available explicitly:

```bash
pio run -e esp32-s3-n16r8-demo
```

In real FLIR mode, the Serial Monitor accepts:

```text
p  cycle palette: Ironbow -> White Hot -> Black Hot
```

## API Reference

### `ImageProcessor::automaticGainControl`

```cpp
bool automaticGainControl(const uint16_t* raw14,
                          size_t pixelCount,
                          uint8_t* out8,
                          uint16_t* minRaw = nullptr,
                          uint16_t* maxRaw = nullptr) const;
```

Normalizes a raw 14-bit radiometric frame into an 8-bit image. It scans the input to find the frame minimum and maximum, then maps the range linearly to `0..255`.

- `raw14`: input array of 14-bit Lepton values stored in `uint16_t`.
- `pixelCount`: number of pixels to process.
- `out8`: output 8-bit normalized buffer.
- `minRaw`: optional output for minimum raw value.
- `maxRaw`: optional output for maximum raw value.
- Returns `true` on success, `false` for invalid buffers.

### `ImageProcessor::upscaleBilinear`

```cpp
bool upscaleBilinear(const uint8_t* src,
                     uint16_t srcWidth,
                     uint16_t srcHeight,
                     uint8_t* dst,
                     uint16_t dstWidth,
                     uint16_t dstHeight) const;
```

Upscales an 8-bit thermal image using bilinear interpolation. The real FLIR app calls this with `80x60` input and `240x320` output.

- `src`: normalized source image.
- `srcWidth`: source width in pixels.
- `srcHeight`: source height in pixels.
- `dst`: destination buffer.
- `dstWidth`: output width in pixels.
- `dstHeight`: output height in pixels.
- Returns `true` on success, `false` for invalid dimensions or buffers.

### `ImageProcessor::applyPalette`

```cpp
void applyPalette(const uint8_t* normalized,
                  size_t pixelCount,
                  uint16_t* rgb565Out,
                  PaletteMode palette) const;
```

Converts 8-bit intensity values to RGB565 pixels for the LCD.

- `normalized`: input 8-bit image.
- `pixelCount`: number of pixels.
- `rgb565Out`: output RGB565 buffer for `pushImage`.
- `palette`: `PaletteMode::Ironbow`, `PaletteMode::WhiteHot`, or `PaletteMode::BlackHot`.

### `ImageProcessor::calculateSpotTemperatures`

```cpp
SpotTemperatures calculateSpotTemperatures(const uint16_t* raw14,
                                           uint16_t width,
                                           uint16_t height) const;
```

Calculates minimum, maximum, and center-pixel temperatures. Raw Lepton values are converted using `C = raw * 0.01 - 273.15`, which matches radiometric Kelvin centi-degree output.

### `LeptonDriver::readFrame`

```cpp
LeptonStatus readFrame(uint16_t* raw14, size_t pixelCount);
```

Reads one 80x60 Lepton frame over VoSPI.

- Returns `LeptonStatus::Ok` on success.
- Returns `Timeout` if a full frame is not received within the frame window.
- Returns `SyncLost` if discard packets or packet ordering indicate VoSPI sync loss.
- Returns `SpiError` for invalid buffers.

### `CaptureStorage::saveCapture`

```cpp
bool saveCapture(const uint16_t* raw14,
                 size_t rawPixelCount,
                 const uint8_t* bitmap8,
                 uint16_t bitmapWidth,
                 uint16_t bitmapHeight);
```

Saves two files to `/flir` on the SD card:

- `frame_00001.raw`: raw radiometric 14-bit values stored as little-endian `uint16_t`.
- `frame_00001.bmp`: 8-bit grayscale BMP generated from the processed display buffer.

Returns `true` only if both files are written successfully.

## Touch Capture Behavior

The app attaches an interrupt to `TOUCH_IRQ`. The ISR only sets a `volatile` flag. The main loop consumes that flag with debounce logic, so SD writes and display updates never run inside interrupt context.

When tapped:

1. The current frame is paused.
2. The raw 14-bit frame and 8-bit BMP are written to SD.
3. The frozen frame remains on screen.
4. A second tap resumes live rendering.

## Troubleshooting Guide

### VoSPI Synchronization Loss

Symptoms:

- Serial prints `Lepton sync loss`.
- Serial prints `frame timeout waiting for VoSPI packets`.
- The LCD stops updating or skips frames.

Likely causes:

- FLIR CS, SCLK, MISO, or ground is not connected correctly.
- SPI mode or clock is incompatible with the module wiring.
- The Lepton has not completed boot before reads start.
- Packet reads started mid-frame and the stream needs resync.

What the firmware does:

- Rejects discard packets where the packet ID nibble is `0x0F`.
- Verifies packet numbers arrive sequentially from `0..59`.
- Holds CS high and waits about `185ms` in `LeptonDriver::resync()`.
- Prints descriptive errors to Serial.

Suggested checks:

- Confirm `FLIR_SPI_CS`, `FLIR_SPI_SCLK`, `FLIR_SPI_MISO`, and `GND`.
- Confirm the Lepton breakout voltage requirement.
- Lower `kSpiFrequency` in `lepton_driver.h` if wiring is long or noisy.
- Add CCI initialization once the module is connected if telemetry/radiometric mode needs explicit configuration.

### SD Card Write Failures

Symptoms:

- Serial prints `SD error: failed to mount card`.
- Serial prints `unable to open ... for BMP write`.
- Capture tap pauses the display but no files appear on the card.

Likely causes:

- SD card CS wiring does not match `Pins::SD_CS`.
- LCD, touch, or SD chip-select is stuck low.
- Card is not formatted as FAT32.
- Shared SPI bus wiring has MISO/MOSI/SCLK swapped.
- Card cannot sustain writes or is write-protected.

What the firmware does:

- Forces LCD, touch, and SD chip selects high before SD mount.
- Creates `/flir` on mount.
- Checks file open and write errors for both `.raw` and `.bmp`.
- Leaves capture disabled if mount fails.

Suggested checks:

- Verify `SD_CS = GPIO17` or update `pin_config.h`.
- Use a known-good FAT32 microSD card.
- Confirm `LCD_CS`, `TOUCH_CS`, and `SD_CS` are not shorted together.
- Temporarily reduce SD SPI speed in `CaptureStorage::begin()`.
