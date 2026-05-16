#include "CO5300.h"
#include <Arduino.h>
#include "driver/spi_master.h"

CO5300::CO5300(int8_t cs, int8_t sclk, int8_t d0, int8_t d1, int8_t d2, int8_t d3,
               int8_t rst, uint16_t w, uint16_t h)
    : _cs(cs), _sclk(sclk), _d0(d0), _d1(d1), _d2(d2), _d3(d3), _rst(rst), _w(w), _h(h) {}

bool CO5300::poll(spi_transaction_t *t) {
    esp_err_t r = spi_device_polling_start(_spi, t, portMAX_DELAY);
    if (r != ESP_OK) { Serial.printf("[CO5300] poll_start err %d\n", r); return false; }
    r = spi_device_polling_end(_spi, portMAX_DELAY);
    if (r != ESP_OK) { Serial.printf("[CO5300] poll_end err %d\n", r); return false; }
    return true;
}

// Register write: instruction 0x02, cmd+addr+data all on 4 wires (4-4-4 mode)
bool CO5300::qspiReg(uint8_t reg, const void *data, size_t len) {
    _t = {};
    _t.base.flags     = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    _t.base.cmd       = 0x02;
    _t.base.addr      = (uint32_t)reg << 8;
    _t.base.tx_buffer = data;
    _t.base.length    = len * 8;
    csLow();
    bool ok = poll((spi_transaction_t *)&_t);
    csHigh();
    return ok;
}

bool CO5300::begin(uint32_t freq) {
    _cs_mask = (1UL << _cs);

    pinMode(_cs, OUTPUT);
    csHigh();

    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH); delay(10);
        digitalWrite(_rst, LOW);  delay(20);
        digitalWrite(_rst, HIGH); delay(200);
    }

    spi_bus_config_t bus = {};
    bus.mosi_io_num   = _d0;
    bus.miso_io_num   = _d1;
    bus.sclk_io_num   = _sclk;
    bus.quadwp_io_num = _d2;
    bus.quadhd_io_num = _d3;
    bus.max_transfer_sz = CO5300_CHUNK_PX * 2 + 8;
    bus.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    esp_err_t r = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) {
        Serial.printf("[CO5300] bus init err %d\n", r); return false;
    }

    spi_device_interface_config_t dev = {};
    dev.command_bits   = 8;
    dev.address_bits   = 24;
    dev.clock_speed_hz = (int)freq;
    dev.mode           = 0;
    dev.spics_io_num   = -1;
    dev.queue_size     = 1;
    dev.flags          = SPI_DEVICE_HALFDUPLEX;

    r = spi_bus_add_device(SPI2_HOST, &dev, &_spi);
    if (r != ESP_OK) { Serial.printf("[CO5300] add device err %d\n", r); return false; }

    spi_device_acquire_bus(_spi, portMAX_DELAY);

    _dma_buf = (uint16_t *)heap_caps_aligned_alloc(16, CO5300_CHUNK_PX * 2, MALLOC_CAP_DMA);
    if (!_dma_buf) { Serial.println("[CO5300] DMA alloc failed"); return false; }

    Serial.printf("[CO5300] cs_mask=0x%08X  dma_buf=%p\n", _cs_mask, _dma_buf);

    // Waveshare init sequence (from esp_lcd_co5300 + Waveshare BSP)

    // Page 0x20: vendor AMOLED power registers
    uint8_t fe20 = 0x20; qspiReg(0xFE, &fe20, 1);
    uint8_t p19  = 0x10; qspiReg(0x19, &p19,  1);
    uint8_t p1c  = 0xA0; qspiReg(0x1C, &p1c,  1);

    // Back to page 0x00: user commands
    uint8_t fe00 = 0x00; qspiReg(0xFE, &fe00, 1);
    uint8_t c4   = 0x80; qspiReg(0xC4, &c4,   1);  // Enable QSPI pixel write mode
    uint8_t pf   = 0x55; qspiReg(0x3A, &pf,   1);  // RGB565
    uint8_t te   = 0x00; qspiReg(0x35, &te,   1);  // Tearing effect on
    uint8_t wr   = 0x20; qspiReg(0x53, &wr,   1);  // Write CTRL Display
    uint8_t br   = 0xFF; qspiReg(0x51, &br,   1);  // Normal brightness
    uint8_t hb   = 0xFF; qspiReg(0x63, &hb,   1);  // HBM brightness

    // Column/row window: x offset 6 (hardware gap), cols 6-471, rows 0-465
    uint8_t ca[] = {0x00, 0x06, 0x01, 0xD7};
    uint8_t ra[] = {0x00, 0x00, 0x01, 0xD1};
    qspiReg(0x2A, ca, 4);
    qspiReg(0x2B, ra, 4);
    delay(600);

    if (!qspiReg(0x11, nullptr, 0)) { Serial.println("[CO5300] 0x11 failed"); return false; }
    delay(600);

    if (!qspiReg(0x29, nullptr, 0)) { Serial.println("[CO5300] 0x29 failed"); return false; }
    delay(20);

    Serial.println("[CO5300] init OK");
    return true;
}

