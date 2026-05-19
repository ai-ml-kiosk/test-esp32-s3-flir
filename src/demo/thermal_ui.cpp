#include "thermal_ui.h"

#include "pin_config.h"
#include "thermal_palette.h"

namespace {

constexpr int kScreenH = 240;
constexpr int kThermalViewX = 0;
constexpr int kThermalViewY = 0;
constexpr int kSideX = 240;
constexpr int kSideW = 80;
constexpr int kPanelTextX = 250;
constexpr int kPanelValueX = 312;
constexpr int kPanelRuleX = 250;
constexpr int kPanelRuleW = 62;

constexpr uint16_t kBackground = 0x0841;
constexpr uint16_t kSidePanel = 0x18E3;
constexpr uint16_t kDivider = 0x6B4D;
constexpr uint16_t kMuted = 0x9CF3;
constexpr uint16_t kButton = 0x31A6;
constexpr uint16_t kButtonBorder = 0x8C71;
constexpr uint16_t kButtonPressed = 0x4A89;
constexpr uint16_t kButtonPressedBorder = 0xB7FF;
constexpr uint16_t kGreen = 0x57EA;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kAmber = 0xFD20;

}  // namespace

void ThermalUi::begin(Display& display) {
  printPinSummary();
  display.fillScreen(kBackground);
  lastFrameMs_ = millis();
}

void ThermalUi::update(Display& display) {
  const uint32_t started = millis();
  if (lastFrameMs_ != 0) {
    const uint32_t dt = max<uint32_t>(1, started - lastFrameMs_);
    fps_ = (fps_ * 0.85f) + ((1000.0f / dt) * 0.15f);
  }
  lastFrameMs_ = started;

  drawFrame(display);
}

void ThermalUi::handleTouch(uint16_t x, uint16_t y) {
  const uint32_t now = millis();
  if (now - lastTouchMs_ < 180) {
    return;
  }
  lastTouchMs_ = now;

  if (contains(rotLeftButton_, x, y)) {
    setPressed(ButtonId::RotLeft);
    rotationDeg_ = (rotationDeg_ + 270) % 360;
    Serial.printf("rotation %d\n", rotationDeg_);
  } else if (contains(rotRightButton_, x, y)) {
    setPressed(ButtonId::RotRight);
    rotationDeg_ = (rotationDeg_ + 90) % 360;
    Serial.printf("rotation %d\n", rotationDeg_);
  } else if (contains(zoomOutButton_, x, y)) {
    setPressed(ButtonId::ZoomOut);
    zoom_ = max(1.0f, zoom_ - 0.5f);
    Serial.printf("zoom %.1f\n", zoom_);
  } else if (contains(zoomInButton_, x, y)) {
    setPressed(ButtonId::ZoomIn);
    zoom_ = min(4.0f, zoom_ + 0.5f);
    Serial.printf("zoom %.1f\n", zoom_);
  } else if (contains(captureButton_, x, y)) {
    setPressed(ButtonId::Capture);
    captureFlash_ = true;
    captureFlashUntilMs_ = now + 700;
    Serial.println("capture requested; SD capture will be wired after storage support");
  } else if (contains(exitButton_, x, y)) {
    setPressed(ButtonId::Exit);
    Serial.println("exit pressed; staying on FLIR preview screen");
  }
}

