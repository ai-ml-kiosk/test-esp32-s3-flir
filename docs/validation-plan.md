# Validation Plan

## Phase 1: Electrical Bring-Up

1. Power ESP32-S3 alone and confirm serial logging.
2. Connect TFT power and UI SPI only.
3. Render a solid-color test pattern to the ILI9341.
4. Add XPT2046 touch and print raw touch coordinates.
5. Add SD card and run read/write tests.
6. Add FLIR power and I2C only; scan for CCI address.
7. Add FLIR VoSPI and attempt packet sync.
8. Initialize the FLIR Lepton 2.5, handling the synchronization of VoSPI packets (discarding 0x0F packets) and handling periodic FFC (Flat Field Correction) shutter clicks.

## Phase 2: FLIR Frame Capture

Expected checks:

- SPI mode matches Lepton breakout requirements.
- Packet size is 164 bytes for Lepton 2.x VoSPI.
- Frame is 80x60.
- Frame rate is around Lepton's normal rate, roughly 8.6Hz for common export
  modules.
- Packet discard/resync behavior recovers after temporary sync loss.

## Phase 3: Display Rendering

Expected checks:

- Thermal image appears with correct orientation.
- Hot/cold markers follow a warm object.
- Color scale remains stable enough for practical viewing.
- UI remains responsive while capture continues.

## Phase 4: Touch And Storage

Expected checks:

- Touch coordinates map correctly after calibration.
- Touch does not interfere with LCD drawing.
- SD writes do not stall Lepton capture.
- Captures include timestamp or monotonic sequence number.

## Phase 5: Stability Test

Run for at least 30 minutes:

- No watchdog resets.
- No permanent VoSPI sync loss.
- No display white-screen or SPI bus lockups.
- Thermal frame rate remains stable.
- ESP32-S3 regulator and display backlight do not overheat.

