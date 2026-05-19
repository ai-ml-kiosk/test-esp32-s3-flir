#include "flir/flir_app.h"

#ifndef FLIR_REAL_APP
#define FLIR_REAL_APP 0
#endif

#if FLIR_REAL_APP

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "flir/capture_storage.h"
#include "display_driver.h"
#include "flir/image_processing.h"
#include "flir/lepton_cci.h"
#include "flir/lepton_driver.h"
#include "pin_config.h"
#include "flir/thermal_types.h"

namespace {

using flir::ImageProcessor;
using flir::LeptonCci;
using flir::LeptonDriver;
using flir::LeptonStatus;
using flir::PaletteMode;
using flir::SpotTemperatures;

constexpr uint32_t kFrameIntervalMs = 111;
constexpr uint32_t kTouchDebounceMs = 220;
constexpr uint16_t kPanelFill = 0x0841;
constexpr uint16_t kPanelBorder = 0x7BEF;
constexpr uint16_t kButtonFill = 0x31A6;
constexpr uint16_t kButtonPressed = 0x4A89;
constexpr uint16_t kGreen = 0x57EA;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kAmber = 0xFD20;
constexpr uint16_t kOverlayShadow = 0x0000;
constexpr uint32_t kOverlayIntervalMs = 500;
constexpr uint32_t kPausedTouchPollMs = 80;
constexpr uint32_t kTemperatureDiagnosticIntervalMs = 1000;
constexpr uint8_t kHistogramBins = 64;

struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char* label;
};

enum class TouchAction : uint8_t {
  None,
  Capture,
  Pause,
  Palette,
};

volatile bool g_touchIrq = false;

void IRAM_ATTR onTouchIrq() {
  g_touchIrq = true;
}

Display* display = nullptr;
LeptonCci* cci = nullptr;
LeptonDriver* lepton = nullptr;
ImageProcessor* processor = nullptr;
flir::CaptureStorage* storage = nullptr;

uint16_t* rawFrame = nullptr;
uint8_t* normalizedFrame = nullptr;
uint8_t* denoisedFrame = nullptr;
uint8_t* upscaledFrame = nullptr;
uint16_t* renderFrame = nullptr;

PaletteMode palette = PaletteMode::Ironbow;
bool paused = false;
bool hasFrame = false;
bool cciPresent = false;
bool sdReady = false;
bool captureInProgress = false;
uint32_t lastFrameMs = 0;
uint32_t lastTouchMs = 0;
uint32_t framesRendered = 0;
uint32_t lastFpsMs = 0;
uint32_t lastStatusUntilMs = 0;
uint32_t syncLossCount = 0;
uint32_t timeoutCount = 0;
uint32_t rejectedFrameCount = 0;
uint32_t displayedFrameCount = 0;
uint32_t partialFrameDropCount = 0;
uint8_t skipFramesAfterFfc = 0;
uint32_t lastOverlayMs = 0;
uint32_t lastPausedTouchPollMs = 0;
uint32_t lastTouchPollMs = 0;
uint32_t lastTemperatureDiagnosticMs = 0;
float measuredFps = 0.0f;
SpotTemperatures spots;
String statusLine = "Booting";

Button paletteButton{6, 282, 72, 30, "MODE"};
Button pauseButton{84, 282, 72, 30, "PAUSE"};
Button captureButton{162, 282, 72, 30, "CAP"};

void renderCurrentFrame();
void drawOverlay();
void setStatus(const String& message, uint32_t durationMs);

bool constructAppObjects() {
  display = new Display();
  cci = new LeptonCci();
  lepton = new LeptonDriver();
  processor = new ImageProcessor();
  storage = new flir::CaptureStorage();
  if (display == nullptr || cci == nullptr || lepton == nullptr || processor == nullptr || storage == nullptr) {
    Serial.println("Startup error: failed to construct real FLIR app objects");
    return false;
  }
  Serial.println("Real FLIR app objects constructed");
  return true;
}

