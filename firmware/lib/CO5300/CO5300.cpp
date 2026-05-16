#include "CO5300.h"
#include <Arduino.h>
#include "driver/spi_master.h"

// CO5300 QSPI write protocol:
//   [0x02]  instruction byte   — 8 bits, single wire
//   [0x00]  addr bits 23:16    \
//   [reg ]  addr bits 15:8      > 24-bit address, quad wire
//   [0x00]  addr bits  7:0    /
//   [data…] payload            — quad wire
//
// Encoded in spi_transaction_ext_t:
//   command_bits=8, cmd=0x02
//   address_bits=24, addr=(reg<<8)   →  wire: 0x00, reg, 0x00
//   tx_buffer = payload

CO5300::CO5300(int8_t cs, int8_t sclk, int8_t d0, int8_t d1, int8_t d2, int8_t d3,
               int8_t rst, uint16_t w, uint16_t h)
    : _cs(cs), _sclk(sclk), _d0(d0), _d1(d1), _d2(d2), _d3(d3), _rst(rst), _w(w), _h(h) {}

esp_err_t CO5300::qspiWrite(uint8_t reg, const void *data, size_t len) {
    spi_transaction_ext_t t = {};
    t.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR |
                   SPI_TRANS_MODE_QIO     | SPI_TRANS_MODE_DIOQIO_ADDR;
    t.command_bits = 8;
    t.address_bits = 24;
    t.base.cmd       = 0x02;
    t.base.addr      = (uint32_t)reg << 8;  // e.g. 0x001100 for reg 0x11
    t.base.tx_buffer = data;
    t.base.length    = len * 8;
    t.base.rxlength  = 0;

    if (len <= 32) {
        return spi_device_polling_transmit(_spi, (spi_transaction_t *)&t);
    }
    return spi_device_transmit(_spi, (spi_transaction_t *)&t);
}

bool CO5300::begin(uint32_t freq) {
    // ── Hardware reset ────────────────────────────────────────────────────────
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH); delay(10);
        digitalWrite(_rst, LOW);  delay(20);
        digitalWrite(_rst, HIGH); delay(120);
    }

    // ── SPI bus (QSPI: mosi=D0, miso=D1, wp=D2, hd=D3) ──────────────────────
    spi_bus_config_t bus = {};
    bus.mosi_io_num   = _d0;
    bus.miso_io_num   = _d1;
    bus.sclk_io_num   = _sclk;
    bus.quadwp_io_num = _d2;
    bus.quadhd_io_num = _d3;
    // chunk size + 4 bytes overhead for cmd/addr phase
    bus.max_transfer_sz = CO5300_CHUNK_PX * (int)sizeof(uint16_t) + 4;

    esp_err_t r = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) {
        Serial.printf("[CO5300] bus init: %d\n", r);
        return false;
    }

    // ── SPI device ───────────────────────────────────────────────────────────
    spi_device_interface_config_t dev = {};
    dev.clock_speed_hz = (int)freq;
    dev.mode           = 0;
    dev.spics_io_num   = _cs;
    dev.queue_size     = 7;
    dev.flags          = SPI_DEVICE_HALFDUPLEX;

    r = spi_bus_add_device(SPI2_HOST, &dev, &_spi);
    if (r != ESP_OK) {
        Serial.printf("[CO5300] add device: %d\n", r);
        return false;
    }

    // ── Init sequence (MIPI DCS) ─────────────────────────────────────────────
    qspiWrite(0x11, nullptr, 0); delay(120);  // Sleep out

    uint8_t pf = 0x55;  qspiWrite(0x3A, &pf, 1);  // Pixel format: RGB565
    uint8_t te = 0x00;  qspiWrite(0x35, &te, 1);  // Tearing effect ON
    uint8_t wr = 0x20;  qspiWrite(0x53, &wr, 1);  // Write Control Display (enable brightness block)
    uint8_t br = 0xFF;  qspiWrite(0x51, &br, 1);  // Max brightness

    // Full-screen window
    uint8_t ca[] = {0x00, 0x00, (uint8_t)((_w-1)>>8), (uint8_t)(_w-1)};
    uint8_t ra[] = {0x00, 0x00, (uint8_t)((_h-1)>>8), (uint8_t)(_h-1)};
    qspiWrite(0x2A, ca, 4);
    qspiWrite(0x2B, ra, 4);

    qspiWrite(0x29, nullptr, 0); delay(20);  // Display on

    Serial.println("[CO5300] init OK");
    return true;
}

void CO5300::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t ca[] = {(uint8_t)(x0>>8), (uint8_t)x0, (uint8_t)(x1>>8), (uint8_t)x1};
    uint8_t ra[] = {(uint8_t)(y0>>8), (uint8_t)y0, (uint8_t)(y1>>8), (uint8_t)y1};
    qspiWrite(0x2A, ca, 4);
    qspiWrite(0x2B, ra, 4);
}

void CO5300::fillScreen(uint16_t color) {
    static uint16_t row[466];
    for (int i = 0; i < _w; i++) row[i] = color;
    setWindow(0, 0, _w - 1, _h - 1);
    qspiWrite(0x2C, row, _w * sizeof(uint16_t));
    for (int y = 1; y < _h; y++) {
        qspiWrite(0x3C, row, _w * sizeof(uint16_t));
    }
}

void CO5300::pushColors(uint16_t *colors, uint32_t len) {
    // Split into CO5300_CHUNK_PX-pixel chunks.
    // First chunk uses RAMWR (0x2C), subsequent ones use RAMWRC (0x3C)
    // so the display cursor continues without re-seeking to window start.
    uint32_t offset = 0;
    bool first = true;
    while (offset < len) {
        uint32_t chunk = len - offset;
        if (chunk > CO5300_CHUNK_PX) chunk = CO5300_CHUNK_PX;
        uint8_t cmd = first ? 0x2C : 0x3C;
        qspiWrite(cmd, colors + offset, chunk * sizeof(uint16_t));
        offset += chunk;
        first = false;
    }
}
