#include "CO5300.h"
#include <Arduino.h>
#include "driver/spi_master.h"

// CO5300 uses two distinct QSPI write instructions:
//
//   0x02  Register write (4-4-4 mode):
//         instruction 0x02 on 4 wires, 24-bit address (reg<<8) on 4 wires, data on 4 wires
//
//   0x32  Pixel data write (1-1-4 mode):
//         instruction 0x32 on 1 wire, 24-bit address 0x003C00 on 1 wire, data on 4 wires
//         CS is held LOW for the entire burst; subsequent chunks have no cmd/addr phase.
//
// CS is managed manually (spics_io_num=-1) so it can span multiple DMA transactions.

CO5300::CO5300(int8_t cs, int8_t sclk, int8_t d0, int8_t d1, int8_t d2, int8_t d3,
               int8_t rst, uint16_t w, uint16_t h)
    : _cs(cs), _sclk(sclk), _d0(d0), _d1(d1), _d2(d2), _d3(d3), _rst(rst), _w(w), _h(h) {}

void CO5300::csLow() {
    if (_cs < 32) GPIO.out_w1tc.val = _cs_mask;
    else          GPIO.out1_w1tc.val = _cs_mask;
}

void CO5300::csHigh() {
    if (_cs < 32) GPIO.out_w1ts.val = _cs_mask;
    else          GPIO.out1_w1ts.val = _cs_mask;
}

// Register write: instruction 0x02, address/data all on 4 wires (4-4-4 mode)
void CO5300::qspiReg(uint8_t reg, const void *data, size_t len) {
    _t = {};
    _t.base.flags     = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    _t.base.cmd       = 0x02;
    _t.base.addr      = (uint32_t)reg << 8;
    _t.base.tx_buffer = data;
    _t.base.length    = len * 8;

    csLow();
    spi_device_polling_start(_spi, (spi_transaction_t *)&_t, portMAX_DELAY);
    spi_device_polling_end(_spi, portMAX_DELAY);
    csHigh();
}

bool CO5300::begin(uint32_t freq) {
    _cs_mask = (1UL << (_cs % 32));

    // ── Manual CS setup ──────────────────────────────────────────────────────
    pinMode(_cs, OUTPUT);
    csHigh();

    // ── Hardware reset ────────────────────────────────────────────────────────
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH); delay(10);
        digitalWrite(_rst, LOW);  delay(20);
        digitalWrite(_rst, HIGH); delay(120);
    }

    // ── SPI bus ───────────────────────────────────────────────────────────────
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
        Serial.printf("[CO5300] bus init: %d\n", r);
        return false;
    }

    // ── SPI device — fixed 8-bit cmd + 24-bit addr, manual CS ────────────────
    spi_device_interface_config_t dev = {};
    dev.command_bits   = 8;
    dev.address_bits   = 24;
    dev.clock_speed_hz = (int)freq;
    dev.mode           = 0;
    dev.spics_io_num   = -1;      // manual CS
    dev.queue_size     = 1;
    dev.flags          = SPI_DEVICE_HALFDUPLEX;

    r = spi_bus_add_device(SPI2_HOST, &dev, &_spi);
    if (r != ESP_OK) {
        Serial.printf("[CO5300] add device: %d\n", r);
        return false;
    }

    spi_device_acquire_bus(_spi, portMAX_DELAY);

    // ── DMA pixel buffer (16-byte aligned, MALLOC_CAP_DMA) ───────────────────
    _dma_buf = (uint16_t *)heap_caps_aligned_alloc(16, CO5300_CHUNK_PX * 2, MALLOC_CAP_DMA);
    if (!_dma_buf) {
        Serial.println("[CO5300] DMA alloc failed");
        return false;
    }

    // ── Init sequence (MIPI DCS) ─────────────────────────────────────────────
    qspiReg(0x11, nullptr, 0); delay(120);   // Sleep out

    uint8_t pf = 0x55; qspiReg(0x3A, &pf, 1);  // Pixel format RGB565
    uint8_t te = 0x00; qspiReg(0x35, &te, 1);  // Tearing effect
    uint8_t wr = 0x20; qspiReg(0x53, &wr, 1);  // Write Control Display
    uint8_t br = 0xFF; qspiReg(0x51, &br, 1);  // Max brightness

    uint8_t ca[] = {0x00, 0x00, (uint8_t)((_w-1)>>8), (uint8_t)(_w-1)};
    uint8_t ra[] = {0x00, 0x00, (uint8_t)((_h-1)>>8), (uint8_t)(_h-1)};
    qspiReg(0x2A, ca, 4);
    qspiReg(0x2B, ra, 4);

    qspiReg(0x29, nullptr, 0); delay(20);    // Display on

    Serial.println("[CO5300] init OK");
    return true;
}

