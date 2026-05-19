#include <Arduino.h>

#include <math.h>

#include "Display.h"
#include "PinConfig.h"

namespace {

Display display;

constexpr int kScreenW = 320;
constexpr int kScreenH = 240;
constexpr int kThermalW = 80;
constexpr int kThermalH = 60;
constexpr int kThermalViewX = 0;
constexpr int kThermalViewY = 0;
constexpr int kThermalViewW = 240;
constexpr int kThermalViewH = 240;
constexpr int kSideX = 240;
constexpr int kSideW = 80;

constexpr uint16_t kBackground = 0x0841;
constexpr uint16_t kSidePanel = 0x18E3;
constexpr uint16_t kDivider = 0x6B4D;
constexpr uint16_t kMuted = 0x9CF3;
constexpr uint16_t kButton = 0x31A6;
constexpr uint16_t kButtonBorder = 0x8C71;
constexpr uint16_t kGreen = 0x57EA;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kAmber = 0xFD20;

struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char* label;
};

Button thermalButton{248, 72, 64, 32, "THM"};
Button rotLeftButton{248, 124, 30, 26, "R<"};
Button rotRightButton{282, 124, 30, 26, "R>"};
Button zoomOutButton{248, 164, 30, 26, "Z-"};
Button zoomInButton{282, 164, 30, 26, "Z+"};
Button captureButton{248, 214, 30, 20, "CAP"};
Button exitButton{282, 214, 30, 20, "EXT"};

float thermalFrame[kThermalH][kThermalW];
float lowTempC = 24.1f;
float highTempC = 36.5f;
int highX = 54;
int highY = 22;
int lowX = 12;
int lowY = 12;
float fps = 0.0f;
float zoom = 1.0f;
int rotationDeg = 360;
bool thermalOverlay = false;
bool captureFlash = false;
uint32_t captureFlashUntilMs = 0;
uint32_t frameCounter = 0;
uint32_t lastFrameMs = 0;
uint32_t lastTouchMs = 0;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount) {
  const uint8_t inv = 255 - amount;
  const uint8_t ar = ((a >> 11) & 0x1F) << 3;
  const uint8_t ag = ((a >> 5) & 0x3F) << 2;
  const uint8_t ab = (a & 0x1F) << 3;
  const uint8_t br = ((b >> 11) & 0x1F) << 3;
  const uint8_t bg = ((b >> 5) & 0x3F) << 2;
  const uint8_t bb = (b & 0x1F) << 3;
  return rgb565((ar * inv + br * amount) / 255,
                (ag * inv + bg * amount) / 255,
                (ab * inv + bb * amount) / 255);
}

uint16_t thermalPalette(float value) {
  value = constrain(value, 0.0f, 1.0f);
  const uint8_t p = static_cast<uint8_t>(value * 255.0f);

  if (p < 64) {
    return rgb565(8, 12 + p, 50 + p * 2);
  }
  if (p < 128) {
    const uint8_t q = p - 64;
    return rgb565(50 + q * 3, 30 + q, 180 + q);
  }
  if (p < 192) {
    const uint8_t q = p - 128;
    return rgb565(240 + q / 4, 60 + q * 2, 40);
  }
  const uint8_t q = p - 192;
  return rgb565(255, 190 + q, 50 + q * 3);
}

void printPinSummary() {
  Serial.println();
  Serial.println("ESP32-S3 FLIR UI preview");
  Serial.println("FLIR module is not required yet; thermal data is simulated.");
  Serial.printf("LCD SCK=%d MOSI=%d MISO=%d CS=%d DC=%d RST=%d BL=%d\n",
                Pins::UI_SPI_SCLK,
                Pins::UI_SPI_MOSI,
                Pins::UI_SPI_MISO,
                Pins::LCD_CS,
                Pins::LCD_DC,
                Pins::LCD_RST,
                Pins::LCD_BACKLIGHT);
  Serial.printf("Touch T_CLK=%d T_DIN=%d T_DO=%d T_CS=%d T_IRQ=%d\n",
                Pins::UI_SPI_SCLK,
                Pins::UI_SPI_MOSI,
                Pins::UI_SPI_MISO,
                Pins::TOUCH_CS,
                Pins::TOUCH_IRQ);
  Serial.printf("Future FLIR VoSPI SCK=%d MOSI=%d MISO=%d CS=%d CCI SDA=%d SCL=%d\n",
                Pins::FLIR_SPI_SCLK,
                Pins::FLIR_SPI_MOSI,
                Pins::FLIR_SPI_MISO,
                Pins::FLIR_SPI_CS,
                Pins::FLIR_CCI_SDA,
                Pins::FLIR_CCI_SCL);
}