template <typename T>
T* allocateFrameBuffer(size_t count, const char* name, uint32_t caps) {
  T* buffer = static_cast<T*>(heap_caps_calloc(count, sizeof(T), caps));
  if (buffer == nullptr && (caps & MALLOC_CAP_SPIRAM) != 0) {
    buffer = static_cast<T*>(heap_caps_calloc(count, sizeof(T), MALLOC_CAP_8BIT));
  }
  if (buffer == nullptr) {
    Serial.printf("Memory error: failed to allocate %s (%lu bytes)\n",
                  name,
                  static_cast<unsigned long>(count * sizeof(T)));
  } else {
    Serial.printf("Allocated %s: %lu bytes\n", name, static_cast<unsigned long>(count * sizeof(T)));
  }
  return buffer;
}

bool allocateFrameBuffers() {
  rawFrame = allocateFrameBuffer<uint16_t>(flir::kLeptonPixels, "rawFrame", MALLOC_CAP_8BIT);
  normalizedFrame = allocateFrameBuffer<uint8_t>(flir::kLeptonPixels, "normalizedFrame", MALLOC_CAP_8BIT);
  denoisedFrame = allocateFrameBuffer<uint8_t>(flir::kLeptonPixels, "denoisedFrame", MALLOC_CAP_8BIT);
  upscaledFrame = allocateFrameBuffer<uint8_t>(flir::kThermalViewPixels, "upscaledFrame", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  renderFrame = allocateFrameBuffer<uint16_t>(flir::kThermalViewPixels, "renderFrame", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return rawFrame != nullptr && normalizedFrame != nullptr && denoisedFrame != nullptr && upscaledFrame != nullptr &&
         renderFrame != nullptr;
}

const char* paletteName(PaletteMode mode) {
  switch (mode) {
    case PaletteMode::Ironbow:
      return "Ironbow";
    case PaletteMode::WhiteHot:
      return "White Hot";
    case PaletteMode::BlackHot:
      return "Black Hot";
    case PaletteMode::Histogram:
      return "Histogram";
    default:
      return "Unknown";
  }
}

void cyclePalette() {
  switch (palette) {
    case PaletteMode::Ironbow:
      palette = PaletteMode::WhiteHot;
      break;
    case PaletteMode::WhiteHot:
      palette = PaletteMode::BlackHot;
      break;
    case PaletteMode::BlackHot:
      palette = PaletteMode::Histogram;
      break;
    case PaletteMode::Histogram:
    default:
      palette = PaletteMode::Ironbow;
      break;
  }
  Serial.printf("Palette selected: %s\n", paletteName(palette));
  setStatus(String("Palette: ") + paletteName(palette), 900);
  if (hasFrame) {
    renderCurrentFrame();
  } else {
    drawOverlay();
  }
}

bool contains(const Button& button, uint16_t x, uint16_t y) {
  return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}

bool containsExpanded(const Button& button, uint16_t x, uint16_t y, uint16_t padding) {
  return x + padding >= button.x && x < button.x + button.w + padding && y + padding >= button.y &&
         y < button.y + button.h + padding;
}

TouchAction bottomButtonAction(uint16_t x, uint16_t y) {
  if (y < flir::kControlPanelY || y >= flir::kDisplayHeight) {
    return TouchAction::None;
  }
  if (x < flir::kDisplayWidth / 3) {
    return TouchAction::Palette;
  }
  if (x < (flir::kDisplayWidth * 2) / 3) {
    return TouchAction::Pause;
  }
  return TouchAction::Capture;
}

TouchAction bottomTouchAction(uint16_t rawX, uint16_t rawY, uint16_t* mappedX = nullptr, uint16_t* mappedY = nullptr) {
  struct Candidate {
    uint16_t x;
    uint16_t y;
  };

  const Candidate candidates[] = {
      // XPT2046 reports the portrait bottom bar as a rotated coordinate pair
      // on this board. rawY maps to horizontal position but is reversed
      // relative to the visible left-to-right button labels.
      {static_cast<uint16_t>(flir::kDisplayWidth - 1 - min<uint16_t>(rawY, flir::kDisplayWidth - 1)),
       static_cast<uint16_t>(flir::kDisplayHeight - 1 - min<uint16_t>(rawX, flir::kDisplayHeight - 1))},
      {rawX, rawY},
      {rawY, rawX},
  };

  for (const Candidate& candidate : candidates) {
    if (candidate.x >= flir::kDisplayWidth || candidate.y >= flir::kDisplayHeight) {
      continue;
    }
    const TouchAction action = bottomButtonAction(candidate.x, candidate.y);
    if (action == TouchAction::None) {
      continue;
    }
    if (mappedX != nullptr) {
      *mappedX = candidate.x;
    }
    if (mappedY != nullptr) {
      *mappedY = candidate.y;
    }
    return action;
  }

  return TouchAction::None;
}

TouchAction resolveTouchAction(uint16_t rawX, uint16_t rawY, uint16_t* mappedX, uint16_t* mappedY) {
  if (rawY < flir::kThermalViewY && rawX >= flir::kDisplayWidth / 2) {
    return TouchAction::None;
  }

  return bottomTouchAction(rawX, rawY, mappedX, mappedY);
}

void setStatus(const String& message, uint32_t durationMs = 1500) {
  statusLine = message;
  lastStatusUntilMs = millis() + durationMs;
  Serial.println(message);
  Serial0.println(message);
}

void bootLog(const char* message) {
  Serial.println(message);
  Serial0.println(message);
  Serial.flush();
  Serial0.flush();
  delay(20);
}

void drawButton(const Button& button, bool active, const char* overrideLabel = nullptr) {
  const uint16_t fill = active ? kButtonPressed : kButtonFill;
  const char* label = overrideLabel != nullptr ? overrideLabel : button.label;
  display->fillRect(button.x, button.y, button.w, button.h, fill);
  display->drawRect(button.x, button.y, button.w, button.h, active ? kGreen : kPanelBorder);
  display->setTextDatum(textdatum_t::middle_center);
  display->setTextSize(1);
  display->setFont(&fonts::Font2);
  display->setTextColor(TFT_WHITE, fill);
  display->drawString(label, button.x + button.w / 2, button.y + button.h / 2);
  display->setFont(&fonts::Font0);
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());
    if (command == 'p' || command == 'P') {
      cyclePalette();
    } else if (command == 'f' || command == 'F') {
      if (cciPresent && cci->runFfc()) {
        skipFramesAfterFfc = 6;
        setStatus("FFC requested", 1200);
      } else {
        setStatus("FFC failed", 1200);
      }
    }
  }
}