void CO5300::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t ca[] = {(uint8_t)(x0>>8), (uint8_t)x0, (uint8_t)(x1>>8), (uint8_t)x1};
    uint8_t ra[] = {(uint8_t)(y0>>8), (uint8_t)y0, (uint8_t)(y1>>8), (uint8_t)y1};
    qspiReg(0x2A, ca, 4);
    qspiReg(0x2B, ra, 4);
}

// Pixel write: instruction 0x32, 1-1-4 mode, CS held for entire burst.
// Pixels are byte-swapped (big-endian) before sending.
void CO5300::pushColors(uint16_t *colors, uint32_t len) {
    bool first = true;
    uint32_t offset = 0;

    csLow();
    while (offset < len) {
        uint32_t chunk = len - offset;
        if (chunk > CO5300_CHUNK_PX) chunk = CO5300_CHUNK_PX;

        // Byte-swap each pixel into DMA buffer
        for (uint32_t i = 0; i < chunk; i++) {
            _dma_buf[i] = __builtin_bswap16(colors[offset + i]);
        }

        if (first) {
            _t = {};
            _t.base.flags     = SPI_TRANS_MODE_QIO;   // cmd+addr on 1 wire, data on 4
            _t.base.cmd       = 0x32;
            _t.base.addr      = 0x003C00;
            _t.base.tx_buffer = _dma_buf;
            _t.base.length    = chunk * 16;
            first = false;
        } else {
            _t = {};
            // No cmd or addr phase for continuation chunks
            _t.base.flags     = SPI_TRANS_MODE_QIO |
                                 SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR |
                                 SPI_TRANS_VARIABLE_DUMMY;
            _t.command_bits   = 0;
            _t.address_bits   = 0;
            _t.base.tx_buffer = _dma_buf;
            _t.base.length    = chunk * 16;
        }

        spi_device_polling_start(_spi, (spi_transaction_t *)&_t, portMAX_DELAY);
        spi_device_polling_end(_spi, portMAX_DELAY);

        offset += chunk;
    }
    csHigh();
}

void CO5300::fillScreen(uint16_t color) {
    uint16_t swapped = __builtin_bswap16(color);
    for (uint32_t i = 0; i < CO5300_CHUNK_PX; i++) _dma_buf[i] = swapped;

    setWindow(0, 0, _w - 1, _h - 1);

    bool first = true;
    uint32_t remaining = (uint32_t)_w * _h;
    csLow();
    while (remaining) {
        uint32_t chunk = remaining < CO5300_CHUNK_PX ? remaining : CO5300_CHUNK_PX;

        if (first) {
            _t = {};
            _t.base.flags     = SPI_TRANS_MODE_QIO;
            _t.base.cmd       = 0x32;
            _t.base.addr      = 0x003C00;
            _t.base.tx_buffer = _dma_buf;
            _t.base.length    = chunk * 16;
            first = false;
        } else {
            _t = {};
            _t.base.flags     = SPI_TRANS_MODE_QIO |
                                 SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR |
                                 SPI_TRANS_VARIABLE_DUMMY;
            _t.command_bits   = 0;
            _t.address_bits   = 0;
            _t.base.tx_buffer = _dma_buf;
            _t.base.length    = chunk * 16;
        }

        spi_device_polling_start(_spi, (spi_transaction_t *)&_t, portMAX_DELAY);
        spi_device_polling_end(_spi, portMAX_DELAY);
        remaining -= chunk;
    }
    csHigh();
}
