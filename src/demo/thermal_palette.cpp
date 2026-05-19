#include "thermal_palette.h"

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