bool consumeTouchTap() {
  if (!g_touchIrq) {
    return false;
  }

  noInterrupts();
  g_touchIrq = false;
  interrupts();

  const uint32_t now = millis();
  if (now - lastTouchMs < kTouchDebounceMs) {
    return false;
  }
  lastTouchMs = now;
  return true;
}

void drawOverlay() {
  display->setTextDatum(textdatum_t::top_left);
  display->setTextSize(1);
  display->setFont(&fonts::Font2);
  display->setTextColor(TFT_WHITE, kOverlayShadow);
  display->fillRect(0, 0, flir::kDisplayWidth, flir::kTopOverlayHeight, kOverlayShadow);
  display->drawString(String(paused ? "FLIR PAUSED" : "FLIR LIVE") + " 80x60>" + String(flir::kThermalViewWidth) + "x" +
                          String(flir::kThermalViewHeight),
                      6,
                      4);
  display->drawRightString(paletteName(palette), flir::kDisplayWidth - 6, 4);
  display->drawString("FPS " + String(measuredFps, 1), 6, 20);
  display->setTextColor(sdReady ? kGreen : kRed, kOverlayShadow);
  display->drawString(sdReady ? "SD" : "NO SD", 72, 20);
  display->setTextColor(cciPresent ? kGreen : kAmber, kOverlayShadow);
  display->drawString(cciPresent ? "CCI" : "NO CCI", 122, 20);

  display->fillRect(0, flir::kControlPanelY, flir::kDisplayWidth, flir::kControlPanelHeight, kPanelFill);
  display->setTextColor(TFT_WHITE, kPanelFill);
  if (millis() < lastStatusUntilMs) {
    display->drawString(statusLine, 6, 250);
  } else {
    display->drawString("S " + String(syncLossCount) + " T " + String(timeoutCount) + " R " + String(rejectedFrameCount),
                        6,
                        250);
  }
  drawButton(captureButton, captureInProgress);
  drawButton(pauseButton, paused, paused ? "RESUME" : "PAUSE");
  drawButton(paletteButton, false);
  display->setFont(&fonts::Font0);
}

