#include <Arduino.h>

#include "Display.h"
#include "PinConfig.h"

namespace {

Display display;

uint32_t lastTouchReportMs = 0;
uint32_t lastBlinkMs = 0;
uint16_t lastTouchX = 0;
uint16_t lastTouchY = 0;
bool hadTouch = false;
bool blinkState = false;

constexpr uint16_t BACKGROUND = TFT_BLACK;
constexpr uint16_t PANEL = 0x2104;
constexpr uint16_t GRID = 0x7BEF;

void printPinSummary() {
  Serial.println();
  Serial.println("ESP32-S3 LCD + touch test");
  Serial.println("LCD:");
  Serial.printf("  SCK=%d MOSI=%d MISO=%d CS=%d DC=%d RST=%d BL=%d\n",
                Pins::UI_SPI_SCLK,
                Pins::UI_SPI_MOSI,
                Pins::UI_SPI_MISO,
                Pins::LCD_CS,
                Pins::LCD_DC,
                Pins::LCD_RST,
                Pins::LCD_BACKLIGHT);
  Serial.println("Touch:");
  Serial.printf("  T_CLK=%d T_DIN=%d T_DO=%d T_CS=%d T_IRQ=%d\n",
                Pins::UI_SPI_SCLK,
                Pins::UI_SPI_MOSI,
                Pins::UI_SPI_MISO,
                Pins::TOUCH_CS,
                Pins::TOUCH_IRQ);
}

void drawColorBars() {
  static constexpr uint16_t colors[] = {
      TFT_RED, TFT_GREEN, TFT_BLUE, TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_WHITE};

  const int barCount = sizeof(colors) / sizeof(colors[0]);
  const int barWidth = display.width() / barCount;
  const int y = 34;
  const int h = 24;

  for (int i = 0; i < barCount; ++i) {
    const int x = i * barWidth;
    const int w = (i == barCount - 1) ? display.width() - x : barWidth;
    display.fillRect(x, y, w, h, colors[i]);
  }
  display.drawRect(0, y, display.width(), h, TFT_DARKGREY);
}

void drawTouchPad() {
  const int x0 = 8;
  const int y0 = 70;
  const int w = display.width() - 16;
  const int h = 120;

  display.fillRect(x0, y0, w, h, BACKGROUND);
  display.drawRect(x0, y0, w, h, GRID);
  display.drawLine(x0 + w / 2, y0, x0 + w / 2, y0 + h, GRID);
  display.drawLine(x0, y0 + h / 2, x0 + w, y0 + h / 2, GRID);
  display.drawLine(x0, y0, x0 + w, y0 + h, TFT_DARKGREY);
  display.drawLine(x0 + w, y0, x0, y0 + h, TFT_DARKGREY);

  display.setTextColor(TFT_DARKGREY, BACKGROUND);
  display.drawString("Touch area", x0 + 8, y0 + 8);
  display.drawString("Corners should track near edges", x0 + 8, y0 + h - 18);
}

void drawPinSummary() {
  const int y = 198;
  display.fillRect(0, y, display.width(), display.height() - y, PANEL);
  display.setTextSize(1);
  display.setTextDatum(textdatum_t::top_left);
  display.setTextColor(TFT_CYAN, PANEL);
  display.drawString("LCD", 8, y + 7);
  display.setTextColor(TFT_WHITE, PANEL);
  display.drawString("SCK12 MOSI11 MISO13 CS10 DC9 RST14 BL21", 38, y + 7);

  display.setTextColor(TFT_MAGENTA, PANEL);
  display.drawString("TOUCH", 8, y + 25);
  display.setTextColor(TFT_WHITE, PANEL);
  display.drawString("T_CLK12 T_DIN11 T_DO13 T_CS15 T_IRQ16", 58, y + 25);
}

void drawStaticScreen() {
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextDatum(textdatum_t::top_left);
  display.setTextSize(2);
  display.drawString("LCD + TOUCH TEST", 8, 8);
  display.setTextSize(1);
  display.drawString("ILI9341 display, XPT2046 touch", 190, 13);
  drawColorBars();
  drawTouchPad();
  drawPinSummary();
}

void drawTouchPoint(uint16_t x, uint16_t y) {
  if (hadTouch) {
    display.drawFastHLine(lastTouchX - 8, lastTouchY, 17, BACKGROUND);
    display.drawFastVLine(lastTouchX, lastTouchY - 8, 17, BACKGROUND);
    display.drawCircle(lastTouchX, lastTouchY, 6, BACKGROUND);
  }

  display.drawFastHLine(x - 8, y, 17, TFT_RED);
  display.drawFastVLine(x, y - 8, 17, TFT_RED);
  display.drawCircle(x, y, 6, TFT_YELLOW);

  display.fillRect(190, 8, 124, 18, BACKGROUND);
  display.setTextColor(TFT_YELLOW, BACKGROUND);
  display.drawString("x=" + String(x) + " y=" + String(y), 190, 13);

  lastTouchX = x;
  lastTouchY = y;
  hadTouch = true;
}

void drawHeartbeat() {
  const uint32_t now = millis();
  if (now - lastBlinkMs < 500) {
    return;
  }

  lastBlinkMs = now;
  blinkState = !blinkState;
  display.fillCircle(display.width() - 12, 17, 5, blinkState ? TFT_GREEN : TFT_DARKGREY);
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
  display.setBrightness(192);
  drawStaticScreen();
  Serial.println("LCD test screen drawn. Touch the panel to verify coordinates.");
}

void loop() {
  drawHeartbeat();

  uint16_t x = 0;
  uint16_t y = 0;
  const bool touched = display.getTouch(&x, &y);

  if (touched) {
    drawTouchPoint(x, y);

    const uint32_t now = millis();
    if (now - lastTouchReportMs > 100) {
      Serial.printf("touch x=%u y=%u\n", x, y);
      lastTouchReportMs = now;
    }
  }

  delay(10);
}
