#pragma once
#include <stdint.h>
#include "driver/spi_master.h"
#include "hal/gpio_ll.h"  // for GPIO.out_w1tc / GPIO.out_w1ts

#define CO5300_CHUNK_PX 1024   // pixels per DMA burst (matches Arduino_GFX default)

class CO5300 {
public:
    CO5300(int8_t cs, int8_t sclk, int8_t d0, int8_t d1, int8_t d2, int8_t d3,
           int8_t rst, uint16_t w, uint16_t h);

    bool begin(uint32_t freq = 40000000);
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void pushColors(uint16_t *colors, uint32_t len);
    void fillScreen(uint16_t color);

    uint16_t width()  const { return _w; }
    uint16_t height() const { return _h; }

private:
    void csLow();
    void csHigh();
    void qspiReg(uint8_t reg, const void *data, size_t len);

    int8_t   _cs, _sclk, _d0, _d1, _d2, _d3, _rst;
    uint16_t _w, _h;
    uint32_t _cs_mask = 0;
    spi_device_handle_t  _spi = nullptr;
    spi_transaction_ext_t _t  = {};
    uint16_t *_dma_buf = nullptr;
};