void drawTemperatureOverlay() {
  if (!hasFrame) {
    return;
  }
  const uint16_t y = flir::kThermalViewY + flir::kThermalViewHeight - 18;
  display->fillRect(flir::kThermalViewX, y, flir::kThermalViewWidth, 18, TFT_BLACK);
  display->setTextDatum(textdatum_t::top_left);
  display->setTextSize(1);
  display->setFont(&fonts::Font2);
  display->setTextColor(TFT_WHITE, TFT_BLACK);
  display->drawString("LO " + String(spots.minC, 1) + "C", 6, y + 1);
  display->drawString("HI " + String(spots.maxC, 1) + "C", 82, y + 1);
  display->drawString("MID " + String(spots.centerC, 1) + "C", 158, y + 1);
  display->setFont(&fonts::Font0);
}

uint16_t leptonXToScreen(uint16_t leptonX) {
  return flir::kThermalViewX +
         min<uint16_t>((static_cast<uint32_t>(leptonX) * flir::kThermalViewWidth) / flir::kLeptonWidth +
                           (flir::kThermalViewWidth / flir::kLeptonWidth) / 2,
                       flir::kThermalViewWidth - 1);
}

uint16_t leptonYToScreen(uint16_t leptonY) {
  return flir::kThermalViewY +
         min<uint16_t>((static_cast<uint32_t>(leptonY) * flir::kThermalViewHeight) / flir::kLeptonHeight +
                           (flir::kThermalViewHeight / flir::kLeptonHeight) / 2,
                       flir::kThermalViewHeight - 1);
}

void drawExtremaMarkers() {
  if (!hasFrame) {
    return;
  }

  const uint16_t minX = leptonXToScreen(spots.minX);
  const uint16_t minY = leptonYToScreen(spots.minY);
  const uint16_t maxX = leptonXToScreen(spots.maxX);
  const uint16_t maxY = leptonYToScreen(spots.maxY);

  display->drawCircle(minX, minY, 5, TFT_CYAN);
  display->drawLine(minX - 7, minY, minX + 7, minY, TFT_CYAN);
  display->drawLine(minX, minY - 7, minX, minY + 7, TFT_CYAN);
  display->setTextColor(TFT_CYAN, TFT_BLACK);
  display->drawString("L", min<uint16_t>(minX + 7, flir::kDisplayWidth - 10), max<int16_t>(flir::kThermalViewY, minY - 8));

  display->drawCircle(maxX, maxY, 5, TFT_RED);
  display->drawLine(maxX - 7, maxY, maxX + 7, maxY, TFT_RED);
  display->drawLine(maxX, maxY - 7, maxX, maxY + 7, TFT_RED);
  display->setTextColor(TFT_RED, TFT_BLACK);
  display->drawString("H", min<uint16_t>(maxX + 7, flir::kDisplayWidth - 10), max<int16_t>(flir::kThermalViewY, maxY - 8));
}

uint16_t histogramBarColor(uint8_t bin) {
  if (bin < 18) {
    return TFT_CYAN;
  }
  if (bin < 42) {
    return TFT_GREEN;
  }
  if (bin < 56) {
    return kAmber;
  }
  return TFT_RED;
}

