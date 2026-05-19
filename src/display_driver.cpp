#include "display_driver.h"

#include "pin_config.h"

Display::Display() {
  {
    auto cfg = bus_.config();
    cfg.spi_host = SPI2_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read = 16000000;
    cfg.spi_3wire = false;
    cfg.use_lock = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk = Pins::UI_SPI_SCLK;
    cfg.pin_mosi = Pins::UI_SPI_MOSI;
    cfg.pin_miso = Pins::UI_SPI_MISO;
    cfg.pin_dc = Pins::LCD_DC;
    bus_.config(cfg);
    panel_.setBus(&bus_);
  }

  {
    auto cfg = panel_.config();
    cfg.pin_cs = Pins::LCD_CS;
    cfg.pin_rst = Pins::LCD_RST;
    cfg.pin_busy = -1;
    cfg.panel_width = 240;
    cfg.panel_height = 320;
    cfg.memory_width = 240;
    cfg.memory_height = 320;
    cfg.offset_x = 0;
    cfg.offset_y = 0;
    cfg.offset_rotation = 0;
    cfg.dummy_read_pixel = 8;
    cfg.dummy_read_bits = 1;
    cfg.readable = true;
    cfg.invert = false;
    cfg.rgb_order = false;
    cfg.dlen_16bit = false;
    cfg.bus_shared = true;
    panel_.config(cfg);
  }

  {
    auto cfg = light_.config();
    cfg.pin_bl = Pins::LCD_BACKLIGHT;
    cfg.invert = false;
    cfg.freq = 44100;
    cfg.pwm_channel = 7;
    light_.config(cfg);
    panel_.setLight(&light_);
  }

  {
    auto cfg = touch_.config();
    cfg.x_min = 300;
    cfg.x_max = 3900;
    cfg.y_min = 300;
    cfg.y_max = 3900;
    cfg.pin_int = Pins::TOUCH_IRQ;
    cfg.bus_shared = true;
    cfg.offset_rotation = 0;
    cfg.spi_host = SPI2_HOST;
    cfg.freq = 2500000;
    cfg.pin_sclk = Pins::UI_SPI_SCLK;
    cfg.pin_mosi = Pins::UI_SPI_MOSI;
    cfg.pin_miso = Pins::UI_SPI_MISO;
    cfg.pin_cs = Pins::TOUCH_CS;
    touch_.config(cfg);
    panel_.setTouch(&touch_);
  }

  setPanel(&panel_);
}
