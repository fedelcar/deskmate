#pragma once
#include <stdint.h>
#include "esp_lcd_panel_io.h"

// CO5300 QSPI AMOLED driver
// Uses ESP-IDF esp_lcd — no third-party display library needed.
class CO5300 {
public:
    CO5300(int8_t cs, int8_t sclk, int8_t d0, int8_t d1, int8_t d2, int8_t d3,
           int8_t rst, uint16_t w, uint16_t h);

    bool begin(uint32_t freq = 40000000);

    // Set the pixel window before pushing color data
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    // Push RGB565 pixel data (len = number of pixels)
    void pushColors(uint16_t *colors, uint32_t len);

    uint16_t width()  const { return _w; }
    uint16_t height() const { return _h; }

private:
    void sendCmd(uint8_t reg);
    void sendCmdData(uint8_t reg, const uint8_t *data, size_t len);

    int8_t  _cs, _sclk, _d0, _d1, _d2, _d3, _rst;
    uint16_t _w, _h;
    esp_lcd_panel_io_handle_t _io = nullptr;
};