void drawHistogramView() {
  if (normalizedFrame == nullptr) {
    return;
  }

  uint16_t bins[kHistogramBins] = {};
  for (size_t i = 0; i < flir::kLeptonPixels; ++i) {
    ++bins[normalizedFrame[i] >> 2];
  }

  uint16_t peak = 1;
  for (uint8_t i = 0; i < kHistogramBins; ++i) {
    peak = max<uint16_t>(peak, bins[i]);
  }

  display->fillRect(flir::kThermalViewX, flir::kThermalViewY, flir::kThermalViewWidth, flir::kThermalViewHeight, TFT_BLACK);
  display->setTextDatum(textdatum_t::top_left);
  display->setTextSize(1);
  display->setFont(&fonts::Font2);
  display->setTextColor(TFT_WHITE, TFT_BLACK);
  display->drawString("Intensity Histogram", 6, flir::kThermalViewY + 4);
  display->setTextColor(TFT_DARKGREY, TFT_BLACK);
  display->drawString("cold", 8, flir::kThermalViewY + flir::kThermalViewHeight - 18);
  display->drawRightString("hot", flir::kDisplayWidth - 8, flir::kThermalViewY + flir::kThermalViewHeight - 18);

  const uint16_t plotX = flir::kThermalViewX + 8;
  const uint16_t plotY = flir::kThermalViewY + 28;
  const uint16_t plotW = flir::kThermalViewWidth - 16;
  const uint16_t plotH = flir::kThermalViewHeight - 54;
  const uint16_t baseY = plotY + plotH;
  display->drawRect(plotX - 1, plotY - 1, plotW + 2, plotH + 2, 0x39E7);

  const uint16_t barW = max<uint16_t>(1, plotW / kHistogramBins);
  for (uint8_t i = 0; i < kHistogramBins; ++i) {
    const uint16_t barH = max<uint16_t>(1, (static_cast<uint32_t>(bins[i]) * plotH) / peak);
    const uint16_t x = plotX + static_cast<uint16_t>(i) * barW;
    display->fillRect(x, baseY - barH, barW, barH, histogramBarColor(i));
  }
}

void printTemperatureDiagnostics(uint16_t minRaw, uint16_t maxRaw) {
  const uint32_t now = millis();
  if (now - lastTemperatureDiagnosticMs < kTemperatureDiagnosticIntervalMs) {
    return;
  }
  lastTemperatureDiagnosticMs = now;

  const uint16_t centerIndex = static_cast<uint16_t>((flir::kLeptonHeight / 2) * flir::kLeptonWidth + (flir::kLeptonWidth / 2));
  const uint16_t centerRaw = rawFrame != nullptr ? rawFrame[centerIndex] : 0;
  const uint16_t clean14 = centerRaw & 0x3FFF;
  const float centerC = processor->rawCentikelvinToCelsius(centerRaw);
  const float clean14C = processor->rawCentikelvinToCelsius(clean14);
  const bool tlinearLooksValid = centerRaw >= 25000 && centerRaw <= 33000;

  char diag[320] = {};
  snprintf(
      diag,
      sizeof(diag),
      "TEMP_DIAG t=%lu cci=%u rawCenter=%u centerC=%.2f clean14=%u clean14C=%.2f rawMin=%u minC=%.2f rawMax=%u "
      "maxC=%.2f heqLow=%u heqHigh=%u span=%u tlinear=%s fps=%.1f sync=%lu timeout=%lu reject=%lu shown=%lu "
      "partialDrop=%lu missRows=%u\n",
      static_cast<unsigned long>(now),
      cciPresent ? 1 : 0,
      centerRaw,
      centerC,
      clean14,
      clean14C,
      spots.minRaw,
      spots.minC,
      spots.maxRaw,
      spots.maxC,
      minRaw,
      maxRaw,
      static_cast<uint16_t>(maxRaw - minRaw),
      tlinearLooksValid ? "OK" : "CHECK",
      measuredFps,
      static_cast<unsigned long>(syncLossCount),
      static_cast<unsigned long>(timeoutCount),
      static_cast<unsigned long>(rejectedFrameCount),
      static_cast<unsigned long>(displayedFrameCount),
      static_cast<unsigned long>(partialFrameDropCount),
      lepton != nullptr ? lepton->lastMissingRows() : 0);
  Serial.print(diag);
  Serial0.print(diag);

  if (!tlinearLooksValid) {
    const char* hint =
        "TEMP_DIAG hint: room-temperature TLinear centikelvin should be about 29300-30000 raw. If rawCenter is much "
        "lower, verify Lepton radiometry/TLinear CCI setup and VoSPI byte order.";
    Serial.println(hint);
    Serial0.println(hint);
  }
}