// x_gap=6: hardware pixel offset on this Waveshare panel
void CO5300::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    x0 += CO5300_X_GAP;
    x1 += CO5300_X_GAP;
    uint8_t ca[] = {(uint8_t)(x0>>8), (uint8_t)x0, (uint8_t)(x1>>8), (uint8_t)x1};
    uint8_t ra[] = {(uint8_t)(y0>>8), (uint8_t)y0, (uint8_t)(y1>>8), (uint8_t)y1};
    qspiReg(0x2A, ca, 4);
    qspiReg(0x2B, ra, 4);
}

// Pixel write: instruction 0x32, QIO mode, CS held for entire burst.
void CO5300::pushColors(uint16_t *colors, uint32_t len) {
    bool first = true;
    uint32_t offset = 0;
    csLow();
    while (offset < len) {
        uint32_t chunk = len - offset;
        if (chunk > CO5300_CHUNK_PX) chunk = CO5300_CHUNK_PX;

        for (uint32_t i = 0; i < chunk; i++)
            _dma_buf[i] = __builtin_bswap16(colors[offset + i]);

        _t = {};
        if (first) {
            _t.base.flags = SPI_TRANS_MODE_QIO;
            _t.base.cmd   = 0x32;
            _t.base.addr  = 0x003C00;
            first = false;
        } else {
            _t.base.flags   = SPI_TRANS_MODE_QIO |
                               SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR |
                               SPI_TRANS_VARIABLE_DUMMY;
            _t.command_bits = 0;
            _t.address_bits = 0;
        }
        _t.base.tx_buffer = _dma_buf;
        _t.base.length    = chunk * 16;

        poll((spi_transaction_t *)&_t);
        offset += chunk;
    }
    csHigh();
}

void CO5300::fillScreen(uint16_t color) {
    Serial.printf("[CO5300] fillScreen 0x%04X\n", color);
    uint16_t sw = __builtin_bswap16(color);
    for (uint32_t i = 0; i < CO5300_CHUNK_PX; i++) _dma_buf[i] = sw;

    setWindow(0, 0, _w - 1, _h - 1);

    bool first = true;
    uint32_t remaining = (uint32_t)_w * _h;
    uint32_t chunks = 0;
    csLow();
    while (remaining) {
        uint32_t chunk = remaining < CO5300_CHUNK_PX ? remaining : CO5300_CHUNK_PX;
        _t = {};
        if (first) {
            _t.base.flags = SPI_TRANS_MODE_QIO;
            _t.base.cmd   = 0x32;
            _t.base.addr  = 0x003C00;
            first = false;
        } else {
            _t.base.flags   = SPI_TRANS_MODE_QIO |
                               SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR |
                               SPI_TRANS_VARIABLE_DUMMY;
            _t.command_bits = 0;
            _t.address_bits = 0;
        }
        _t.base.tx_buffer = _dma_buf;
        _t.base.length    = chunk * 16;
        if (!poll((spi_transaction_t *)&_t)) break;
        remaining -= chunk;
        chunks++;
    }
    csHigh();
    Serial.printf("[CO5300] fillScreen done, %u chunks\n", chunks);
}
