#include "CO5300.h"
#include <Arduino.h>
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"

// CO5300 QSPI command encoding:
//   byte[0] = 0x02  (write instruction)
//   byte[1] = 0x00  (address high)
//   byte[2] = reg   (address low — MIPI DCS register)
//   byte[3] = 0x00  (padding)
// Encoded as a single 32-bit integer sent MSB-first.
#define CO5300_CMD(reg)  (int)(((uint32_t)0x02 << 24) | ((uint32_t)(reg) << 8))

CO5300::CO5300(int8_t cs, int8_t sclk, int8_t d0, int8_t d1, int8_t d2, int8_t d3,
               int8_t rst, uint16_t w, uint16_t h)
    : _cs(cs), _sclk(sclk), _d0(d0), _d1(d1), _d2(d2), _d3(d3), _rst(rst), _w(w), _h(h) {}

bool CO5300::begin(uint32_t freq) {
    // Hardware reset
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH); delay(10);
        digitalWrite(_rst, LOW);  delay(20);
        digitalWrite(_rst, HIGH); delay(120);
    }

    // ── QSPI bus ─────────────────────────────────────────────────────────────
    // On ESP32-S3:  mosi=D0, miso=D1, quadwp=D2, quadhd=D3
    spi_bus_config_t bus = {};
    bus.mosi_io_num   = _d0;
    bus.miso_io_num   = _d1;
    bus.sclk_io_num   = _sclk;
    bus.quadwp_io_num = _d2;
    bus.quadhd_io_num = _d3;
    bus.max_transfer_sz = _w * 80 * sizeof(uint16_t); // matches LVGL buffer
    bus.flags = SPICOMMON_BUSFLAG_QUAD;

    esp_err_t r = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) {
        Serial.printf("[CO5300] SPI bus init failed: %d\n", r);
        return false;
    }

    // ── Panel IO (QSPI, 32-bit cmd, 8-bit data) ───────────────────────────
    esp_lcd_panel_io_spi_config_t io = {};
    io.cs_gpio_num       = _cs;
    io.dc_gpio_num       = -1;  // no DC pin; cmd/data distinguished by protocol
    io.spi_mode          = 0;
    io.pclk_hz           = freq;
    io.trans_queue_depth = 10;
    io.lcd_cmd_bits      = 32;
    io.lcd_param_bits    = 8;
    io.flags.quad_mode   = true;

    r = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &_io);
    if (r != ESP_OK) {
        Serial.printf("[CO5300] panel IO init failed: %d\n", r);
        return false;
    }

    // ── Init sequence (MIPI DCS) ─────────────────────────────────────────
    sendCmd(0x11);           delay(120); // Sleep out
    uint8_t pf = 0x55;
    sendCmdData(0x3A, &pf, 1);           // Pixel format: RGB565
    uint8_t te = 0x00;
    sendCmdData(0x35, &te, 1);           // Tearing effect line ON
    uint8_t br = 0xFF;
    sendCmdData(0x51, &br, 1);           // Max brightness

    // Set full-screen window once so RAMWR knows the address range
    uint8_t ca[] = {0x00, 0x00, (uint8_t)((_w-1)>>8), (uint8_t)(_w-1)};
    uint8_t ra[] = {0x00, 0x00, (uint8_t)((_h-1)>>8), (uint8_t)(_h-1)};
    sendCmdData(0x2A, ca, 4);
    sendCmdData(0x2B, ra, 4);

    sendCmd(0x29);           delay(20);  // Display on

    return true;
}

void CO5300::sendCmd(uint8_t reg) {
    esp_lcd_panel_io_tx_param(_io, CO5300_CMD(reg), nullptr, 0);
}

void CO5300::sendCmdData(uint8_t reg, const uint8_t *data, size_t len) {
    esp_lcd_panel_io_tx_param(_io, CO5300_CMD(reg), data, len);
}

void CO5300::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t ca[] = {(uint8_t)(x0>>8), (uint8_t)x0, (uint8_t)(x1>>8), (uint8_t)x1};
    uint8_t ra[] = {(uint8_t)(y0>>8), (uint8_t)y0, (uint8_t)(y1>>8), (uint8_t)y1};
    sendCmdData(0x2A, ca, 4);
    sendCmdData(0x2B, ra, 4);
}

void CO5300::pushColors(uint16_t *colors, uint32_t len) {
    // Send RAMWR (0x2C) + pixel data in one QSPI transaction
    esp_lcd_panel_io_tx_color(_io, CO5300_CMD(0x2C), colors, len * sizeof(uint16_t));
}