void ThermalUi::printPinSummary() const {
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

void ThermalUi::drawFrame(Display& display) {
  simulator_.nextFrame(frame_);
  drawThermalFrame(display);
  drawThermalOverlayText(display);
  drawSidePanel(display);

  if (captureFlash_ && millis() > captureFlashUntilMs_) {
    captureFlash_ = false;
  }
}

void ThermalUi::drawThermalFrame(Display& display) {
  const float span = max(1.0f, frame_.highTempC - frame_.lowTempC);

  for (int y = 0; y < kThermalFrameHeight; ++y) {
    for (int x = 0; x < kThermalFrameWidth; ++x) {
      const float normalized = (frame_.pixels[y][x] - frame_.lowTempC) / span;
      display.fillRect(kThermalViewX + x * 3,
                       kThermalViewY + y * 4,
                       3,
                       4,
                       thermalPalette(normalized));
    }
  }

  if (thermalOverlay_) {
    display.fillRect(34, 36, 34, 30, blend565(0x001F, TFT_WHITE, 72));
    display.fillRect(72, 74, 52, 42, blend565(0x781F, TFT_WHITE, 54));
    display.fillRect(53, 141, 68, 62, blend565(0x001F, kAmber, 62));
    display.fillRect(151, 58, 65, 57, blend565(kRed, kAmber, 58));
    display.fillRect(160, 148, 56, 44, blend565(kRed, kAmber, 38));
  }
}

void ThermalUi::drawThermalOverlayText(Display& display) {
  display.setTextDatum(textdatum_t::top_left);
  display.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
  display.setTextSize(2);
  // display.drawString("FLIR 80x60", 8, 9);

  const int hotScreenX = kThermalViewX + frame_.highX * 3 + 1;
  const int hotScreenY = kThermalViewY + frame_.highY * 4 + 2;
  const int coldScreenX = kThermalViewX + frame_.lowX * 3 + 1;
  const int coldScreenY = kThermalViewY + frame_.lowY * 4 + 2;
  drawMarker(display, coldScreenX, coldScreenY, TFT_WHITE);
  drawMarker(display, hotScreenX, hotScreenY, kRed);

  display.setTextSize(2);
  display.drawString("HIGH " + String(frame_.highTempC, 1) + "C", 8, 198);
  display.drawString("LOW  " + String(frame_.lowTempC, 1) + "C", 8, 220);
}

void ThermalUi::drawSidePanel(Display& display) {
  display.fillRect(kSideX, 0, kSideW, kScreenH, kSidePanel);
  display.drawFastVLine(kSideX, 0, kScreenH, kDivider);

  display.setTextDatum(textdatum_t::top_left);
  display.setTextSize(1);
  display.setFont(&fonts::Font2);
  display.setTextColor(TFT_WHITE, kSidePanel);
  display.drawString("THERMAL", kPanelTextX, 7);
  display.setFont(&fonts::Font0);
  display.setTextSize(2);
  display.setTextColor(kGreen, kSidePanel);
  display.drawString("LIVE", kPanelTextX, 31);
  drawStatusRow(display, 55, "FPS", String(fps_, 1));
  display.drawFastHLine(kPanelRuleX, 72, kPanelRuleW, kDivider);

  drawSectionLabel(display, 92, "VIEW");
  drawStatusRow(display, 101, "Rot", String(rotationDeg_));
  drawButton(display, rotLeftButton_, false, isPressed(ButtonId::RotLeft));
  drawButton(display, rotRightButton_, false, isPressed(ButtonId::RotRight));

  drawStatusRow(display, 145, "Zoom", String(zoom_, 1) + "x");
  drawButton(display, zoomOutButton_, false, isPressed(ButtonId::ZoomOut));
  drawButton(display, zoomInButton_, false, isPressed(ButtonId::ZoomIn));

  drawButton(display, captureButton_, captureFlash_, isPressed(ButtonId::Capture));
  drawButton(display, exitButton_, false, isPressed(ButtonId::Exit));
}

void ThermalUi::drawButton(Display& display, const Button& button, bool active, bool pressed) const {
  const uint16_t fill = pressed ? kButtonPressed : (active ? 0x4A89 : kButton);
  const uint16_t border = pressed ? kButtonPressedBorder : (active ? kGreen : kButtonBorder);
  display.fillRect(button.x, button.y, button.w, button.h, fill);
  display.drawRect(button.x, button.y, button.w, button.h, border);
  if (pressed) {
    display.drawRect(button.x + 1, button.y + 1, button.w - 2, button.h - 2, kButtonPressedBorder);
  }
  display.setTextDatum(textdatum_t::middle_center);
  display.setTextSize(button.h < 24 ? 1 : 2);
  display.setTextColor(TFT_WHITE, fill);
  display.drawString(button.label, button.x + button.w / 2, button.y + button.h / 2);
}

void ThermalUi::drawSectionLabel(Display& display, int16_t y, const char* label) const {
  display.setTextDatum(textdatum_t::top_left);
  display.setTextSize(1);
  display.setTextColor(kMuted, kSidePanel);
  display.drawString(label, kPanelTextX, y);
}

void ThermalUi::drawStatusRow(Display& display, int16_t y, const char* label, const String& value) const {
  display.setTextDatum(textdatum_t::top_left);
  display.setTextSize(1);
  display.setTextColor(kMuted, kSidePanel);
  display.drawString(label, kPanelTextX, y);
  display.setTextColor(TFT_WHITE, kSidePanel);
  display.drawRightString(value, kPanelValueX, y);
}

void ThermalUi::drawMarker(Display& display, int px, int py, uint16_t color) const {
  display.drawCircle(px, py, 8, color);
  display.drawCircle(px, py, 9, color);
}

void ThermalUi::setPressed(ButtonId buttonId) {
  pressedButton_ = buttonId;
  pressedUntilMs_ = millis() + 220;
}

bool ThermalUi::isPressed(ButtonId buttonId) const {
  return pressedButton_ == buttonId && millis() <= pressedUntilMs_;
}

bool ThermalUi::contains(const Button& button, uint16_t x, uint16_t y) const {
  return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}