void renderCurrentFrame() {
  if (upscaledFrame == nullptr || renderFrame == nullptr) {
    return;
  }
  if (palette == PaletteMode::Histogram) {
    drawHistogramView();
    drawOverlay();
    ++framesRendered;
    return;
  }
  processor->applyPalette(upscaledFrame, flir::kThermalViewPixels, renderFrame, palette);
  display->pushImage(flir::kThermalViewX,
                     flir::kThermalViewY,
                     flir::kThermalViewWidth,
                     flir::kThermalViewHeight,
                     renderFrame);
  drawExtremaMarkers();
  drawTemperatureOverlay();
  const uint32_t now = millis();
  if (now - lastOverlayMs >= kOverlayIntervalMs || framesRendered == 0) {
    drawOverlay();
    lastOverlayMs = now;
  }
  ++framesRendered;
}

bool processFrame() {
  if (rawFrame == nullptr || normalizedFrame == nullptr || denoisedFrame == nullptr || upscaledFrame == nullptr) {
    return false;
  }
  uint16_t minRaw = 0;
  uint16_t maxRaw = 0;
  if (!processor->isFrameUsable(rawFrame, flir::kLeptonPixels, &minRaw, &maxRaw)) {
    ++rejectedFrameCount;
    return false;
  }
  if (!processor->histogramEqualizeRaw14(rawFrame, flir::kLeptonPixels, normalizedFrame, &minRaw, &maxRaw)) {
    return false;
  }
  memcpy(denoisedFrame, normalizedFrame, flir::kLeptonPixels);
  processor->temporalSmooth(denoisedFrame, flir::kLeptonPixels, 1);
  if (!processor->upscaleBilinear(denoisedFrame,
                                 flir::kLeptonWidth,
                                 flir::kLeptonHeight,
                                 upscaledFrame,
                                 flir::kThermalViewWidth,
                                 flir::kThermalViewHeight)) {
    return false;
  }
  if (!processor->sharpen3x3(upscaledFrame, flir::kThermalViewWidth, flir::kThermalViewHeight, 1)) {
    return false;
  }
  spots = processor->calculateSpotTemperatures(rawFrame,
                                               flir::kLeptonWidth,
                                               flir::kLeptonHeight);
  printTemperatureDiagnostics(minRaw, maxRaw);
  hasFrame = true;
  return true;
}

void handleCaptureTap() {
  if (!hasFrame) {
    setStatus("No frame yet");
    return;
  }
  if (!storage->isReady()) {
    setStatus("No SD card", 1200);
    return;
  }

  captureInProgress = true;
  drawOverlay();
  setStatus("Saving capture");
  if (!storage->saveCapture(rawFrame,
                            flir::kLeptonPixels,
                            upscaledFrame,
                            flir::kThermalViewWidth,
                            flir::kThermalViewHeight)) {
    setStatus("Capture failed");
  } else {
    setStatus("Capture saved");
  }
  captureInProgress = false;
  renderCurrentFrame();
}

void handlePauseTap() {
  paused = !paused;
  setStatus(paused ? "Paused" : "Live resumed", 900);
  drawOverlay();
}