void generateSimulatedThermalFrame() {
  lowTempC = 99.0f;
  highTempC = -99.0f;

  const float t = frameCounter * 0.08f;
  const float cx = 40.0f + sinf(t * 0.72f) * 8.0f;
  const float cy = 30.0f + cosf(t * 0.55f) * 6.0f;
  const float hotX = 56.0f + sinf(t * 0.48f) * 5.0f;
  const float hotY = 22.0f + cosf(t * 0.60f) * 4.0f;
  const float coldX = 13.0f + cosf(t * 0.50f) * 4.0f;
  const float coldY = 14.0f + sinf(t * 0.45f) * 3.0f;

  for (int y = 0; y < kThermalH; ++y) {
    for (int x = 0; x < kThermalW; ++x) {
      const float dx = (x - cx) / 26.0f;
      const float dy = (y - cy) / 21.0f;
      const float body = expf(-(dx * dx + dy * dy)) * 11.5f;
      const float hdx = (x - hotX) / 8.0f;
      const float hdy = (y - hotY) / 7.0f;
      const float hot = expf(-(hdx * hdx + hdy * hdy)) * 5.0f;
      const float cdx = (x - coldX) / 6.0f;
      const float cdy = (y - coldY) / 6.0f;
      const float cold = expf(-(cdx * cdx + cdy * cdy)) * 5.0f;
      const float wave = sinf(x * 0.17f + t) * 0.35f + cosf(y * 0.21f - t) * 0.25f;
      const float temp = 24.0f + body + hot - cold + wave;

      thermalFrame[y][x] = temp;
      if (temp > highTempC) {
        highTempC = temp;
        highX = x;
        highY = y;
      }
      if (temp < lowTempC) {
        lowTempC = temp;
        lowX = x;
        lowY = y;
      }
    }
  }
}

void drawThermalFrame() {
  const float span = max(1.0f, highTempC - lowTempC);

  for (int y = 0; y < kThermalH; ++y) {
    for (int x = 0; x < kThermalW; ++x) {
      const float normalized = (thermalFrame[y][x] - lowTempC) / span;
      display.fillRect(kThermalViewX + x * 3,
                       kThermalViewY + y * 4,
                       3,
                       4,
                       thermalPalette(normalized));
    }
  }

  if (thermalOverlay) {
    display.fillRect(34, 36, 34, 30, blend565(0x001F, TFT_WHITE, 72));
    display.fillRect(72, 74, 52, 42, blend565(0x781F, TFT_WHITE, 54));
    display.fillRect(53, 141, 68, 62, blend565(0x001F, kAmber, 62));
    display.fillRect(151, 58, 65, 57, blend565(kRed, kAmber, 58));
    display.fillRect(160, 148, 56, 44, blend565(kRed, kAmber, 38));
  }
}

void drawMarker(int px, int py, uint16_t color) {
  display.drawCircle(px, py, 8, color);
  display.drawCircle(px, py, 9, color);
}

void drawThermalOverlayText() {
  display.setTextDatum(textdatum_t::top_left);
  display.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
  display.setTextSize(2);
  display.drawString("FLIR 80x60", 8, 9);

  const int hotScreenX = kThermalViewX + highX * 3 + 1;
  const int hotScreenY = kThermalViewY + highY * 4 + 2;
  const int coldScreenX = kThermalViewX + lowX * 3 + 1;
  const int coldScreenY = kThermalViewY + lowY * 4 + 2;
  drawMarker(coldScreenX, coldScreenY, TFT_WHITE);
  drawMarker(hotScreenX, hotScreenY, kRed);

  display.setTextSize(2);
  display.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
  display.drawString("HIGH " + String(highTempC, 1) + "C", 8, 198);
  display.drawString("LOW  " + String(lowTempC, 1) + "C", 8, 220);
}

