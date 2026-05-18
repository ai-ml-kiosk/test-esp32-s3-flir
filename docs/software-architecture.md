# Software Architecture

This project should copy the intent of GoVision's `core/thermal.py` and
`ui/thermal_ui.py`, not the Python implementation directly. The ESP32-S3
firmware should be written as embedded C/C++ components or Arduino/PlatformIO
modules.

## GoVision Logic To Preserve

From `core/thermal.py`:

- FLIR Lepton configuration.
- SPI bus open/init.
- VoSPI packet synchronization.
- Raw 80x60 frame assembly.
- Error handling and resync behavior.
- Optional reset/enable support.
- TLinear raw value conversion to Celsius.

From `ui/thermal_ui.py`:

- Live thermal viewer.
- Color palette mapping.
- Auto low/high percentile range.
- Sensitivity adjustment.
- Hot/cold spot detection.
- Capture control.
- Rotation/zoom state.
- Persisted viewer settings.

## Proposed ESP32-S3 Components

| Component | Responsibility |
|---|---|
| `lepton_vospi` | Dedicated SPI read loop, packet validation, frame assembly, resync. |
| `lepton_cci` | I2C/CCI control, status, optional FFC control, optional reset handling. |
| `thermal_math` | TLinear to Celsius conversion, min/max detection, auto range, palette mapping. |
| `display_driver` | ILI9341 initialization, DMA/SPI drawing, backlight control. |
| `touch_driver` | XPT2046 sampling, calibration, debouncing. |
| `sd_storage` | Optional BMP/raw frame capture and logs. |
| `thermal_ui` | Main screen layout, buttons, rotation/zoom, high/low markers. |
| `settings` | Non-volatile settings in NVS or JSON on SD card. |

## Suggested FreeRTOS Task Model

| Task | Priority | Notes |
|---|---:|---|
| Lepton capture task | High | Owns FLIR SPI bus. Assembles newest full raw frame. |
| UI/render task | Medium | Converts latest raw frame to RGB565 and updates TFT. |
| Touch/input task | Medium/low | Polls XPT2046 or waits for `T_IRQ`. |
| Storage task | Low | Writes captures to SD without blocking capture. |

Use double buffering:

- Buffer A: being filled by Lepton capture.
- Buffer B: latest complete raw frame for rendering.

Avoid letting SD card writes block Lepton VoSPI capture.

## Display Pipeline

1. Capture complete 80x60 Lepton raw frame.
2. Convert raw values to Celsius if TLinear is enabled.
3. Calculate low/high display range.
4. Normalize to palette index.
5. Map palette to RGB565.
6. Scale to display viewport.
7. Draw high/low markers and UI controls.

## First Firmware Milestone

Start with a minimal thermal-only screen:

- No touch controls.
- No SD card writes.
- No Wi-Fi.
- Fixed palette.
- Fixed scale.
- Serial logs for SPI sync and frame rate.

Then add controls in this order:

1. Touch read and calibration.
2. Palette/range controls.
3. Capture to SD.
4. Settings persistence.
5. Optional manual FFC button.

## ESP32-S3 Constraints

- Lepton is low resolution, but VoSPI packet timing is strict.
- ILI9341 full-screen updates over SPI can be expensive. Update only the
  thermal viewport when possible.
- Use RGB565 for display buffers to reduce memory and bandwidth.
- Keep the UI bus and FLIR bus separate to avoid long LCD/SD transactions
  disturbing Lepton capture.
- If Wi-Fi is added later, test whether radio activity affects SPI timing or
  power stability.