void handleTouchPoint(uint16_t x, uint16_t y) {
  uint16_t mappedX = x;
  uint16_t mappedY = y;
  const TouchAction action = resolveTouchAction(x, y, &mappedX, &mappedY);
  Serial.printf("Touch point: raw=%u,%u mapped=%u,%u action=%u\n",
                x,
                y,
                mappedX,
                mappedY,
                static_cast<unsigned>(action));
  Serial0.printf("Touch point: raw=%u,%u mapped=%u,%u action=%u\n",
                 x,
                 y,
                 mappedX,
                 mappedY,
                 static_cast<unsigned>(action));

  switch (action) {
    case TouchAction::Capture:
      handleCaptureTap();
      break;
    case TouchAction::Pause:
      handlePauseTap();
      break;
    case TouchAction::Palette:
      cyclePalette();
      drawOverlay();
      break;
    case TouchAction::None:
    default:
      setStatus("Touch ignored", 600);
      drawOverlay();
      break;
  }
}

void handleTouchAction() {
  uint16_t x = 0;
  uint16_t y = 0;
  bool hasPoint = false;
  for (uint8_t attempt = 0; attempt < 3 && !hasPoint; ++attempt) {
    hasPoint = display->getTouch(&x, &y);
    if (!hasPoint) {
      delay(8);
    }
  }
  if (!hasPoint) {
    if (paused) {
      handlePauseTap();
    } else {
      setStatus("Touch unreadable", 600);
    }
    return;
  }

  if (paused) {
    Serial.printf("Resume touch point: x=%u y=%u\n", x, y);
    Serial0.printf("Resume touch point: x=%u y=%u\n", x, y);
    if (bottomTouchAction(x, y) == TouchAction::Pause) {
      handlePauseTap();
    } else {
      setStatus("Press RESUME", 700);
      drawOverlay();
    }
    return;
  }

  handleTouchPoint(x, y);
}

void pollTouchWhilePaused() {
  if (!paused || millis() - lastPausedTouchPollMs < kPausedTouchPollMs) {
    return;
  }
  lastPausedTouchPollMs = millis();

  uint16_t x = 0;
  uint16_t y = 0;
  bool hasPoint = false;
  for (uint8_t attempt = 0; attempt < 3 && !hasPoint; ++attempt) {
    hasPoint = display->getTouch(&x, &y);
    if (!hasPoint) {
      delay(8);
    }
  }
  if (!hasPoint) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastTouchMs < kTouchDebounceMs) {
    return;
  }
  lastTouchMs = now;
  Serial.printf("Resume poll touch point: x=%u y=%u\n", x, y);
  Serial0.printf("Resume poll touch point: x=%u y=%u\n", x, y);
  if (bottomTouchAction(x, y) == TouchAction::Pause) {
    handlePauseTap();
  }
}

void pollTouchFallback() {
  if (millis() - lastTouchPollMs < 80) {
    return;
  }
  lastTouchPollMs = millis();

  uint16_t x = 0;
  uint16_t y = 0;
  if (!display->getTouch(&x, &y)) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastTouchMs < kTouchDebounceMs) {
    return;
  }
  lastTouchMs = now;
  Serial.printf("Polled touch point: x=%u y=%u irq=%d\n", x, y, digitalRead(Pins::TOUCH_IRQ));
  Serial0.printf("Polled touch point: x=%u y=%u irq=%d\n", x, y, digitalRead(Pins::TOUCH_IRQ));

  if (paused) {
    if (bottomTouchAction(x, y) == TouchAction::Pause) {
      handlePauseTap();
    }
    return;
  }
  handleTouchPoint(x, y);
}

void drawBootStatus(const char* line1, const char* line2) {
  display->fillScreen(TFT_BLACK);
  display->setTextDatum(textdatum_t::top_left);
  display->setTextSize(1);
  display->setFont(&fonts::Font2);
  display->setTextColor(TFT_WHITE, TFT_BLACK);
  display->drawString(line1, 8, 8);
  display->drawString(line2, 8, 28);
  display->setFont(&fonts::Font0);
}

