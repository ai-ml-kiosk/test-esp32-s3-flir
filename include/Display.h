#pragma once

#include <LovyanGFX.hpp>

class Display : public lgfx::LGFX_Device {
 public:
  Display();

 private:
  lgfx::Bus_SPI bus_;
  lgfx::Panel_ILI9341 panel_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;
};
