#pragma once
#include <stdint.h>
#include "driver/spi_master.h"
#include "soc/gpio_reg.h"   // GPIO_OUT_W1TC_REG / GPIO_OUT_W1TS_REG

#define CO5300_CHUNK_PX 1024
#define CO5300_X_GAP    6     // hardware pixel offset on Waveshare 1.75" panel

class CO5300 {
public:
    CO5300(int8_t cs, int8_t sclk, int8_t d0, int8_t d1, int8_t d2, int8_t d3,
           int8_t rst, uint16_t w, uint16_t h);

    bool begin(uint32_t freq = 40000000);
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void pushColors(uint16_t *colors, uint32_t len);
    void fillScreen(uint16_t color);
    void setBrightness(uint8_t level);  // 0=off, 255=max

    uint16_t width()  const { return _w; }
    uint16_t height() const { return _h; }

private:
    inline void csLow()  { REG_WRITE(GPIO_OUT_W1TC_REG, _cs_mask); }
    inline void csHigh() { REG_WRITE(GPIO_OUT_W1TS_REG, _cs_mask); }
    bool qspiReg(uint8_t reg, const void *data, size_t len);
    bool poll(spi_transaction_t *t);

    int8_t   _cs, _sclk, _d0, _d1, _d2, _d3, _rst;
    uint16_t _w, _h;
    uint32_t _cs_mask = 0;
    spi_device_handle_t   _spi    = nullptr;
    spi_transaction_ext_t _t      = {};
    uint16_t             *_dma_buf = nullptr;
};