void updateFps() {
  const uint32_t now = millis();
  if (lastFpsMs == 0) {
    lastFpsMs = now;
    framesRendered = 0;
    return;
  }
  const uint32_t elapsed = now - lastFpsMs;
  if (elapsed >= 1000) {
    measuredFps = (framesRendered * 1000.0f) / elapsed;
    framesRendered = 0;
    lastFpsMs = now;
  }
}

void recordFrameError(LeptonStatus status) {
  if (status == LeptonStatus::Timeout) {
    ++timeoutCount;
  } else if (status == LeptonStatus::SyncLost) {
    ++syncLossCount;
  }

  static uint32_t lastErrorPrintMs = 0;
  if (millis() - lastErrorPrintMs > 1000) {
    lastErrorPrintMs = millis();
    char line[96] = {};
    snprintf(line,
             sizeof(line),
             "FLIR stream waiting: status=%u syncLoss=%lu timeout=%lu\n",
             static_cast<unsigned>(status),
             static_cast<unsigned long>(syncLossCount),
             static_cast<unsigned long>(timeoutCount));
    Serial.print(line);
    Serial0.print(line);
  }
}

}  // namespace

namespace FlirApp {

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(500);
  Serial.println();
  bootLog("Real FLIR app booting");

  if (!constructAppObjects()) {
    return;
  }
  Serial.flush();

  pinMode(Pins::LCD_CS, OUTPUT);
  pinMode(Pins::TOUCH_CS, OUTPUT);
  pinMode(Pins::SD_CS, OUTPUT);
  digitalWrite(Pins::LCD_CS, HIGH);
  digitalWrite(Pins::TOUCH_CS, HIGH);
  digitalWrite(Pins::SD_CS, HIGH);

  bootLog("Display init");
  display->init();
  display->setRotation(0);
  display->setBrightness(210);
  drawBootStatus("FLIR 80x60", "Initializing Lepton and SD");

  bootLog("Allocate frame buffers");
  if (!allocateFrameBuffers()) {
    drawBootStatus("Memory allocation failed", "Check PSRAM config");
    return;
  }

  pinMode(Pins::TOUCH_IRQ, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Pins::TOUCH_IRQ), onTouchIrq, FALLING);

  bootLog("CCI init");
  cciPresent = cci->begin();
  if (cciPresent) {
    if (cci->configureImageQuality()) {
      skipFramesAfterFfc = 6;
    }
  }
  bootLog("VoSPI init");
  lepton->begin();
  bootLog("SD init");
  sdReady = storage->begin();
  if (!sdReady) {
    drawBootStatus("SD mount failed", "Captures disabled; see Serial");
  }

  Serial.println("Real FLIR app ready: 9 Hz processing loop active");
  Serial.println("Serial commands: p = cycle palette, f = run FFC");
  setStatus("Waiting for FLIR");
  display->fillScreen(TFT_BLACK);
  drawOverlay();
}

void loop() {
  handleSerialCommands();

  if (consumeTouchTap()) {
    handleTouchAction();
  }
  pollTouchWhilePaused();
  pollTouchFallback();

  updateFps();

  if (paused || millis() - lastFrameMs < kFrameIntervalMs) {
    return;
  }
  lastFrameMs = millis();

  const LeptonStatus status = lepton->readFrame(rawFrame, flir::kLeptonPixels);
  if (status != LeptonStatus::Ok) {
    if (lepton->lastMissingRows() > 2) {
      ++partialFrameDropCount;
    }
    recordFrameError(status);
    return;
  }

  if (skipFramesAfterFfc > 0) {
    --skipFramesAfterFfc;
    return;
  }

  if (processFrame()) {
    ++displayedFrameCount;
    if (!hasFrame || statusLine == "Waiting for FLIR") {
      setStatus("FLIR stream live", 1200);
    }
    renderCurrentFrame();
  }
}

}  // namespace FlirApp

#endif  // FLIR_REAL_APP