void drawButton(const Button& button, bool active = false) {
  const uint16_t fill = active ? 0x4A89 : kButton;
  const uint16_t border = active ? kGreen : kButtonBorder;
  display.fillRect(button.x, button.y, button.w, button.h, fill);
  display.drawRect(button.x, button.y, button.w, button.h, border);
  display.setTextDatum(textdatum_t::middle_center);
  display.setTextSize(button.h < 24 ? 1 : 2);
  display.setTextColor(TFT_WHITE, fill);
  display.drawString(button.label, button.x + button.w / 2, button.y + button.h / 2);
}

void drawSidePanel() {
  display.fillRect(kSideX, 0, kSideW, kScreenH, kSidePanel);
  display.drawFastVLine(kSideX, 0, kScreenH, kDivider);

  display.setTextDatum(textdatum_t::top_left);
  display.setTextSize(2);
  display.setTextColor(TFT_WHITE, kSidePanel);
  display.drawString("THERMAL", 248, 8);
  display.setTextColor(kGreen, kSidePanel);
  display.drawString("LIVE", 248, 34);
  display.setTextColor(TFT_WHITE, kSidePanel);
  display.drawString("FPS", 248, 58);
  display.drawString(String(fps, 1), 286, 58);
  display.drawFastHLine(248, 92, 64, kDivider);

  drawButton(thermalButton, thermalOverlay);

  display.setTextSize(2);
  display.setTextColor(kMuted, kSidePanel);
  display.drawString("VIEW", 248, 104);
  display.setTextColor(TFT_WHITE, kSidePanel);
  display.drawString("Rot", 248, 146);
  display.drawString(String(rotationDeg), 290, 146);
  drawButton(rotLeftButton);
  drawButton(rotRightButton);

  display.drawString("Zoom", 248, 190);
  display.drawString(String(zoom, 1) + "x", 298, 190);
  drawButton(zoomOutButton);
  drawButton(zoomInButton);

  drawButton(captureButton, captureFlash);
  drawButton(exitButton);
}

void drawFrame() {
  generateSimulatedThermalFrame();
  drawThermalFrame();
  drawThermalOverlayText();
  drawSidePanel();

  if (captureFlash && millis() > captureFlashUntilMs) {
    captureFlash = false;
  }

  ++frameCounter;
}

bool contains(const Button& button, uint16_t x, uint16_t y) {
  return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}

void handleTouch(uint16_t x, uint16_t y) {
  const uint32_t now = millis();
  if (now - lastTouchMs < 180) {
    return;
  }
  lastTouchMs = now;

  if (contains(thermalButton, x, y)) {
    thermalOverlay = !thermalOverlay;
    Serial.printf("thermal overlay %s\n", thermalOverlay ? "on" : "off");
  } else if (contains(rotLeftButton, x, y)) {
    rotationDeg = (rotationDeg + 270) % 360;
    Serial.printf("rotation %d\n", rotationDeg);
  } else if (contains(rotRightButton, x, y)) {
    rotationDeg = (rotationDeg + 90) % 360;
    Serial.printf("rotation %d\n", rotationDeg);
  } else if (contains(zoomOutButton, x, y)) {
    zoom = max(1.0f, zoom - 0.5f);
    Serial.printf("zoom %.1f\n", zoom);
  } else if (contains(zoomInButton, x, y)) {
    zoom = min(4.0f, zoom + 0.5f);
    Serial.printf("zoom %.1f\n", zoom);
  } else if (contains(captureButton, x, y)) {
    captureFlash = true;
    captureFlashUntilMs = now + 700;
    Serial.println("capture requested; SD capture will be wired after storage support");
  } else if (contains(exitButton, x, y)) {
    Serial.println("exit pressed; staying on FLIR preview screen");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  printPinSummary();

  pinMode(Pins::SD_CS, OUTPUT);
  digitalWrite(Pins::SD_CS, HIGH);

  display.init();
  display.setRotation(1);
  display.setBrightness(210);
  display.fillScreen(kBackground);
  lastFrameMs = millis();
}

void loop() {
  const uint32_t started = millis();
  if (lastFrameMs != 0) {
    const uint32_t dt = max<uint32_t>(1, started - lastFrameMs);
    fps = (fps * 0.85f) + ((1000.0f / dt) * 0.15f);
  }
  lastFrameMs = started;

  uint16_t x = 0;
  uint16_t y = 0;
  if (display.getTouch(&x, &y)) {
    handleTouch(x, y);
  }

  drawFrame();
  delay(100);
}
