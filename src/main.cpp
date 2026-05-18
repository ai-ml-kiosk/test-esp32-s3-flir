#include <Arduino.h>

#include "Display.h"
#include "PinConfig.h"

namespace {

Display display;

uint32_t lastTouchReportMs = 0;
uint16_t lastX = 0;
uint16_t lastY = 0;
bool hadTouch = false;

void drawStaticScreen() {
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextDatum(textdatum_t::top_left);
  display.setTextSize(2);
  display.drawString("ESP32-S3 FLIR", 12, 12);
  display.setTextSize(1);
  display.drawString("ILI9341 + XPT2046 bring-up", 12, 42);
  display.drawString("Touch the screen to print coordinates.", 12, 60);

  display.drawRect(10, 86, 220, 180, TFT_DARKGREY);
  display.drawLine(10, 176, 230, 176, TFT_DARKGREY);
  display.drawLine(120, 86, 120, 266, TFT_DARKGREY);

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("LCD pins", 12, 284);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("SCLK=12 MOSI=11 MISO=13", 12, 300);
  display.drawString("CS=10 DC=9 RST=14 BL=21", 12, 314);

  display.setTextColor(TFT_MAGENTA, TFT_BLACK);
  display.drawString("Touch pins", 12, 334);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("T_CLK=12 T_DIN=11 T_DO=13", 12, 350);
  display.drawString("T_CS=15 T_IRQ=16", 12, 364);
}

void drawTouchPoint(uint16_t x, uint16_t y) {
  if (hadTouch) {
    display.fillCircle(lastX, lastY, 5, TFT_BLACK);
  }

  display.fillCircle(x, y, 5, TFT_RED);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.fillRect(12, 388, 210, 20, TFT_BLACK);
  display.drawString("Touch x=" + String(x) + " y=" + String(y), 12, 388);

  lastX = x;
  lastY = y;
  hadTouch = true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("ESP32-S3 FLIR viewer bring-up");
  Serial.println("LCD: SCLK=12 MOSI=11 MISO=13 CS=10 DC=9 RST=14 BL=21");
  Serial.println("Touch: T_CLK=12 T_DIN=11 T_DO=13 T_CS=15 T_IRQ=16");

  pinMode(Pins::SD_CS, OUTPUT);
  digitalWrite(Pins::SD_CS, HIGH);

  display.init();
  display.setRotation(0);
  display.setBrightness(192);
  drawStaticScreen();
}

void loop() {
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
