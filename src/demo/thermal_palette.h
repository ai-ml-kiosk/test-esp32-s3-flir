#pragma once

#include <Arduino.h>

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount);
uint16_t thermalPalette(float value);
